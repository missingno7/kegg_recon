/* test_m_13a48.c - oracle differential tests for DAC writes and DS-to-ES. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

void write_dac_palette(void *, int, int, int);
void write_dac_palette_entry(void *, int, int, int);
void copy_ds_to_es(void);
extern uint32_t saved_ds;
int outp(int port, int value);

#define MAX_TRACE 1024

static uint32_t rng = 0x13a48u;
static uint32_t rnd(uint32_t n) { rng = rng * 1103515245u + 12345u; return n ? (rng >> 8) % n : 0; }

static int traces_equal(const OracleEvent *a, int na, const OracleEvent *b, int nb)
{
    int i;
    if (na != nb)
        return 0;
    for (i = 0; i < na; i++)
        if (a[i].kind != b[i].kind || a[i].size != b[i].size ||
            a[i].port != b[i].port || a[i].value != b[i].value)
            return 0;
    return 1;
}

static void seed_dac(void)
{
    int i, c;
    outp(0x3c8, 0);
    for (i = 0; i < 256; i++)
        for (c = 0; c < 3; c++)
            outp(0x3c9, (i * 13 + c * 7) & 0x3f);
}

static int test_dac_palette(void)
{
    uint8_t input[768];
    uint8_t original_palette[256][3], translated_palette[256][3];
    OracleEvent before[MAX_TRACE];
    int failures = 0, trial;
    for (trial = 0; trial < 48; trial++) {
        int start = (int)rnd(256);
        int count = 1 + (int)rnd(256);
        int brightness = (int)rnd(321) - 80;
        int i, n, translated_n;
        uint32_t args[4];
        const OracleEvent *trace;
        /* The entry label aliases the public PROC at the same LE offset. */
        void *original = oracle_sym("write_dac_palette");
        if (trial == 0) { start = 0; count = 256; brightness = 0; }
        if (trial == 1) { start = 255; count = 1; brightness = -63; }
        if (trial == 2) { start = 23; count = 1; brightness = 0; }
        for (i = 0; i < count * 3; i++)
            input[i] = (uint8_t)rnd(256);
        if (trial == 2)
            input[0] = 0x3f;       /* exercises the inclusive 3Fh clamp boundary */

        vga_bios_set_mode(3);
        seed_dac();
        oracle_trace_reset();
        args[0] = (uint32_t)(uintptr_t)input;
        args[1] = (uint32_t)start;
        args[2] = (uint32_t)count;
        args[3] = (uint32_t)brightness;
        oracle_call(original, 4, args);
        trace = oracle_trace();
        n = oracle_trace_count();
        if (n > MAX_TRACE) {
            printf("    write_dac_palette: trace overflow (%d)\n", n);
            return failures + 1;
        }
        memcpy(before, trace, (size_t)n * sizeof before[0]);
        vga_palette_rgb888(original_palette);

        vga_bios_set_mode(3);
        seed_dac();
        oracle_trace_reset();
        if (trial & 1)
            write_dac_palette_entry(input, start, count, brightness);
        else
            write_dac_palette(input, start, count, brightness);
        trace = oracle_trace();
        translated_n = oracle_trace_count();
        vga_palette_rgb888(translated_palette);
        if (!traces_equal(before, n, trace, translated_n) ||
            memcmp(original_palette, translated_palette, sizeof original_palette) != 0 ||
            n != 1 + 3 * count) {
            if (failures++ < 5)
                printf("    DAC trial %d (start=%d count=%d brightness=%d): result or trace differs\n",
                       trial, start, count, brightness);
        }
    }
    return failures;
}

static int test_copy_ds_to_es(void)
{
    uint32_t *original_saved = (uint32_t *)oracle_sym("saved_ds");
    uint16_t original_es, translated_es;
    uint32_t original_value;
    int failures = 0;
    if (!original_saved)
        return 1;
    *original_saved = 0xa55a0000u;
    oracle_call(oracle_sym("copy_ds_to_es"), 0, NULL);
    original_value = *original_saved;
    __asm__ volatile("mov %%es, %0" : "=r"(original_es));

    saved_ds = 0xa55a0000u;
    copy_ds_to_es();
    __asm__ volatile("mov %%es, %0" : "=r"(translated_es));
    if (saved_ds != original_value || original_es != translated_es ||
        (uint16_t)saved_ds != translated_es) {
        printf("    copy_ds_to_es: saved selector or ES differs\n");
        failures++;
    }
    return failures;
}

void register_m_13a48_tests(void)
{
    oracle_register("m_13a48 DAC palette (randomized data and brightness)", test_dac_palette);
    oracle_register("m_13a48 copy_ds_to_es selector", test_copy_ds_to_es);
}
