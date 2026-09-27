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
static __thread uint64_t irq_clock_ns;
static __thread uint64_t irq_wall_ns;
static __thread int irq_clock_active;
static __thread unsigned poll_yield_count;

/* IRQ0 is entered from a host thread after the device edge. Start VHW time at that edge,
 * then let it advance with ISR execution; the scheduler delay before dispatch is excluded. */
uint64_t vhw_clock_now_ns(void)
{
    uint64_t now = ke_now_ns();
    return irq_clock_active && now >= irq_wall_ns ? irq_clock_ns + (now - irq_wall_ns) : now;
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

void vcpu_cli(void) { InterlockedExchange(&vcpu_if_flag, 0); }
void vcpu_sti(void) { InterlockedExchange(&vcpu_if_flag, 1); }

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
