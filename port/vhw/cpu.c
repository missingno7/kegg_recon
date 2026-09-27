/* cpu.c - virtual CPU interrupt flag and the game-code/virtual-PC boundary.
 *
 * game_depth > 0 while the game thread executes inside the virtual PC (any vhw service).
 * The asynchronous IRQ thread never interrupts the game thread there (it may hold vhw
 * locks); pending interrupts are then delivered synchronously when the outermost service
 * returns (vhw_leave), which is where a real CPU would take them after the IN/OUT/INT
 * instruction completes.
 */
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

volatile LONG vcpu_if_flag = 1;      /* IF: 1 = interrupts enabled                            */
volatile LONG vhw_game_depth;        /* nesting of vhw services on the game thread            */
volatile LONG vhw_in_isr;            /* an interrupt handler is running (either thread)       */
static DWORD game_tid;
static DWORD irq_tid;
static __thread HANDLE poll_timer;
volatile LONG vhw_cpu_poll_waiting;
static INIT_ONCE machine_clock_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION machine_clock_lock;
static uint64_t machine_clock_ns;
static uint64_t machine_wall_ns;
static int calibration_clock_active;
static DWORD calibration_clock_owner;
static uint64_t calibration_clock_ns;
static volatile LONG if_owner_tid;
#define CALIBRATION_CLOCK_STEP_NS 5000ull
static __thread uint64_t irq_clock_ns;
static __thread uint64_t irq_wall_ns;
static __thread int irq_clock_active;
static __thread unsigned poll_yield_count;

/* One monotonic machine clock feeds both the PIT and VGA raster. A calibration entered by
 * programming channel 0 in mode 2 while IF=0 gets a per-CPU clock: device-time observations
 * advance by fixed steps, so host preemption cannot change a 3DA/PIT poll. Outside calibration
 * the clock follows wall time. IRQ0 retains its edge-time scope. */
static BOOL CALLBACK machine_clock_init(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&machine_clock_lock);
    machine_clock_ns = ke_now_ns();
    machine_wall_ns = machine_clock_ns;
    return TRUE;
}

static void ensure_machine_clock(void)
{
    InitOnceExecuteOnce(&machine_clock_once, machine_clock_init, NULL, NULL);
}

static void machine_clock_sync_wall(uint64_t now)
{
    if (now >= machine_wall_ns)
        machine_clock_ns += now - machine_wall_ns;
    machine_wall_ns = now;
}

uint64_t vhw_clock_now_ns(void)
{
    uint64_t now = ke_now_ns(), result;
    DWORD tid = GetCurrentThreadId();
    if (irq_clock_active)
        return now >= irq_wall_ns ? irq_clock_ns + (now - irq_wall_ns) : irq_clock_ns;

    ensure_machine_clock();
    EnterCriticalSection(&machine_clock_lock);
    if (calibration_clock_active && tid == calibration_clock_owner && !vcpu_if_flag) {
        calibration_clock_ns += CALIBRATION_CLOCK_STEP_NS;
        result = calibration_clock_ns;
    } else {
        machine_clock_sync_wall(now);
        result = machine_clock_ns;
    }
    LeaveCriticalSection(&machine_clock_lock);
    return result;
}

void vhw_clock_begin_calibration(uint64_t start_ns)
{
    DWORD tid = GetCurrentThreadId();
    uint64_t now = ke_now_ns();
    ensure_machine_clock();
    EnterCriticalSection(&machine_clock_lock);
    if (!vcpu_if_flag && tid == (DWORD)InterlockedCompareExchange(&if_owner_tid, 0, 0)) {
        machine_clock_sync_wall(now);
        calibration_clock_ns = start_ns;
        calibration_clock_owner = tid;
        calibration_clock_active = 1;
    }
    LeaveCriticalSection(&machine_clock_lock);
}

int vhw_clock_calibration_active(void)
{
    int active;
    ensure_machine_clock();
    EnterCriticalSection(&machine_clock_lock);
    active = calibration_clock_active && GetCurrentThreadId() == calibration_clock_owner &&
             !vcpu_if_flag;
    LeaveCriticalSection(&machine_clock_lock);
    return active;
}

