/* test_vhw_irq.c - acceptance test of the interrupt model (docs/port/architecture.md):
 * a PIT IRQ0 handler installed like the game does (INT 31h/0205h) must interrupt a pure
 * memory busy-wait (no vhw calls), be held off by CLI and be taken at STI. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <windows.h>
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"
int inp(int port);
int outp(int port, int value);
void wait_for_tick(short wait_flags);
extern unsigned char tmr_rec[];
extern unsigned char *timer_sync_flag_ptr;
extern short retrace_count5, ticklim;
extern int retrace_spin_count8;
extern int pit_rollover_value, timer_enabled09;
int measure_pit_channel0(void);
int set_pit_channel0_reload(int value);
void pit_channel0_interrupt(void);

static volatile LONG ticks;
static uint64_t irq_times[512];
static volatile LONG retrace_measure_stop;
extern volatile LONG vpit_oracle_irq_count;
extern uint64_t vpit_oracle_irq_times[4096];
extern uint32_t vpit_oracle_irq_reload[4096];

static uint64_t filetime_value(FILETIME ft)
{
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return value.QuadPart;
}

static uint16_t read_latched_counter0(void)
{
    uint8_t lo, hi;
    outp(0x43, 0x00);
    lo = (uint8_t)inp(0x40);
    hi = (uint8_t)inp(0x40);
    return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static int test_pit_latch_and_modes(void)
{
    uint16_t first, second, count;
    uint8_t status, first_lo, first_hi;
    int failures = 0, saw_high = 0, saw_low = 0, i;

    vpit_init();
    outp(0x43, 0x34);                /* channel 0, lo/hi, mode 2 */
    outp(0x40, 0x50);                /* 50000 clocks */
    outp(0x40, 0xc3);
    Sleep(2);

    outp(0x43, 0x00);                /* latch the down-counter */
    first_lo = (uint8_t)inp(0x40);   /* low byte starts a latched lo/hi read */
    Sleep(5);
    outp(0x43, 0x00);                /* a pending latch must not be overwritten */
    first_hi = (uint8_t)inp(0x40);   /* high byte must still belong to the first latch */
    first = (uint16_t)(first_lo | ((uint16_t)first_hi << 8));
    second = read_latched_counter0();
    if (first <= second || first - second < 1000 || first > 50000 || second == 0) {
        printf("    PIT latch order: first %u, later %u\n", first, second);
        failures++;
    }

    outp(0x43, 0xcc);                /* 8254: latch channel-0 status and count */
    status = (uint8_t)inp(0x40);     /* status is returned before the count */
    {
        uint8_t lo = (uint8_t)inp(0x40);
        uint8_t hi = (uint8_t)inp(0x40);
        count = (uint16_t)(lo | ((uint16_t)hi << 8));
    }
    if ((status & 0x3f) != 0x34 || (status & 0x40) || count == 0 || count > 50000) {
        printf("    PIT read-back: status %02X, count %u\n", status, count);
        failures++;
    }

    outp(0x43, 0x30);                /* one-shot mode 0 */
    outp(0x40, 0xe8);                /* 1000 clocks */
    outp(0x40, 0x03);
    Sleep(3);
    outp(0x43, 0xec);                /* status only, channel 0 selected */
    status = (uint8_t)inp(0x40);
    count = read_latched_counter0();
    if ((status & 0x3f) != 0x30 || (status & 0xc0) != 0x80 || count != 0) {
        printf("    PIT mode 0 terminal: status %02X, count %u\n", status, count);
        failures++;
    }

    outp(0x43, 0x36);                /* square-wave mode 3 */
    outp(0x40, 0xe8);
    outp(0x40, 0x03);
    for (i = 0; i < 20; i++) {
        outp(0x43, 0xec);
        status = (uint8_t)inp(0x40);
        saw_high |= (status & 0x80) != 0;
        saw_low |= (status & 0x80) == 0;
        Sleep(1);
    }
    count = read_latched_counter0();
    if (!saw_high || !saw_low || count == 0 || count > 1000) {
        printf("    PIT mode 3: OUT high=%d low=%d count=%u\n", saw_high, saw_low, count);
        failures++;
    }
    return failures;
}

static void timer_isr(void)
{
    LONG n = InterlockedIncrement(&ticks);
    if (n > 0 && n <= (LONG)(sizeof irq_times / sizeof irq_times[0]))
        irq_times[n - 1] = ke_now_ns();
    outp(0x20, 0x20);                       /* EOI, as the game's handlers do */
}

static DWORD WINAPI release_wait_for_tick(LPVOID ptr)
{
    ke_sleep_ns(50000000ull);
    *(volatile uint8_t *)ptr = 1;
    return 0;
}

static DWORD WINAPI stop_retrace_measure(LPVOID unused)
{
    uint64_t duration = *(const uint64_t *)unused;
    ke_sleep_ns(duration);
    InterlockedExchange(&retrace_measure_stop, 1);
    return 0;
}

