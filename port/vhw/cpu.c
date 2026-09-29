/* cpu.c - virtual CPU interrupt flag and the game-code/virtual-PC boundary.
 *
 * game_depth > 0 while the game thread executes inside the virtual PC (any vhw service).
 * The asynchronous IRQ thread never interrupts the game thread there (it may hold vhw
 * locks); pending interrupts are then delivered synchronously when the outermost service
 * returns (vhw_leave), which is where a real CPU would take them after the IN/OUT/INT
 * instruction completes.
 */
#include "../platform/ke_platform.h"
#include "vhw.h"
#include "../include/ke_port.h"

ke_atomic_t vcpu_if_flag = 1;      /* IF: 1 = interrupts enabled                            */
ke_atomic_t vhw_game_depth;        /* nesting of vhw services on the game thread            */
ke_atomic_t vhw_in_isr;            /* an interrupt handler is running (either thread)       */
static ke_thread_id game_tid;
static ke_thread_id irq_tid;
/* POSIX: an asynchronously delivered handler runs on the game thread itself (inside the
 * interrupt signal); it then counts as "the IRQ thread" for every IRQ-context decision. */
static volatile int async_isr_context;
ke_atomic_t vhw_cpu_poll_waiting;
static KeOnce machine_clock_once = KE_ONCE_INIT;
static KeMutex machine_clock_lock;
static uint64_t machine_clock_ns;
static uint64_t machine_wall_ns;
static int calibration_clock_active;
static ke_thread_id calibration_clock_owner;
static uint64_t calibration_clock_ns;
static ke_atomic_t if_owner_tid;
#define CALIBRATION_CLOCK_STEP_NS 5000ull
/* A deliverable IRQ0 freezes the shared PIT/VGA clock at its scheduled edge until the
 * handler starts. Host delivery delay is accumulated as clock debt and repaid at a bounded
 * rate after delivery, so the clock resumes smoothly without losing long-run wall pace. */
#define CLOCK_CATCHUP_PPM 200000ull
#define IRQ_CLOCK_MAX_STEP_NS 20000ull
static uint64_t clock_catchup_debt_ns;
static uint64_t clock_catchup_fraction;
static int irq0_hold_active;
static uint64_t irq0_hold_ns;
static uint64_t irq0_hold_wall_ns;
static int irq_clock_active;
static uint64_t irq_clock_ns;
static uint64_t irq_wall_ns;
static __thread unsigned poll_yield_count;

/* ---- lockstep mode (docs/port/lockstep.md) ---------------------------------------------
 * A single-threaded, host-time-free machine: the clock advances only by fixed costs of
 * emulated events (port I/O, memory-poll iterations, idle waits), the PIT raises IRQ0 when
 * the clock crosses its next edge (no PIT/IRQ threads), and interrupts are delivered
 * synchronously at instruction boundaries (vhw_leave, STI, poll yields). Both the port and
 * the original KE.EXE machine code (port/oracle/lockstep.c) run on it, so two runs with the
 * same input schedule see the same device timeline. */
int vhw_lockstep;
uint64_t vhw_lockstep_ns;
void (*vhw_lockstep_idle_hook)(void);

void (*vhw_lockstep_ms_hook)(void);

void vhw_lockstep_advance(uint64_t ns)
{
    uint64_t before = vhw_lockstep_ns;
    vhw_lockstep_ns += ns;
    vpit_lockstep_update();
    vsb_lockstep_update();
    if (vhw_lockstep_ms_hook && before / 1000000u != vhw_lockstep_ns / 1000000u)
        vhw_lockstep_ms_hook();
}

/* One shared machine clock feeds both the PIT and VGA raster. A calibration entered by
 * programming channel 0 in mode 2 while IF=0 gets a per-CPU clock: device-time observations
 * advance by fixed steps, so host preemption cannot change a 3DA/PIT poll. Outside calibration
 * the clock follows wall time. A deliverable IRQ0 holds time at its PIT edge until delivery;
 * its edge-time scope advances through the ISR and delivery delay is smoothed back in. */
