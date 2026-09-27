/* test_vhw_irq.c - acceptance test of the interrupt model (docs/port/architecture.md):
 * a PIT IRQ0 handler installed like the game does (INT 31h/0205h) must interrupt a pure
 * memory busy-wait (no vhw calls), be held off by CLI and be taken at STI. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"
int inp(int port);
int outp(int port, int value);

static volatile LONG ticks;

static void timer_isr(void)
{
    InterlockedIncrement(&ticks);
    outp(0x20, 0x20);                       /* EOI, as the game's handlers do */
}

static int test_async_irq(void)
{
    union REGS r;
    struct SREGS s;
    uint64_t t0, dt;
    LONG before, after_cli, after_sti;
    int failures = 0;
    ke_config.irq_async = 1;
    ke_game_thread_adopt();
    vpit_init();
    memset(&s, 0, sizeof s);
    r.x.eax = 0x0205;                       /* DPMI set PM vector 08h */
    r.h.bl = (uint8_t)vpic_vector_base(0);
    r.w.cx = KE_FLAT_SELECTOR;
    r.x.edx = (uint32_t)(uintptr_t)timer_isr;
    int386x(0x31, &r, &r, &s);
    outp(0x21, inp(0x21) & ~1);             /* unmask IRQ0 */
    outp(0x43, 0x34);                       /* channel 0, lo/hi, mode 2 */
    outp(0x40, 1193 & 0xff);                /* 1000 Hz */
    outp(0x40, 1193 >> 8);
    vhw_start_devices();

    t0 = ke_now_ns();
    while (ticks < 200 && ke_now_ns() - t0 < 2000000000ull) {
        /* pure memory busy-wait: only asynchronous delivery can end it */
    }
    dt = ke_now_ns() - t0;
    printf("    busy-wait: %ld IRQ0 in %.1f ms (%.0f Hz)\n", ticks, dt / 1e6,
           ticks * 1e9 / (double)dt);
    if (ticks < 200 || ticks * 1e9 / (double)dt < 800)
        failures++;

    _disable();
    before = ticks;
    t0 = ke_now_ns();
    while (ke_now_ns() - t0 < 50000000ull) {
    }
    after_cli = ticks;
    _enable();                              /* STI: the pending IRQ0 is taken here */
    after_sti = ticks;
    printf("    CLI 50 ms: %ld delivered while IF=0, %ld at STI\n", after_cli - before,
           after_sti - after_cli);
    if (after_cli != before || after_sti == after_cli)
        failures++;

    outp(0x21, inp(0x21) | 1);              /* mask IRQ0 again */
    vhw_shutdown();
    return failures;
}

void register_vhw_irq_tests(void)
{
    oracle_register("vhw async IRQ0 into a busy-wait, CLI/STI", test_async_irq);
}