static int test_wait_for_tick_cpu(void)
{
    uint8_t sync_byte = 0, saved_vector[2];
    unsigned char *saved_sync = timer_sync_flag_ptr;
    short saved_retrace = retrace_count5, saved_ticklim = ticklim;
    int saved_spin = retrace_spin_count8;
    HANDLE release_thread;
    FILETIME create_time, exit_time, kernel_before, user_before, kernel_after, user_after;
    uint64_t before, after, wall_start, wall_end;
    int failures = 0;

    ke_game_thread_adopt();
    memcpy(saved_vector, tmr_rec, sizeof saved_vector);
    tmr_rec[0] = tmr_rec[1] = 0xff;  /* select the original memory-poll path */
    timer_sync_flag_ptr = &sync_byte;
    retrace_count5 = 0;
    release_thread = CreateThread(NULL, 0, release_wait_for_tick, &sync_byte, 0, NULL);
    if (!release_thread) {
        memcpy(tmr_rec, saved_vector, sizeof saved_vector);
        timer_sync_flag_ptr = saved_sync;
        return 1;
    }
    GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_before, &user_before);
    before = filetime_value(kernel_before) + filetime_value(user_before);
    wall_start = ke_now_ns();
    wait_for_tick(0);
    wall_end = ke_now_ns();
    GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_after, &user_after);
    after = filetime_value(kernel_after) + filetime_value(user_after);
    WaitForSingleObject(release_thread, 1000);
    CloseHandle(release_thread);

    printf("    wait_for_tick poll: %d iterations, %.1f ms wall, %.1f%% game-thread CPU\n",
           retrace_spin_count8, (wall_end - wall_start) / 1e6,
           10000.0 * (after - before) / (double)(wall_end - wall_start));
    if (wall_end - wall_start < 40000000ull || !retrace_spin_count8)
        failures++;
    memcpy(tmr_rec, saved_vector, sizeof saved_vector);
    timer_sync_flag_ptr = saved_sync;
    retrace_count5 = saved_retrace;
    ticklim = saved_ticklim;
    retrace_spin_count8 = saved_spin;
    return failures;
}