static void machine_clock_init(void)
{
    ke_mutex_init(&machine_clock_lock);
    machine_clock_ns = ke_now_ns();
    machine_wall_ns = machine_clock_ns;
}

static void ensure_machine_clock(void)
{
    ke_once(&machine_clock_once, machine_clock_init);
}

static void machine_clock_sync_wall(uint64_t now, int smooth_catchup)
{
    if (now >= machine_wall_ns) {
        uint64_t elapsed = now - machine_wall_ns;
        uint64_t extra = 0;
        if (smooth_catchup && clock_catchup_debt_ns && elapsed) {
            uint64_t whole = (elapsed / 1000000ull) * CLOCK_CATCHUP_PPM;
            uint64_t fraction = (elapsed % 1000000ull) * CLOCK_CATCHUP_PPM +
                                clock_catchup_fraction;
            extra = whole + fraction / 1000000ull;
            clock_catchup_fraction = fraction % 1000000ull;
            if (extra >= clock_catchup_debt_ns) {
                extra = clock_catchup_debt_ns;
                clock_catchup_fraction = 0;
            }
            clock_catchup_debt_ns -= extra;
        }
        machine_clock_ns += elapsed + extra;
    }
    machine_wall_ns = now;
}

static void clock_add_catchup_debt(uint64_t ns)
{
    uint64_t max = ~(uint64_t)0;
    if (max - clock_catchup_debt_ns < ns)
        clock_catchup_debt_ns = max;
    else
        clock_catchup_debt_ns += ns;
}

/* A long host scheduling gap while the IRQ thread is in a VGA poll is not emulated CPU
 * execution. Bound each observation's advance and repay the ignored wall interval later. */
static uint64_t irq_clock_advance_to(uint64_t now)
{
    uint64_t elapsed = now > irq_wall_ns ? now - irq_wall_ns : 0;
    uint64_t advance = elapsed > IRQ_CLOCK_MAX_STEP_NS ? IRQ_CLOCK_MAX_STEP_NS : elapsed;
    if (elapsed > advance)
        clock_add_catchup_debt(elapsed - advance);
    irq_clock_ns += advance;
    irq_wall_ns = now;
    return irq_clock_ns;
}

uint64_t vhw_clock_now_ns(void)
{
    uint64_t now, result;
    ke_thread_id tid;
    if (vhw_lockstep)
        return vhw_lockstep_ns;
    now = ke_now_ns();
    tid = ke_thread_current_id();
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (irq_clock_active) {
        result = irq_clock_advance_to(now);
    } else if (irq0_hold_active) {
        result = irq0_hold_ns;
    } else if (calibration_clock_active && tid == calibration_clock_owner && !vcpu_if_flag) {
        calibration_clock_ns += CALIBRATION_CLOCK_STEP_NS;
        result = calibration_clock_ns;
    } else {
        machine_clock_sync_wall(now, 1);
        result = machine_clock_ns;
    }
    ke_mutex_unlock(&machine_clock_lock);
    return result;
}

/* Convert a virtual-clock interval to host wait time. The PIT's waitable timer must honor
 * the same temporary slew as vhw_clock_now_ns or it would deliver edges late during recovery. */
uint64_t vhw_clock_wall_delay_ns(uint64_t clock_delta_ns)
{
    uint64_t extra = 0, denominator = 1000000ull + CLOCK_CATCHUP_PPM;
    if (vhw_lockstep || !clock_delta_ns)
        return clock_delta_ns;
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (clock_catchup_debt_ns) {
        extra = (clock_delta_ns / denominator) * CLOCK_CATCHUP_PPM +
                ((clock_delta_ns % denominator) * CLOCK_CATCHUP_PPM) / denominator;
        if (extra > clock_catchup_debt_ns)
            extra = clock_catchup_debt_ns;
    }
    ke_mutex_unlock(&machine_clock_lock);
    return clock_delta_ns - extra;
}

