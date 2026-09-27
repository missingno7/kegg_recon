/* test_m_13944.c - oracle differential tests for VGA register helpers and plane caches. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

extern unsigned char vga_state[];
int inp(int port);
int outp(int port, int value);
int update_attr_register(uint8_t, uint8_t, uint8_t);
int update_attr_register_entry(uint8_t, uint8_t, uint8_t);
int update_crtc_register(uint8_t, uint8_t, uint8_t);
int update_crtc_register_entry(uint8_t, uint8_t, uint8_t);
int update_seq_register(uint8_t, uint8_t, uint8_t);
int update_seq_register_entry(uint8_t, uint8_t, uint8_t);
int update_gc_register(uint8_t, uint8_t, uint8_t);
int update_gc_register_entry(uint8_t, uint8_t, uint8_t);
void set_seq_plane_mask(uint32_t);
void set_seq_plane_mask_entry(uint32_t);
void rotate_seq_plane_mask(uint32_t);
void rotate_seq_plane_mask_entry(uint32_t);
void set_gc_read_map(uint32_t);
void set_gc_read_map_entry(uint32_t);
void set_gc_mode(uint32_t);
void set_gc_mode_entry(uint32_t);

#define CACHE_GC_MODE 0x60
#define CACHE_SEQ_MASK 0x61
#define CACHE_READ_MAP 0x62
#define MAX_TRACE 8

static uint32_t rng = 0x5a17a5u;
static uint32_t rnd(uint32_t n) { rng = rng * 1664525u + 1013904223u; return n ? rng % n : 0; }
static uint8_t *original_vga_state;

static const char *const symbols[] = {
    "update_attr_register", "update_attr_register_entry",
    "update_crtc_register", "update_crtc_register_entry",
    "update_seq_register", "update_seq_register_entry",
    "update_gc_register", "update_gc_register_entry",
    "set_seq_plane_mask", "set_seq_plane_mask_entry",
    "rotate_seq_plane_mask", "rotate_seq_plane_mask_entry",
    "set_gc_read_map", "set_gc_read_map_entry",
    "set_gc_mode", "set_gc_mode_entry"
};
static const uint32_t code_offsets[] = {
    0x13944, 0x13944, 0x13964, 0x13964, 0x13984, 0x13984, 0x139a4, 0x139a4,
    0x139c4, 0x139c4, 0x139e3, 0x139e3, 0x13a0a, 0x13a0a, 0x13a29, 0x13a29
};

static void reset_fixture(uint8_t gc_mode, uint8_t seq_mask, uint8_t read_map)
{
    int i;
    vga_bios_set_mode(3);
    for (i = 5; i < 8; i++) {
        outp(0x3c4, i);
        outp(0x3c5, 0);              /* BIOS mode set leaves these modeled seq bytes alone */
    }
    /* The port models 32 CRTC registers, while BIOS mode set initializes only 25. Clear
     * the extra modeled registers as well so both original and translated calls start alike. */
    outp(0x3d4, 0x11);
    outp(0x3d5, 0x0e);              /* clear CRTC write-protect bit */
    for (i = 8; i < 32; i++) {
        outp(0x3d4, i);
        outp(0x3d5, 0);
    }
    vga_bios_set_mode(3);
    (void)inp(0x3da);               /* reading status resets the attribute flip-flop */
    outp(0x3c0, 0);
    outp(0x3c0, 0);
    vga_state[CACHE_GC_MODE] = gc_mode;
    vga_state[CACHE_SEQ_MASK] = seq_mask;
    vga_state[CACHE_READ_MAP] = read_map;
}

static void call_translated(int id, uint8_t a, uint8_t b, uint8_t c)
{
    switch (id) {
    case 0: update_attr_register(a, b, c); break;
    case 1: update_attr_register_entry(a, b, c); break;
    case 2: update_crtc_register(a, b, c); break;
    case 3: update_crtc_register_entry(a, b, c); break;
    case 4: update_seq_register(a, b, c); break;
    case 5: update_seq_register_entry(a, b, c); break;
    case 6: update_gc_register(a, b, c); break;
    case 7: update_gc_register_entry(a, b, c); break;
    case 8: set_seq_plane_mask(a); break;
    case 9: set_seq_plane_mask_entry(a); break;
    case 10: rotate_seq_plane_mask(a); break;
    case 11: rotate_seq_plane_mask_entry(a); break;
    case 12: set_gc_read_map(a); break;
    case 13: set_gc_read_map_entry(a); break;
    case 14: set_gc_mode(a); break;
    case 15: set_gc_mode_entry(a); break;
    }
}

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