static int test_async_irq(void)
{
    union REGS r;
    struct SREGS s;
    uint64_t t0, dt, min_interval = UINT64_MAX, max_interval = 0, sum_interval = 0;
    FILETIME create_time, exit_time, kernel_before, user_before, kernel_after, user_after;
    uint64_t cpu_before, cpu_after;
    LONG before, after_cli, after_sti;
    uint64_t frame_start, frame_end;
    uint64_t frame_ticks;
    uint64_t retrace_start_ns, retrace_end_ns;
    uint64_t probe_start_ns, probe_elapsed_ns;
    uint32_t measured_sample;
    double probe_expected_hz, probe_actual_hz;
    HANDLE retrace_timer;
    uint64_t retrace_measure_duration;
    uint8_t saved_game_timer_record[2];
    int game_wait_ticks = 0;
    int failures = 0;
    ke_config.irq_async = 1;
    ke_game_thread_adopt();
    vpit_init();
    InterlockedExchange(&ticks, 0);
    memset(irq_times, 0, sizeof irq_times);
    GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_before, &user_before);
    cpu_before = filetime_value(kernel_before) + filetime_value(user_before);
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
    GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_after, &user_after);
    cpu_after = filetime_value(kernel_after) + filetime_value(user_after);
    printf("    busy-wait: %ld IRQ0 in %.1f ms (%.0f Hz)\n", ticks, dt / 1e6,
           ticks * 1e9 / (double)dt);
    if (ticks < 200 || ticks * 1e9 / (double)dt < 800)
        failures++;
    for (LONG i = 1; i < ticks && i < 200; i++) {
        uint64_t interval = irq_times[i] - irq_times[i - 1];
        if (interval < min_interval) min_interval = interval;
        if (interval > max_interval) max_interval = interval;
        sum_interval += interval;
    }
    printf("    IRQ0 interval: min %.2f ms, mean %.2f ms, max %.2f ms; busy thread %.1f%% CPU\n",
           min_interval / 1e6,
           sum_interval / (double)((ticks > 1) ? ((ticks < 200) ? ticks - 1 : 199) : 1) / 1e6,
           max_interval / 1e6,
           100.0 * (cpu_after - cpu_before) / (dt / 100));

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

    outp(0x21, inp(0x21) | 1);              /* mask IRQ0 while switching to the game handler */
    measured_sample = (uint32_t)measure_pit_channel0();
    if (measured_sample < 0x2710 || measured_sample > 0x61a8) {
        printf("    timer calibration for game ISR: %u\n", measured_sample);
        failures++;
    }
    pit_rollover_value = (int)measured_sample - 0x100;
    timer_enabled09 = 0;
    retrace_count5 = 0;
    vpic_lower_irq(0);
    vpic_set_pm_vector(vpic_vector_base(0), KE_FLAT_SELECTOR,
                       (uint32_t)(uintptr_t)timer_isr);
    set_pit_channel0_reload(pit_rollover_value);
    InterlockedExchange(&vpit_oracle_irq_count, 0);
    InterlockedExchange(&ticks, 0);
    retrace_measure_duration = 1000000000ull;
    InterlockedExchange(&retrace_measure_stop, 0);
    retrace_timer = CreateThread(NULL, 0, stop_retrace_measure, &retrace_measure_duration, 0,
                                 NULL);
    if (!retrace_timer) {
        failures++;
        vhw_shutdown();
        return failures;
    }
    probe_start_ns = ke_now_ns();
    outp(0x21, inp(0x21) & ~1);
    while (!retrace_measure_stop) { }
    probe_elapsed_ns = ke_now_ns() - probe_start_ns;
    WaitForSingleObject(retrace_timer, 1000);
    CloseHandle(retrace_timer);
    outp(0x21, inp(0x21) | 1);
    vpic_lower_irq(0);
    probe_expected_hz = (double)VPIT_HZ / (uint32_t)pit_rollover_value;
    probe_actual_hz = vpit_oracle_irq_count * 1e9 / (double)probe_elapsed_ns;
    printf("    1s PIT probe: %ld PIT edges, %ld delivered IRQ0 handlers "
           "(%.2f Hz; programmed %.2f Hz)\n",
           vpit_oracle_irq_count, ticks, probe_actual_hz, probe_expected_hz);
    if (fabs(probe_actual_hz - probe_expected_hz) > probe_expected_hz * 0.025 ||
        ticks < vpit_oracle_irq_count - 1 || ticks > vpit_oracle_irq_count + 1)
        failures++;

    vpic_set_pm_vector(vpic_vector_base(0), KE_FLAT_SELECTOR,
                       (uint32_t)(uintptr_t)pit_channel0_interrupt);
    set_pit_channel0_reload(pit_rollover_value);
    InterlockedExchange(&vpit_oracle_irq_count, 0);
    memcpy(saved_game_timer_record, tmr_rec, sizeof saved_game_timer_record);
    tmr_rec[0] = tmr_rec[1] = 0xff; /* exercise wait_for_tick's IRQ0-updated memory path */
    retrace_count5 = 0;
    frame_start = vga_frame_counter();
    retrace_start_ns = ke_now_ns();
    retrace_measure_duration = 10000000000ull;
    InterlockedExchange(&retrace_measure_stop, 0);
    retrace_timer = CreateThread(NULL, 0, stop_retrace_measure, &retrace_measure_duration, 0,
                                 NULL);
    if (!retrace_timer) {
        failures++;
        vhw_shutdown();
        return failures;
    }
    outp(0x21, inp(0x21) & ~1);              /* unmask translated IRQ0 handler */
    while (!retrace_measure_stop) {
        wait_for_tick(0);
        game_wait_ticks++;
    }
    retrace_end_ns = ke_now_ns();
    WaitForSingleObject(retrace_timer, 1000);
    CloseHandle(retrace_timer);
    frame_end = vga_frame_counter();
    frame_ticks = frame_end - frame_start;
    outp(0x21, inp(0x21) | 1);
    memcpy(tmr_rec, saved_game_timer_record, sizeof saved_game_timer_record);
    printf("    game timer: %ld PIT edges, %d wait_for_tick returns, %d IRQ0 handler calls, "
           "%llu virtual retraces in %.3f s "
           "(%.3f / %.3f Hz)\n",
           vpit_oracle_irq_count, game_wait_ticks, timer_enabled09,
           (unsigned long long)frame_ticks,
           (retrace_end_ns - retrace_start_ns) / 1e9,
           game_wait_ticks * 1e9 / (double)(retrace_end_ns - retrace_start_ns),
           (frame_end - frame_start) * 1e9 / (double)(retrace_end_ns - retrace_start_ns));
    if (vpit_oracle_irq_count > 1) {
        int i, shown = vpit_oracle_irq_count < 8 ? (int)vpit_oracle_irq_count : 8;
        printf("    PIT edge periods/reloads:");
        for (i = 1; i < shown; i++)
            printf(" %.2fms/%u", (vpit_oracle_irq_times[i] - vpit_oracle_irq_times[i - 1]) / 1e6,
                   vpit_oracle_irq_reload[i]);
        printf("\n");
    }
    printf("    IRQ0 ticks per retrace: %.4f\n",
           frame_ticks ? (double)timer_enabled09 / (double)frame_ticks : 0.0);
    if (!frame_ticks || game_wait_ticks <= 0 ||
        fabs(game_wait_ticks - (double)(frame_end - frame_start)) >
            frame_ticks * 0.005 ||
        fabs((double)timer_enabled09 - (double)frame_ticks) > frame_ticks * 0.005)
        failures++;
    vhw_shutdown();
    return failures;
}

void register_vhw_irq_tests(void)
{
    oracle_register("vhw PIT modes 0/2/3, read-back and latch order", test_pit_latch_and_modes);
    oracle_register("vhw wait_for_tick spin CPU measurement", test_wait_for_tick_cpu);
    oracle_register("vhw async IRQ0 into a busy-wait, CLI/STI", test_async_irq);
}