void vhw_clock_begin_calibration(uint64_t start_ns)
{
    ke_thread_id tid = ke_thread_current_id();
    uint64_t now;
    if (vhw_lockstep)
        return;                 /* the lockstep clock is already per-event deterministic */
    now = ke_now_ns();
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (!vcpu_if_flag && tid == (ke_thread_id)ke_atomic_load(&if_owner_tid)) {
        machine_clock_sync_wall(now, 0);
        calibration_clock_ns = start_ns;
        calibration_clock_owner = tid;
        calibration_clock_active = 1;
    }
    ke_mutex_unlock(&machine_clock_lock);
}

int vhw_clock_calibration_active(void)
{
    int active;
    if (vhw_lockstep)
        return 0;
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    active = calibration_clock_active && ke_thread_current_id() == calibration_clock_owner &&
             !vcpu_if_flag;
    ke_mutex_unlock(&machine_clock_lock);
    return active;
}

static void vhw_clock_end_calibration(ke_thread_id tid)
{
    uint64_t now = ke_now_ns();
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (calibration_clock_active && calibration_clock_owner == tid) {
        machine_clock_sync_wall(now, 0);
        if (calibration_clock_ns > machine_clock_ns)
            machine_clock_ns = calibration_clock_ns;
        calibration_clock_active = 0;
        calibration_clock_owner = 0;
        machine_wall_ns = now;
    }
    ke_mutex_unlock(&machine_clock_lock);
}

void vhw_clock_irq0_pending(uint64_t edge_ns)
{
    uint64_t now;
    if (vhw_lockstep)
        return;
    now = ke_now_ns();
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (!irq0_hold_active && !irq_clock_active) {
        machine_clock_sync_wall(now, 1);
        if (edge_ns && edge_ns < machine_clock_ns) {
            clock_add_catchup_debt(machine_clock_ns - edge_ns);
            irq0_hold_ns = edge_ns;
        } else {
            irq0_hold_ns = machine_clock_ns;
        }
        irq0_hold_wall_ns = now;
        machine_clock_ns = irq0_hold_ns;
        machine_wall_ns = now;
        irq0_hold_active = 1;
    }
    ke_mutex_unlock(&machine_clock_lock);
}

int vhw_clock_irq0_held(void)
{
    int held;
    if (vhw_lockstep)
        return 0;
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    held = irq0_hold_active;
    ke_mutex_unlock(&machine_clock_lock);
    return held;
}

void vhw_clock_irq0_release(void)
{
    uint64_t now;
    int released = 0;
    if (vhw_lockstep)
        return;
    now = ke_now_ns();
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (irq0_hold_active && !irq_clock_active) {
        if (now > irq0_hold_wall_ns)
            clock_add_catchup_debt(now - irq0_hold_wall_ns);
        machine_clock_ns = irq0_hold_ns;
        machine_wall_ns = now;
        irq0_hold_active = 0;
        released = 1;
    }
    ke_mutex_unlock(&machine_clock_lock);
    if (released)
        vpit_clock_changed();
}

void vhw_clock_irq0_enter(uint64_t edge_ns)
{
    uint64_t now;
    if (vhw_lockstep)
        return;
    now = ke_now_ns();
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (irq0_hold_active && !irq_clock_active) {
        irq_clock_ns = edge_ns > irq0_hold_ns ? edge_ns : irq0_hold_ns;
        irq_wall_ns = now;
        if (now > irq0_hold_wall_ns)
            clock_add_catchup_debt(now - irq0_hold_wall_ns);
        irq_clock_active = 1;
    }
    ke_mutex_unlock(&machine_clock_lock);
}

void vhw_clock_irq0_leave(void)
{
    uint64_t now;
    if (vhw_lockstep)
        return;
    now = ke_now_ns();
    ensure_machine_clock();
    ke_mutex_lock(&machine_clock_lock);
    if (irq_clock_active) {
        machine_clock_ns = irq_clock_advance_to(now);
        machine_wall_ns = now;
        irq0_hold_ns = machine_clock_ns;
        irq0_hold_wall_ns = now;
        irq_clock_active = 0;
    }
    ke_mutex_unlock(&machine_clock_lock);
}

