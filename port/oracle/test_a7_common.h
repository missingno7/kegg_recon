/* Shared fixtures for A7's TASM renderer translations. */
#ifndef KE_ORACLE_TEST_A7_COMMON_H
#define KE_ORACLE_TEST_A7_COMMON_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

#define A7_STATE_BYTES 100
#define A7_ARENA_BYTES 8192

extern unsigned char vga_state[];
extern short page_idx, drawpage;
extern int outpw(int port, int value);

typedef void (*a7_io_observer)(uint16_t port, uint32_t value, int size);
typedef void (*a7_set_observer_fn)(a7_io_observer observer);

static unsigned char *a7_original_state;
static short *a7_original_page_idx;
static short *a7_original_drawpage;

static void a7_init_symbols(void)
{
    if (!a7_original_state)
        a7_original_state = (unsigned char *)oracle_sym("vga_state");
    if (!a7_original_page_idx)
        a7_original_page_idx = (short *)oracle_sym("page_idx");
    if (!a7_original_drawpage)
        a7_original_drawpage = (short *)oracle_sym("drawpage");
}

static void a7_state16(unsigned char *state, unsigned offset, uint16_t value)
{
    state[offset] = (uint8_t)value;
    state[offset + 1] = (uint8_t)(value >> 8);
}

static void a7_state32(unsigned char *state, unsigned offset, uint32_t value)
{
    state[offset] = (uint8_t)value;
    state[offset + 1] = (uint8_t)(value >> 8);
    state[offset + 2] = (uint8_t)(value >> 16);
    state[offset + 3] = (uint8_t)(value >> 24);
}

static uint32_t a7_read_state32(const unsigned char *state, unsigned offset)
{
    return (uint32_t)state[offset] | ((uint32_t)state[offset + 1] << 8) |
           ((uint32_t)state[offset + 2] << 16) | ((uint32_t)state[offset + 3] << 24);
}

static uint16_t a7_read_state16(const unsigned char *state, unsigned offset)
{
    return (uint16_t)(state[offset] | ((uint16_t)state[offset + 1] << 8));
}

static void a7_relocate_oracle_page_bases(void)
{
    unsigned page;
    for (page = 0; page < 4; page++) {
        unsigned offset = 0x02u + page * 4u;
        uint32_t base = a7_read_state32(a7_original_state, offset);
        if (base >= 0xa0000u && base < 0xc0000u) {
            base = oracle_vga_host_address(base);
        } else if (base >= 0x280000u && base < 0x300000u) {
            uint32_t relocated = oracle_vga_host_address(base >> 2);
            base = (relocated << 2) | (base & 3u);
        }
        a7_state32(a7_original_state, offset, base);
    }
}

static void a7_canonicalize_oracle_page_bases(unsigned char *state)
{
    unsigned page;
    for (page = 0; page < 4; page++) {
        unsigned offset = 0x02u + page * 4u;
        uint32_t base = a7_read_state32(state, offset);
        uint32_t guest = oracle_vga_guest_address(base);
        if (guest != base) {
            base = guest;
        } else {
            guest = oracle_vga_guest_address(base >> 2);
            if (guest != (base >> 2))
                base = (guest << 2) | (base & 3u);
        }
        a7_state32(state, offset, base);
    }
}

static uint32_t a7_normalize_relocated_pixel_result(
    uint32_t result, const unsigned char state[A7_STATE_BYTES], short page,
    const uint32_t *args)
{
    uint32_t page_offset = (uint16_t)page * 4u;
    uint32_t base = a7_read_state32(state, 0x02u + page_offset);
    uint32_t origin, x = args[0], y = args[1], stride;
    int32_t sx = (int32_t)x, sy = (int32_t)y;
    if (a7_read_state16(state, 0) == 0 ||
        oracle_vga_host_address(base >> 2) == (base >> 2))
        return result;
    if (sx < (int32_t)a7_read_state32(state, 0x4au)) x = a7_read_state32(state, 0x4au);
    if ((int32_t)x > (int32_t)a7_read_state32(state, 0x52u)) x = a7_read_state32(state, 0x52u);
    if (sy < (int32_t)a7_read_state32(state, 0x4eu)) y = a7_read_state32(state, 0x4eu);
    if ((int32_t)y > (int32_t)a7_read_state32(state, 0x56u)) y = a7_read_state32(state, 0x56u);
    stride = a7_read_state32(state, 0x3au);
    origin = base + a7_read_state32(state, 0x12u + page_offset) +
             a7_read_state32(state, 0x22u + page_offset) + y * stride + x;
    return (result & 0x00ffffffu) | ((origin & 0x00ff0000u) << 8);
}

