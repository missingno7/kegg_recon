/* pit.c - 8253/8254 programmable interval timer, channel 0 -> IRQ0.
 *
 * The counter runs at 1.193182 MHz derived from the host monotonic clock; reads (with or
 * without the latch command) return the down-counter of the current period. A timer thread
 * raises IRQ0 every reload/1193182 s (reload 0 = 65536, the BIOS 18.2 Hz default), using
 * absolute deadlines so the long-run rate is exact. Channel 2 / port 61h (speaker) are only
 * stored.
 *
 * WORK PACKAGE "timer": mode 0/2/3 differences, read-back command, verify against
 * measure_pit_channel0 (m_09f64) once translated.
 */
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

typedef struct PitChannel {
    uint32_t reload;          /* 1..65536 */
    uint8_t mode, access;     /* access: 1 lo, 2 hi, 3 lo/hi */
    uint8_t write_hi_next, read_hi_next;
    uint8_t latched;
    uint16_t latch_value;
    uint16_t write_lo;
    uint64_t origin_ns;       /* start of the current programming */
} PitChannel;

static PitChannel ch[3];
static uint8_t port61;
static CRITICAL_SECTION pit_lock;
static HANDLE pit_thread, pit_wake;
static volatile LONG pit_stop;

static uint16_t counter_now(PitChannel *c)
{
    uint64_t elapsed = ke_now_ns() - c->origin_ns;
    uint64_t ticks = elapsed * VPIT_HZ / 1000000000ull;
    uint32_t pos = (uint32_t)(ticks % c->reload);
    return (uint16_t)(c->reload - pos);
}

static uint32_t pit_in(void *ctx, uint16_t port, int size)
{
    PitChannel *c;
    uint16_t v;
    uint8_t out;
    (void)ctx; (void)size;
    if (port == 0x61)
        return port61;
    if (port == 0x43)
        return 0xff;
    c = &ch[port - 0x40];
    EnterCriticalSection(&pit_lock);
    v = c->latched ? c->latch_value : counter_now(c);
    if (c->access == 1)
        out = (uint8_t)v, c->latched = 0;
    else if (c->access == 2)
        out = (uint8_t)(v >> 8), c->latched = 0;
    else if (!c->read_hi_next)
        out = (uint8_t)v, c->read_hi_next = 1;
    else
        out = (uint8_t)(v >> 8), c->read_hi_next = 0, c->latched = 0;
    LeaveCriticalSection(&pit_lock);
    return out;
}

static void pit_reprogram(PitChannel *c, uint32_t value)
{
    c->reload = value ? value : 65536;
    c->origin_ns = ke_now_ns();
    if (c == &ch[0]) {
        ke_log(KE_LOG_DEBUG, "pit", "channel 0 reload %u (%.3f Hz)", c->reload,
               (double)VPIT_HZ / c->reload);
        if (pit_wake)
            SetEvent(pit_wake);
    }
}

static void pit_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    PitChannel *c;
    (void)ctx; (void)size;
    value &= 0xff;
    EnterCriticalSection(&pit_lock);
    if (port == 0x61) {
        port61 = (uint8_t)value;
    } else if (port == 0x43) {
        int sel = (value >> 6) & 3;
        if (sel < 3) {
            c = &ch[sel];
            if (((value >> 4) & 3) == 0) {        /* counter latch */
                c->latched = 1;
                c->latch_value = counter_now(c);
                c->read_hi_next = 0;
            } else {
                c->access = (uint8_t)((value >> 4) & 3);
                c->mode = (uint8_t)((value >> 1) & 7);
                c->write_hi_next = 0;
                c->read_hi_next = 0;
            }
        }
    } else {
        c = &ch[port - 0x40];
        if (c->access == 1)
            pit_reprogram(c, value);
        else if (c->access == 2)
            pit_reprogram(c, value << 8);
        else if (!c->write_hi_next) {
            c->write_lo = (uint16_t)value;
            c->write_hi_next = 1;
        } else {
            c->write_hi_next = 0;
            pit_reprogram(c, c->write_lo | (value << 8));
        }
    }
    LeaveCriticalSection(&pit_lock);
}

static DWORD WINAPI pit_thread_main(LPVOID unused)
{
    uint64_t next;
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    next = ke_now_ns();
    while (!pit_stop) {
        uint64_t period, now;
        EnterCriticalSection(&pit_lock);
        period = (uint64_t)ch[0].reload * 1000000000ull / VPIT_HZ;
        LeaveCriticalSection(&pit_lock);
        next += period;
        now = ke_now_ns();
        if (now > next + 100000000ull)
            next = now;                            /* fell far behind: resynchronise */
        if (next > now) {
            uint64_t wait = next - now;
            if (wait > 2000000)
                WaitForSingleObject(pit_wake, (DWORD)((wait - 1500000) / 1000000));
            now = ke_now_ns();
            if (next > now)
                ke_sleep_ns(next - now);
        }
        if (!pit_stop)
            vpic_raise_irq(0);
    }
    return 0;
}

void vpit_init(void)
{
    int i;
    InitializeCriticalSection(&pit_lock);
    for (i = 0; i < 3; i++) {
        ch[i].access = 3;
        ch[i].mode = 3;
        pit_reprogram(&ch[i], 0);
    }
    vhw_register_ports(0x40, 0x43, pit_in, pit_out, NULL, "pit");
    vhw_register_ports(0x61, 0x61, pit_in, pit_out, NULL, "port61");
}

void vpit_start(void)
{
    pit_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    pit_thread = CreateThread(NULL, 64u << 10, pit_thread_main, NULL, 0, NULL);
}

void vpit_shutdown(void)
{
    InterlockedExchange(&pit_stop, 1);
    if (pit_wake)
        SetEvent(pit_wake);
    if (pit_thread) {
        WaitForSingleObject(pit_thread, 1000);
        CloseHandle(pit_thread);
    }
}
