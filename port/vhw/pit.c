/* pit.c - 8253/8254 programmable interval timer, channel 0 -> IRQ0. */
#include <windows.h>
#include <string.h>
#include "vhw.h"
#include "../include/ke_port.h"

typedef struct PitChannel {
    uint32_t reload;                 /* 1..65536; a programmed zero means 65536 */
    uint8_t mode, access, bcd;
    uint8_t write_hi_next, read_hi_next;
    uint8_t null_count, gate, irq_fired;
    uint8_t count_latched, status_latched;
    uint16_t count_latch, status_latch, write_lo;
    uint64_t gate_ticks;
    uint64_t origin_ns;
    uint32_t generation;
} PitChannel;

static PitChannel ch[3];
static uint8_t port61;
static CRITICAL_SECTION pit_lock;
static HANDLE pit_thread, pit_wake;
static volatile LONG pit_stop;
static int pit_initialized;
#ifdef KE_ORACLE
volatile LONG vpit_oracle_irq_count;
uint64_t vpit_oracle_irq_times[4096];
uint32_t vpit_oracle_irq_reload[4096];
#endif

static uint64_t ticks_from_ns(uint64_t ns)
{
    return (ns / 1000000000ull) * VPIT_HZ +
           ((ns % 1000000000ull) * VPIT_HZ) / 1000000000ull;
}

static uint64_t ns_from_ticks(uint64_t ticks)
{
    uint64_t whole = ticks / VPIT_HZ;
    uint64_t part = ticks % VPIT_HZ;
    return whole * 1000000000ull +
           (part * 1000000000ull + VPIT_HZ - 1) / VPIT_HZ;
}

static uint64_t elapsed_ticks(const PitChannel *c, uint64_t now)
{
    if (c != &ch[0] && !c->gate)
        return c->gate_ticks;
    return now <= c->origin_ns ? 0 : ticks_from_ns(now - c->origin_ns);
}

static uint32_t mode_number(const PitChannel *c)
{
    return c->mode == 6 ? 2 : c->mode == 7 ? 3 : c->mode;
}

static int output_now(const PitChannel *c, uint64_t now)
{
    uint64_t ticks;
    uint32_t mode;
    if (c->null_count)
        return 0;
    mode = mode_number(c);
    if (c != &ch[0] && !c->gate && (mode == 2 || mode == 3))
        return 1;
    ticks = elapsed_ticks(c, now);
    switch (mode) {
    case 0: case 1:
        return ticks >= c->reload;
    case 2:
        return (ticks % c->reload) != c->reload - 1;
    case 3:
        return (ticks % c->reload) < (c->reload + 1) / 2;
    case 4: case 5:
        return ticks != c->reload;
    default:
        return 1;
    }
}

static uint16_t counter_now(const PitChannel *c, uint64_t now)
{
    uint64_t ticks;
    uint32_t pos, mode;
    if (c->null_count)
        return 0;
    ticks = elapsed_ticks(c, now);
    mode = mode_number(c);
    if (mode == 0 || mode == 1 || mode == 4 || mode == 5) {
        if (ticks >= c->reload)
            return 0;
        return (uint16_t)(c->reload - ticks);
    }
    pos = (uint32_t)(ticks % c->reload);
    if (mode == 2)
        return (uint16_t)(pos ? c->reload - pos : c->reload);
    if (mode == 3) {
        uint32_t high = (c->reload + 1) / 2;
        uint32_t phase = pos < high ? pos : pos - high;
        uint32_t value;
        if ((c->reload & 1) && pos < high)
            value = c->reload - (phase ? 2 * phase - 1 : 0);
        else if ((c->reload & 1) && pos >= high)
            value = c->reload - 1 - 2 * phase;
        else
            value = c->reload - 2 * phase;
        return (uint16_t)(value ? value : c->reload);
    }
    return (uint16_t)(c->reload - pos);
}

static uint64_t irq_deadline(const PitChannel *c, uint64_t now)
{
    uint64_t elapsed, target_ticks;
    uint32_t mode;
    if (c->null_count || (c != &ch[0] && !c->gate))
        return 0;
    mode = mode_number(c);
    elapsed = elapsed_ticks(c, now);
    if (mode == 0 || mode == 1 || mode == 4 || mode == 5) {
        if (c->irq_fired)
            return 0;
        target_ticks = c->reload;
    } else if (mode == 2 || mode == 3) {
        target_ticks = (elapsed / c->reload + 1) * (uint64_t)c->reload;
    } else {
        return 0;
    }
    return c->origin_ns + ns_from_ticks(target_ticks);
}

static void wake_pit_thread(void)
{
    if (pit_wake)
        SetEvent(pit_wake);
}