static int test_register_traces(void)
{
    OracleEvent before[MAX_TRACE];
    int failures = 0, i;
    for (i = 0; i < 16 * 128; i++) {
        int id = i / 128, argc = id < 8 ? 3 : 1;
        uint8_t a = (uint8_t)rnd(id < 8 ? (id < 2 ? 32 : 256) : 256);
        uint8_t b = (uint8_t)rnd(256), c = (uint8_t)rnd(256);
        uint8_t cache_before[3] = {(uint8_t)rnd(256), (uint8_t)rnd(256), (uint8_t)rnd(256)};
        uint8_t original_cache[3], translated_cache[3];
        uint32_t args[3] = {a, b, c};
        const OracleEvent *trace;
        void *original = oracle_sym(symbols[id & ~1]);
        int n, saved;

        if (!original_vga_state)
            original_vga_state = (uint8_t *)oracle_sym("vga_state");
        if (!original_vga_state) {
            printf("    missing original vga_state data symbol\n");
            return failures + 1;
        }
        original_vga_state[CACHE_GC_MODE] = cache_before[0];
        original_vga_state[CACHE_SEQ_MASK] = cache_before[1];
        original_vga_state[CACHE_READ_MAP] = cache_before[2];
        reset_fixture(cache_before[0], cache_before[1], cache_before[2]);
        oracle_trace_reset();
        /* The manifest omits several unused publics. Their exact LE offsets are the label
         * addresses in this frozen module; canonical labels in the manifest use oracle_sym. */
        if (!original)
            original = (uint8_t *)oracle_object_base(1) + code_offsets[id & ~1];
        oracle_call(original, argc, args);
        trace = oracle_trace();
        n = oracle_trace_count();
        if (n > MAX_TRACE) {
            printf("    %s: trace overflow (%d)\n", symbols[id], n);
            return failures + 1;
        }
        memcpy(before, trace, (size_t)n * sizeof before[0]);
        original_cache[0] = original_vga_state[CACHE_GC_MODE];
        original_cache[1] = original_vga_state[CACHE_SEQ_MASK];
        original_cache[2] = original_vga_state[CACHE_READ_MAP];

        reset_fixture(cache_before[0], cache_before[1], cache_before[2]);
        oracle_trace_reset();
        call_translated(id, a, b, c);
        trace = oracle_trace();
        saved = oracle_trace_count();
        translated_cache[0] = vga_state[CACHE_GC_MODE];
        translated_cache[1] = vga_state[CACHE_SEQ_MASK];
        translated_cache[2] = vga_state[CACHE_READ_MAP];
        if (!traces_equal(before, n, trace, saved) ||
            memcmp(original_cache, translated_cache, sizeof original_cache) != 0) {
            if (failures++ < 1) {
                printf("    %s args=%02X,%02X,%02X: I/O trace or vga_state cache differs\n",
                       symbols[id], a, b, c);
                printf("      original trace=%d translated=%d cache=%02X,%02X,%02X / %02X,%02X,%02X\n",
                       n, saved, original_cache[0], original_cache[1], original_cache[2],
                       translated_cache[0], translated_cache[1], translated_cache[2]);
                { int j; for (j = 0; j < n || j < saved; j++) {
                    if (j < n) printf("      O %d: %c %u %04X %08X\n", j, before[j].kind,
                                      before[j].size, before[j].port, before[j].value);
                    if (j < saved) printf("      T %d: %c %u %04X %08X\n", j, trace[j].kind,
                                          trace[j].size, trace[j].port, trace[j].value);
                } }
            }
        }
    }
    return failures;
}

void register_m_13944_tests(void)
{
    oracle_register("m_13944 VGA register traces and cache bytes (randomized)",
                    test_register_traces);
}