static void a7_set_page(unsigned char *state, unsigned page, uint32_t base,
                        uint32_t start, uint32_t display)
{
    a7_state32(state, 0x02u + page * 4u, base);
    a7_state32(state, 0x12u + page * 4u, start);
    a7_state32(state, 0x22u + page * 4u, display);
}

static void a7_make_state(unsigned char state[A7_STATE_BYTES], uint16_t mode,
                          uint32_t stride, int planar_storage)
{
    unsigned page;
    memset(state, 0, A7_STATE_BYTES);
    a7_state16(state, 0, mode);
    for (page = 0; page < 4; page++) {
        uint32_t base = planar_storage ? 0x280000u + page * 0x10000u : 0xa0000u;
        a7_set_page(state, page, base, page * 0x100u, page * 0x40u);
    }
    a7_state32(state, 0x3a, stride);
    a7_state32(state, 0x4a, 0);
    a7_state32(state, 0x4e, 0);
    a7_state32(state, 0x52, 63);
    a7_state32(state, 0x56, 63);
    state[0x60] = 0;
    state[0x61] = 0;
}

static void a7_reset_device(void)
{
    unsigned plane, i;
    vga_bios_set_mode(0x13);
    outpw(0x3c4, 0x0604);           /* unchained addressing */
    outpw(0x3c4, 0x0f02);           /* all planes */
    outpw(0x3ce, 0x0004);           /* read plane 0 */
    outpw(0x3ce, 0x4005);           /* 256-color shift, write mode 0 */
    outpw(0x3ce, 0x0506);           /* A0000h aperture */
    outpw(0x3ce, 0xff08);           /* full bit mask */
    outpw(0x3ce, 0x0000);           /* set/reset disabled */
    outpw(0x3ce, 0x0001);           /* enable set/reset disabled */
    outpw(0x3ce, 0x0003);           /* rotate/logical op clear */
    for (plane = 0; plane < 4; plane++) {
        outpw(0x3c4, (int)(((plane + 1u) << 8) | 2u));
        for (i = 0; i < 512; i++)
            vga_mem_write8(0xa0000u + i, (uint8_t)(0x31u + plane * 47u + i * 13u));
    }
    outpw(0x3c4, 0x0f02);
    outpw(0x3ce, 0x0004);
    outpw(0x3ce, 0x4005);
}

static void a7_init_pair(const unsigned char baseline[A7_STATE_BYTES], short page, short draw)
{
    a7_reset_device();
    memcpy(vga_state, baseline, A7_STATE_BYTES);
    a7_init_symbols();
    if (a7_original_state) {
        memcpy(a7_original_state, baseline, A7_STATE_BYTES);
        a7_relocate_oracle_page_bases();
    }
    page_idx = page;
    drawpage = draw;
    if (a7_original_page_idx) *a7_original_page_idx = page;
    if (a7_original_drawpage) *a7_original_drawpage = draw;
}

static void a7_trace_word(uint16_t port, uint32_t value, int size)
{
    oracle_trace_add('O', port, value, size);
}

static int a7_trace_equal(const OracleEvent *a, int count)
{
    int i;
    if (oracle_trace_count() != count) {
        printf("    I/O trace length differs: %d vs %d\n", count, oracle_trace_count());
        return 1;
    }
    for (i = 0; i < count; i++) {
        const OracleEvent *b = &oracle_trace()[i];
        if (a[i].kind != b->kind || a[i].size != b->size || a[i].port != b->port ||
            a[i].value != b->value) {
            printf("    I/O event %d differs: %c/%u %04Xh %08Xh vs %c/%u %04Xh %08Xh\n",
                   i, a[i].kind, a[i].size, a[i].port, (unsigned)a[i].value,
                   b->kind, b->size, b->port, (unsigned)b->value);
            return 1;
        }
    }
    return 0;
}

static uint32_t a7_call_eight(void *fn, const uint32_t *a)
{
    typedef uint32_t (__cdecl *Fn8)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t);
    return ((Fn8)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}