static void vhw_clock_end_calibration(DWORD tid)
{
    uint64_t now = ke_now_ns();
    ensure_machine_clock();
    EnterCriticalSection(&machine_clock_lock);
    if (calibration_clock_active && calibration_clock_owner == tid) {
        machine_clock_sync_wall(now);
        if (calibration_clock_ns > machine_clock_ns)
            machine_clock_ns = calibration_clock_ns;
        calibration_clock_active = 0;
        calibration_clock_owner = 0;
        machine_wall_ns = now;
    }
    LeaveCriticalSection(&machine_clock_lock);
}

void vhw_clock_irq0_enter(uint64_t edge_ns)
{
    irq_clock_ns = edge_ns;
    irq_wall_ns = ke_now_ns();
    irq_clock_active = 1;
}

void vhw_clock_irq0_leave(void)
{
    irq_clock_active = 0;
}

void vhw_bind_game_thread(void) { game_tid = GetCurrentThreadId(); }
void vhw_bind_irq_thread(void) { irq_tid = GetCurrentThreadId(); }
int vhw_on_irq_thread(void) { return irq_tid && GetCurrentThreadId() == irq_tid; }
int vcpu_interrupts_enabled(void) { return vcpu_if_flag != 0; }

void vhw_enter(void)
{
    if (GetCurrentThreadId() == game_tid)
        InterlockedIncrement(&vhw_game_depth);
}

void vhw_leave(void)
{
    if (GetCurrentThreadId() != game_tid)
        return;
    if (vhw_game_depth == 1 && !vhw_in_isr) {
        if (vcpu_if_flag && vpic_has_deliverable())
            vpic_deliver_pending();
        if (ke_quit_requested())
            ke_check_quit();
    }
    InterlockedDecrement(&vhw_game_depth);
}

/* Forget service/ISR nesting abandoned by ke_exit()'s longjmp. */
void vhw_reset_nesting(void)
{
    InterlockedExchange(&vhw_game_depth, 0);
    InterlockedExchange(&vhw_in_isr, 0);
}

void vcpu_cli(void)
{
    DWORD tid = GetCurrentThreadId();
    InterlockedExchange(&if_owner_tid, (LONG)tid);
    InterlockedExchange(&vcpu_if_flag, 0);
}

void vcpu_sti(void)
{
    DWORD tid = GetCurrentThreadId();
    vhw_clock_end_calibration(tid);
    InterlockedExchange(&if_owner_tid, 0);
    InterlockedExchange(&vcpu_if_flag, 1);
}

/* The original wait_for_tick path polls a memory flag without entering the vhw. */
void vhw_cpu_poll_yield(void)
{
    if (GetCurrentThreadId() == game_tid && !(++poll_yield_count & 0x0fffu)) {
        LARGE_INTEGER due;
        if (ke_quit_requested())
            ke_check_quit();
        /* This is a safe game-code boundary: the PIC may run an IRQ handler while this
         * host wait is in progress. Device edges keep their original scheduled timestamps. */
        if (!poll_timer)
            poll_timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                TIMER_ALL_ACCESS);
        InterlockedExchange(&vhw_cpu_poll_waiting, 1);
        due.QuadPart = -1000;           /* wait 100 us once per 4096 polls */
        if (poll_timer && SetWaitableTimer(poll_timer, &due, 0, NULL, NULL, FALSE))
            WaitForSingleObject(poll_timer, INFINITE);
        else
            Sleep(1);                   /* bounded safe-point wait if high-res timers fail */
        InterlockedExchange(&vhw_cpu_poll_waiting, 0);
    }
}

/* Watcom clib _disable()/_enable() (CLI/STI). */
void _disable(void)
{
    vhw_enter();
    vcpu_cli();
    vhw_leave();
}

void _enable(void)
{
    vhw_enter();
    vcpu_sti();
    vhw_leave(); /* STI: pending interrupts are taken here */
}

/* Called by blocking BIOS services on the game thread: deliver interrupts, sleep a little. */
void vhw_idle(uint64_t max_ns)
{
    if (vhw_on_irq_thread()) {
        ke_sleep_ns(max_ns);
        return;
    }
    if (vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
        vpic_deliver_pending();
    if (ke_quit_requested() && !vhw_in_isr)
        ke_check_quit();
    ke_sleep_ns(max_ns);
}
