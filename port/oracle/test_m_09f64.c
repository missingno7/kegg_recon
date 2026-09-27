/* test_m_09f64.c - differential checks for the PIT assembly translation. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

int measure_pit_channel0(void);
int set_pit_channel0_reload(int value);
extern uint32_t g_pit_elapsed_ticks;
extern short windows_environment_detected;
extern short timer_ok;
int verify_timer(void);

#define TRACE_CAPACITY 8192
static OracleEvent original_trace[TRACE_CAPACITY];
static uint32_t rng_state = 0x4d595df4u;

static uint32_t next_random(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static int traces_match(const OracleEvent *a, int an, const OracleEvent *b, int bn)
{
    int ai = 0, bi = 0, index = 0;
    for (;;) {
        while (ai < an && a[ai].kind == 'I' && a[ai].port == 0x3da) ai++;
        while (bi < bn && b[bi].kind == 'I' && b[bi].port == 0x3da) bi++;
        if (ai == an || bi == bn)
            break;
        if (a[ai].kind != b[bi].kind || a[ai].size != b[bi].size ||
            a[ai].port != b[bi].port ||
            ((a[ai].kind != 'I' || a[ai].port != 0x40) && a[ai].value != b[bi].value)) {
            printf("    trace[%d]: original %c/%u/%04X/%08X, port %c/%u/%04X/%08X\n", index,
                   a[ai].kind, a[ai].size, a[ai].port, a[ai].value, b[bi].kind, b[bi].size,
                   b[bi].port, b[bi].value);
            return 0;
        }
        ai++;
        bi++;
        index++;
    }
    if (ai != an || bi != bn) {
        printf("    trace length: original %d (filtered %d), port %d\n", an, ai, bn);
        return 0;
    }
    return an < TRACE_CAPACITY;
}

static int run_measure_pair(int report)
{
    uint32_t original_sample;
    uint32_t port_sample;
    int n, failures = 0;

    oracle_trace_reset();
    original_sample = oracle_call(oracle_sym("measure_pit_channel0"), 0, NULL);
    n = oracle_trace_count();
    if (n > TRACE_CAPACITY)
        return 1;
    memcpy(original_trace, oracle_trace(), (size_t)n * sizeof original_trace[0]);

    oracle_trace_reset();
    port_sample = (uint32_t)measure_pit_channel0();
    if (!traces_match(original_trace, n, oracle_trace(), oracle_trace_count()))
        failures++;
    if (port_sample != g_pit_elapsed_ticks || port_sample < 0x2710 || port_sample > 0x61a8)
        failures++;
    if (original_sample < 0x2710 || original_sample > 0x61a8)
        failures++;
    if (report)
        printf("    PIT sample: original %04Xh, port %04Xh\n", original_sample, port_sample);
    return failures;
}

static int test_measure_and_trace(void)
{
    int i, failures = 0;
    int result;
    vpit_init();
    for (i = 0; i < 3; i++)
        failures += run_measure_pair(i == 0);
    windows_environment_detected = 0;
    result = verify_timer();
    if (result != -1) {
        printf("    verify_timer returned %d (timer_ok=%d)\n", result, timer_ok);
        failures++;
    }
    return failures;
}

static int test_reload_trace(void)
{
    static const uint32_t values[] = { 0, 1, 0xffff, 0x1234abcd, 0x80000000u, 0xffffffffu };
    unsigned i;
    int failures = 0;
    for (i = 0; i < sizeof values / sizeof values[0] + 64; i++) {
        uint32_t value = i < sizeof values / sizeof values[0] ? values[i] : next_random();
        uint32_t args[1] = { value };
        int n;
        oracle_trace_reset();
        /* This public label is not in ke_symbols.txt; asm/m_09f64 starts at 9F64h and the
         * disassembled measure routine ends at A03Eh, so the next entry is A03Fh. */
        oracle_call((uint8_t *)oracle_object_base(1) + 0xa03f, 1, args);
        n = oracle_trace_count();
        if (n > TRACE_CAPACITY) {
            failures++;
            continue;
        }
        memcpy(original_trace, oracle_trace(), (size_t)n * sizeof original_trace[0]);
        oracle_trace_reset();
        set_pit_channel0_reload((int)value);
        if (!traces_match(original_trace, n, oracle_trace(), oracle_trace_count()))
            failures++;
    }
    return failures;
}

void register_m_09f64_tests(void)
{
    oracle_register("m_09f64 measure_pit_channel0 (range, stability, trace)",
                    test_measure_and_trace);
    oracle_register("m_09f64 set_pit_channel0_reload (randomized input traces)",
                    test_reload_trace);
}