static void pit_reprogram(PitChannel *c, uint32_t value)
{
    c->reload = value ? value : 65536;
    c->origin_ns = vhw_clock_now_ns();
    c->gate_ticks = 0;
    c->null_count = 0;
    c->irq_fired = 0;
    c->count_latched = 0;
    c->read_hi_next = 0;
    c->generation++;
    if (c == &ch[0] && mode_number(c) == 2 && c->reload == 65536u &&
        !vcpu_interrupts_enabled())
        vhw_clock_begin_calibration(c->origin_ns);
    wake_pit_thread();
    if (c == &ch[0])
        ke_log(KE_LOG_DEBUG, "pit", "channel 0 reload %u (%.3f Hz)", c->reload,
               (double)VPIT_HZ / c->reload);
}

static uint8_t status_now(const PitChannel *c, uint64_t now)
{
    uint8_t mode = (uint8_t)mode_number(c);
    return (uint8_t)((output_now(c, now) << 7) | (c->null_count << 6) |
                     (c->access << 4) | ((mode & 7) << 1) | c->bcd);
}

static void latch_count(PitChannel *c, uint64_t now)
{
    if (!c->count_latched) {
        c->count_latch = counter_now(c, now);
        c->count_latched = 1;
        c->read_hi_next = 0;
    }
}

static uint32_t pit_in(void *ctx, uint16_t port, int size)
{
    uint64_t now;
    PitChannel *c;
    uint32_t result;
    (void)ctx; (void)size;
    if (port == 0x43)
        return 0xff;                /* reading the control port is not defined */
    EnterCriticalSection(&pit_lock);
    now = vhw_clock_now_ns();
    if (port == 0x61) {
        result = (uint32_t)((port61 & 0x0f) | (output_now(&ch[2], now) ? 0x20 : 0));
    } else {
        c = &ch[port - 0x40];
        if (c->status_latched) {
            result = c->status_latch;
            c->status_latched = 0;
        } else if (c->access == 1) {
            result = c->count_latched ? (uint8_t)c->count_latch :
                     (uint8_t)counter_now(c, now);
            c->count_latched = 0;
        } else if (c->access == 2) {
            result = c->count_latched ? (uint8_t)(c->count_latch >> 8) :
                     (uint8_t)(counter_now(c, now) >> 8);
            c->count_latched = 0;
        } else if (!c->read_hi_next) {
            uint16_t count = c->count_latched ? c->count_latch : counter_now(c, now);
            result = (uint8_t)count;
            c->read_hi_next = 1;
        } else {
            uint16_t count = c->count_latched ? c->count_latch : counter_now(c, now);
            result = (uint8_t)(count >> 8);
            c->read_hi_next = 0;
            c->count_latched = 0;
        }
    }
    LeaveCriticalSection(&pit_lock);
    return result;
}

static void control_write(uint8_t value, uint64_t now)
{
    int select = value >> 6;
    PitChannel *c;
    if (select == 3) {             /* 8254 read-back command, selected bits are active-low */
        int select_count = !(value & 0x20);
        int select_status = !(value & 0x10);
        int i;
        for (i = 0; i < 3; i++) {
            if (value & (1 << (i + 1)))
                continue;
            c = &ch[i];
            if (select_status && !c->status_latched) {
                c->status_latch = status_now(c, now);
                c->status_latched = 1;
            }
            if (select_count)
                latch_count(c, now);
        }
        return;
    }
    c = &ch[select];
    if (((value >> 4) & 3) == 0) { /* counter latch command; a pending latch is not replaced */
        latch_count(c, now);
        return;
    }
    c->access = (uint8_t)((value >> 4) & 3);
    c->mode = (uint8_t)((value >> 1) & 7);
    c->bcd = value & 1;
    c->write_hi_next = 0;
    c->read_hi_next = 0;
    c->null_count = 1;
    c->irq_fired = 0;
    c->count_latched = 0;
    c->status_latched = 0;
    c->origin_ns = now;
    c->generation++;
    wake_pit_thread();
}

static void pit_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    uint64_t now, current_ticks;
    PitChannel *c;
    uint8_t old_gate;
    (void)ctx; (void)size;
    value &= 0xff;
    EnterCriticalSection(&pit_lock);
    now = vhw_clock_now_ns();
    if (port == 0x61) {
        old_gate = port61 & 1;
        current_ticks = now <= ch[2].origin_ns ? 0 :
                        ticks_from_ns(now - ch[2].origin_ns);
        port61 = (uint8_t)value;
        ch[2].gate = (uint8_t)(port61 & 1);
        if (ch[2].gate != old_gate) {
            uint32_t mode = mode_number(&ch[2]);
            if (!ch[2].gate) {
                ch[2].gate_ticks = current_ticks;
            } else if (mode == 1 || mode == 2 || mode == 3 || mode == 5) {
                ch[2].origin_ns = now;
                ch[2].gate_ticks = 0;
            } else {
                ch[2].origin_ns = now - ns_from_ticks(ch[2].gate_ticks);
                ch[2].gate_ticks = 0;
            }
            ch[2].irq_fired = 0;
            ch[2].generation++;
        }
    } else if (port == 0x43) {
        control_write((uint8_t)value, now);
    } else {
        c = &ch[port - 0x40];
        if (c->access == 1)
            pit_reprogram(c, value);
        else if (c->access == 2)
            pit_reprogram(c, value << 8);
        else if (!c->write_hi_next) {
            c->write_lo = (uint16_t)value;
            c->write_hi_next = 1;
            c->null_count = 1;
        } else {
            c->write_hi_next = 0;
            pit_reprogram(c, c->write_lo | (value << 8));
        }
    }
    LeaveCriticalSection(&pit_lock);
}