void vhw_bind_game_thread(void) { game_tid = ke_thread_current_id(); }
void vhw_bind_irq_thread(void) { irq_tid = ke_thread_current_id(); }
int vhw_on_irq_thread(void)
{
    return (irq_tid && ke_thread_current_id() == irq_tid) || async_isr_context;
}
void vhw_set_async_isr_context(int active) { async_isr_context = active; }
int vhw_in_async_isr_context(void) { return async_isr_context; }
int vcpu_interrupts_enabled(void) { return vcpu_if_flag != 0; }

void vhw_enter(void)
{
    if (ke_thread_current_id() == game_tid)
        ke_atomic_increment(&vhw_game_depth);
}

void vhw_leave(void)
{
    if (ke_thread_current_id() != game_tid)
        return;
    if (vhw_game_depth == 1 && !vhw_in_isr) {
        if (vcpu_if_flag && vpic_has_deliverable())
            vpic_deliver_pending();
        if (ke_quit_requested())
            ke_check_quit();
        ke_check_pause();
    }
    ke_atomic_decrement(&vhw_game_depth);
}

/* Forget service/ISR nesting abandoned by ke_exit()'s longjmp. */
void vhw_reset_nesting(void)
{
    ke_atomic_exchange(&vhw_game_depth, 0);
    ke_atomic_exchange(&vhw_in_isr, 0);
    async_isr_context = 0;
}

void vcpu_cli(void)
{
    ke_thread_id tid = ke_thread_current_id();
    ke_atomic_exchange(&if_owner_tid, (ke_atomic_value)tid);
    ke_atomic_exchange(&vcpu_if_flag, 0);
    vhw_clock_irq0_release();
}

void vcpu_sti(void)
{
    ke_thread_id tid = ke_thread_current_id();
    vhw_clock_end_calibration(tid);
    ke_atomic_exchange(&if_owner_tid, 0);
    ke_atomic_exchange(&vcpu_if_flag, 1);
}

/* The original wait_for_tick path polls a memory flag without entering the vhw. */
void vhw_cpu_poll_yield(void)
{
    if (vhw_lockstep) {
        /* One loop iteration of a pure memory poll: jump to the next device event (at
         * least VHW_LOCKSTEP_POLL_NS), then take pending interrupts at this boundary. */
        uint64_t next = vpit_lockstep_next_edge();
        uint64_t step = next > vhw_lockstep_ns ? next - vhw_lockstep_ns : 0;
        if (step < VHW_LOCKSTEP_POLL_NS || step > VHW_LOCKSTEP_POLL_MAX_NS)
            step = step < VHW_LOCKSTEP_POLL_NS ? VHW_LOCKSTEP_POLL_NS : VHW_LOCKSTEP_POLL_MAX_NS;
        vhw_lockstep_advance(step);
        if (vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
            vpic_deliver_pending();
        return;
    }
    if (ke_thread_current_id() == game_tid && !(++poll_yield_count & 0x0fffu)) {
        if (ke_quit_requested())
            ke_check_quit();
        ke_check_pause();
        /* This is a safe game-code boundary: the PIC may run an IRQ handler while this
         * host wait is in progress. Device edges keep their original scheduled timestamps. */
        ke_atomic_exchange(&vhw_cpu_poll_waiting, 1);
        ke_poll_wait_100us();       /* wait 100 us once per 4096 polls */
        ke_atomic_exchange(&vhw_cpu_poll_waiting, 0);
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
    if (vhw_lockstep) {
        if (vhw_lockstep_idle_hook)
            vhw_lockstep_idle_hook();
        vhw_lockstep_advance(max_ns);
        if (vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
            vpic_deliver_pending();
        return;
    }
    if (vhw_on_irq_thread()) {
        ke_sleep_ns(max_ns);
        return;
    }
    if (vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
        vpic_deliver_pending();
    if (ke_quit_requested() && !vhw_in_isr)
        ke_check_quit();
    if (!vhw_in_isr)
        ke_check_pause();
    ke_sleep_ns(max_ns);
}