/* Run original and translated entries from identical device, state and RAM images. */
static int a7_compare_call(const char *label, void *original, void *translated,
                           int argc, const uint32_t *args,
                           const unsigned char baseline[A7_STATE_BYTES],
                           short page, short draw, a7_set_observer_fn set_observer,
                           uint8_t *ram, size_t ram_size, int compare_return,
                           int compare_vga)
{
    uint8_t *ram_before = NULL, *ram_original = NULL;
    unsigned char original_state[A7_STATE_BYTES];
    OracleEvent *trace = NULL;
    OracleVgaSnapshot *vga_original = NULL, *vga_ported = NULL;
    uint32_t result_original, result_ported;
    int trace_count, failures = 0;
    a7_init_symbols();
    if (!original || !translated || !a7_original_state || !a7_original_page_idx ||
        !a7_original_drawpage) {
        printf("    %s: oracle symbols unavailable\n", label);
        return 1;
    }
    if (ram && ram_size) {
        ram_before = (uint8_t *)malloc(ram_size);
        ram_original = (uint8_t *)malloc(ram_size);
        if (!ram_before || !ram_original) {
            printf("    %s: RAM fixture allocation failed\n", label);
            failures++;
            goto done;
        }
        memcpy(ram_before, ram, ram_size);
    }
    if (compare_vga) {
        vga_original = (OracleVgaSnapshot *)malloc(sizeof *vga_original);
        vga_ported = (OracleVgaSnapshot *)malloc(sizeof *vga_ported);
        if (!vga_original || !vga_ported) {
            printf("    %s: VGA fixture allocation failed\n", label);
            failures++;
            goto done;
        }
    }
    a7_init_pair(baseline, page, draw);
    oracle_trace_reset();
    result_original = argc == 8 ? a7_call_eight(original, args) : oracle_call(original, argc, args);
    memcpy(original_state, a7_original_state, A7_STATE_BYTES);
    a7_canonicalize_oracle_page_bases(original_state);
    if (compare_return && compare_vga)
        result_original = a7_normalize_relocated_pixel_result(result_original, baseline,
                                                              page, args);
    trace_count = oracle_trace_count();
    if (trace_count) {
        trace = (OracleEvent *)malloc((size_t)trace_count * sizeof *trace);
        if (!trace) { printf("    %s: trace allocation failed\n", label); failures++; goto done; }
        memcpy(trace, oracle_trace(), (size_t)trace_count * sizeof *trace);
    }
    if (ram && ram_size)
        memcpy(ram_original, ram, ram_size);
    if (compare_vga && oracle_vga_snapshot(vga_original) != 0) {
        printf("    %s: original VGA snapshot failed\n", label);
        failures++;
        goto done;
    }

    a7_init_pair(baseline, page, draw);
    if (ram && ram_size)
        memcpy(ram, ram_before, ram_size);
    oracle_trace_reset();
    set_observer(a7_trace_word);
    result_ported = argc == 8 ? a7_call_eight(translated, args) :
                    oracle_port_call(translated, argc, args);
    set_observer(NULL);
    if (compare_return && result_original != result_ported) {
        printf("    %s: return %08Xh vs %08Xh\n", label,
               (unsigned)result_original, (unsigned)result_ported);
        failures++;
    }
    if (memcmp(vga_state, original_state, A7_STATE_BYTES) != 0) {
        printf("    %s: DisplayModeInfo differs\n", label);
        failures++;
    }
    if (ram && ram_size && memcmp(ram, ram_original, ram_size) != 0) {
        size_t i, original_first = ram_size, port_first = ram_size;
        for (i = 0; i < ram_size; i++)
            if (ram[i] != ram_original[i]) {
                printf("    %s: RAM byte +%u differs: %02Xh vs %02Xh\n", label,
                       (unsigned)i, (unsigned)ram_original[i], (unsigned)ram[i]);
                break;
            }
        for (i = 0; i < ram_size; i++) {
            if (ram_original[i] != ram_before[i] && original_first == ram_size)
                original_first = i;
            if (ram[i] != ram_before[i] && port_first == ram_size)
                port_first = i;
        }
        printf("    changed starts original +%u, port +%u\n",
               (unsigned)original_first, (unsigned)port_first);
        failures++;
    }
    failures += a7_trace_equal(trace, trace_count);
    if (compare_vga && (oracle_vga_snapshot(vga_ported) != 0 ||
        oracle_vga_snapshot_equal(vga_original, vga_ported, label) != 0))
        failures++;
done:
    free(ram_before);
    free(ram_original);
    free(trace);
    free(vga_original);
    free(vga_ported);
    if (!failures)
        printf("    %s: identical\n", label);
    return failures;
}

static void *a7_original_at(unsigned offset, const char *name)
{
    void *named = oracle_sym(name);
    return named ? named : (uint8_t *)oracle_object_base(1) + offset;
}

static void *a7_find_code(unsigned first, unsigned last,
                          const uint8_t *signature, size_t signature_size)
{
    const uint8_t *object = (const uint8_t *)oracle_object_base(1);
    unsigned offset;
    void *found = NULL;
    for (offset = first; offset + signature_size <= last; offset++) {
        if (memcmp(object + offset, signature, signature_size) == 0) {
            if (found)
                return NULL;
            found = (void *)(object + offset);
        }
    }
    return found;
}

#endif