static void wait_pit_deadline(HANDLE timer, uint64_t deadline)
{
    HANDLE waits[2] = { pit_wake, timer };
    for (;;) {
        uint64_t now = vhw_clock_now_ns();
        uint64_t remain;
        LARGE_INTEGER due;
        if (pit_stop || now >= deadline)
            return;
        remain = deadline - now;
        if (remain > 100000) {
            DWORD wait_result;
            due.QuadPart = -(LONGLONG)((remain - 50000) / 100);
            if (due.QuadPart == 0)
                due.QuadPart = -1;
            SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
            wait_result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            CancelWaitableTimer(timer);
            if (wait_result == WAIT_OBJECT_0)
                return;             /* a PIT write changed the next edge; recompute it */
        } else {
            if (WaitForSingleObject(pit_wake, 0) == WAIT_OBJECT_0)
                return;
            YieldProcessor();
        }
    }
}

static DWORD WINAPI pit_thread_main(LPVOID unused)
{
    HANDLE timer;
    (void)unused;
    timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_ALL_ACCESS);
    if (!timer)
        timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (!pit_stop) {
        uint64_t now = vhw_clock_now_ns(), deadline;
        uint32_t generation;
#ifdef KE_ORACLE
        uint32_t event_reload;
#endif
        EnterCriticalSection(&pit_lock);
        deadline = irq_deadline(&ch[0], now);
        generation = ch[0].generation;
#ifdef KE_ORACLE
        event_reload = ch[0].reload;
#endif
        LeaveCriticalSection(&pit_lock);
        if (!deadline) {
            WaitForSingleObject(pit_wake, 50);
            continue;
        }
        wait_pit_deadline(timer, deadline);
        if (pit_stop)
            break;
        now = vhw_clock_now_ns();
        if (now < deadline)
            continue;               /* the timer woke because the channel was reprogrammed */
        EnterCriticalSection(&pit_lock);
        if (generation == ch[0].generation && !ch[0].null_count) {
            uint32_t mode = mode_number(&ch[0]);
            if (mode == 0 || mode == 1 || mode == 4 || mode == 5)
                ch[0].irq_fired = 1;
            LeaveCriticalSection(&pit_lock);
#ifdef KE_ORACLE
            {
                LONG n = InterlockedIncrement(&vpit_oracle_irq_count);
                if (n > 0 && n <= (LONG)(sizeof vpit_oracle_irq_times /
                                          sizeof vpit_oracle_irq_times[0])) {
                    vpit_oracle_irq_times[n - 1] = ke_now_ns();
                    vpit_oracle_irq_reload[n - 1] = event_reload;
                }
            }
#endif
            vpic_raise_irq_at(0, deadline);
        } else {
            LeaveCriticalSection(&pit_lock);
        }
    }
    if (timer)
        CloseHandle(timer);
    return 0;
}

void vpit_init(void)
{
    int i;
    if (!pit_initialized) {
        InitializeCriticalSection(&pit_lock);
        pit_initialized = 1;
        vhw_register_ports(0x40, 0x43, pit_in, pit_out, NULL, "pit");
        vhw_register_ports(0x61, 0x61, pit_in, pit_out, NULL, "port61");
    }
    EnterCriticalSection(&pit_lock);
#ifdef KE_ORACLE
    InterlockedExchange(&vpit_oracle_irq_count, 0);
    memset(vpit_oracle_irq_times, 0, sizeof vpit_oracle_irq_times);
    memset(vpit_oracle_irq_reload, 0, sizeof vpit_oracle_irq_reload);
#endif
    port61 = 0;
    for (i = 0; i < 3; i++) {
        memset(&ch[i], 0, sizeof ch[i]);
        ch[i].reload = 65536;
        ch[i].access = 3;
        ch[i].mode = 3;
        ch[i].gate = (uint8_t)(i != 2);
        ch[i].origin_ns = vhw_clock_now_ns();
    }
    LeaveCriticalSection(&pit_lock);
    wake_pit_thread();
}

void vpit_start(void)
{
    if (!pit_initialized)
        vpit_init();
    if (pit_thread)
        return;
    if (!pit_wake)
        pit_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    InterlockedExchange(&pit_stop, 0);
    pit_thread = CreateThread(NULL, 64u << 10, pit_thread_main, NULL, 0, NULL);
}

void vpit_shutdown(void)
{
    InterlockedExchange(&pit_stop, 1);
    wake_pit_thread();
    if (pit_thread) {
        WaitForSingleObject(pit_thread, 1000);
        CloseHandle(pit_thread);
        pit_thread = NULL;
    }
}
