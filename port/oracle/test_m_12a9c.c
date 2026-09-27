/* A6 differential coverage: BOB clipping, transparent/RLE pixels, planar runs and pages. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../asm/sprite_records.h"

extern unsigned char vga_state[];
extern short image_color_depth, image_buffer_error_code, drawpage, page2;
extern unsigned char *active_video_page_buffer;
extern unsigned char *image_buffer_cursor, *image_update_list_start;
extern unsigned int image_buffer_record_limit;
void draw_bob_sprite_entry(const uint8_t *record, int32_t x, int32_t y);
void render_image_region(int x, int y);

#define TEST_WIDTH 320
#define TEST_HEIGHT 200
#define RECORD_BYTES 8192
#define TRACE_CAPACITY 4096

static OracleEvent expected_trace[TRACE_CAPACITY];
static int expected_trace_count;

static int save_trace(void)
{
    expected_trace_count = oracle_trace_count();
    if (expected_trace_count > TRACE_CAPACITY) {
        printf("    A6 I/O trace exceeded %d events\n", TRACE_CAPACITY);
        expected_trace_count = -1;
        return 1;
    }
    memcpy(expected_trace, oracle_trace(),
           (size_t)expected_trace_count * sizeof expected_trace[0]);
    return 0;
}

static int compare_trace(const char *label)
{
    const OracleEvent *actual = oracle_trace();
    int count = oracle_trace_count(), i;
    if (expected_trace_count < 0)
        return 1;
    if (count != expected_trace_count) {
        printf("    %s I/O trace length %d != %d\n", label, count, expected_trace_count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        const OracleEvent *a = &actual[i], *e = &expected_trace[i];
        if (a->kind != e->kind || a->size != e->size || a->port != e->port ||
            a->value != e->value) {
            printf("    %s I/O trace event %d differs\n", label, i);
            return 1;
        }
    }
    return 0;
}

static uint32_t rng_state = 0x31415926u;
static uint32_t random_u32(uint32_t limit)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return limit ? rng_state % limit : 0;
}

static void set_word(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void set_dword(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void set_state(uint8_t *state, int planar)
{
    uint32_t page_base = planar ? 0x280000u : 0xa0000u;
    memset(state, 0, 100);
    set_word(state, (uint16_t)planar);
    set_dword(state + 2, page_base);
    set_dword(state + 6, page_base);
    set_dword(state + 0x12, 0);
    set_dword(state + 0x16, 0);
    set_dword(state + 0x22, 0);
    set_dword(state + 0x26, 0);
    set_dword(state + 0x3a, TEST_WIDTH);
    set_dword(state + 0x3e, TEST_HEIGHT);
    set_dword(state + 0x4a, 10);
    set_dword(state + 0x4e, 10);
    set_dword(state + 0x52, 39);
    set_dword(state + 0x56, 29);
    state[0x60] = 0;
    state[0x61] = 0;
}

static void setup_vga(int planar)
{
    unsigned plane, offset;
    vga_bios_set_mode(0x13);
    if (planar) {
        outpw(0x3c4, 0x0604);              /* unchained four-plane mode */
        outpw(0x3ce, 0x0005);              /* write mode 0, read mode 0 */
        outpw(0x3ce, 0x0506);              /* A000 aperture, graphics mode */
        outpw(0x3ce, 0xff08);
        for (plane = 0; plane < 4; plane++) {
            outpw(0x3c4, (int)((1u << plane) << 8) | 2);
            for (offset = 0; offset < 4096; offset++)
                vga_mem_write8(0xa0000u + offset, (uint8_t)(plane * 37u + offset * 3u));
        }
    } else {
        outpw(0x3c4, 0x0e04);              /* chain-4 indexed-color mode */
        outpw(0x3c4, 0x0f02);
        outpw(0x3ce, 0x4005);              /* 256-color shift, write mode 0 */
        outpw(0x3ce, 0x0506);
        outpw(0x3ce, 0xff08);
        for (offset = 0; offset < 4096; offset++)
            vga_mem_write8(0xa0000u + offset, (uint8_t)(offset * 11u + 0x59u));
    }
}

static uint8_t *make_transparent(uint8_t *record, uint16_t width, uint16_t height)
{
    uint32_t i;
    set_word(record + 2, width);
    set_word(record + 4, height);
    set_word(record + 6, 18);
    set_word(record + 8, 1);
    for (i = 0; i < (uint32_t)width * height; i++)
        record[18 + i] = (i % 5u == 0) ? 0 : (uint8_t)(i * 19u + 7u);
    return record;
}

static uint8_t *make_rle(uint8_t *record, uint16_t width, uint16_t height)
{
    uint8_t *out = record + 18;
    uint32_t row;
    set_word(record + 2, width);
    set_word(record + 4, height);
    set_word(record + 6, 18);
    set_word(record + 8, 3);
    for (row = 0; row < height; row++) {
        uint32_t x = 0;
        uint8_t *run_count = out++;
        unsigned runs = 0;
        while (x < width) {
            uint32_t n = (x % 3u == 1u) ? 2u : 3u;
            if (n > width - x) n = width - x;
            runs++;
            if ((x + row) % 4u == 1u) {
                *out++ = (uint8_t)(0u - n);
            } else {
                uint32_t i;
                *out++ = (uint8_t)n;
                for (i = 0; i < n; i++)
                    *out++ = (uint8_t)(x * 17u + row * 29u + i + 1u);
            }
            x += n;
        }
        *run_count = (uint8_t)runs;
    }
    return record;
}

static uint8_t *make_planar(uint8_t *record, uint16_t width, uint16_t height)
{
    uint8_t *table = record + 18;
    uint32_t samples = (width + 3u) / 4u;
    uint32_t offset = 0;
    unsigned plane;
    set_word(record + 2, width);
    set_word(record + 4, height);
    set_word(record + 6, 28);
    set_word(record + 8, 5);
    for (plane = 0; plane < 4; plane++) {
        uint32_t row;
        set_word(table + plane * 2u, (uint16_t)offset);
        for (row = 0; row < height; row++) {
            uint32_t x;
            record[28 + offset++] = 1;    /* one literal run in this plane row */
            record[28 + offset++] = (uint8_t)samples;
            for (x = 0; x < samples; x++)
                record[28 + offset++] = (uint8_t)(0x21u + plane * 31u + row * 7u + x);
        }
    }
    set_word(table + 8, (uint16_t)offset);
    /* Background replay's row stream is present after the four plane streams. */
    for (uint32_t row = 0; row < height; row++) {
        record[28 + offset++] = 1;
        record[28 + offset++] = (uint8_t)samples;
        memset(record + 28 + offset, 0x55, samples);
        offset += samples;
    }
    return record;
}

static void setup_image_globals(int original, int planar, uint8_t *updates)
{
    uint8_t *state = original ? (uint8_t *)oracle_sym("vga_state") : vga_state;
    set_state(state, planar);
    if (original) {
        *(short *)oracle_sym("image_color_depth") = 8;
        *(short *)oracle_sym("image_buffer_error_code") = 0;
        *(short *)oracle_sym("drawpage") = 0;
        *(short *)oracle_sym("page2") = 1;
        *(uint8_t **)oracle_sym("active_video_page_buffer") = updates;
        *(int *)oracle_sym("render_page_base") = 0;
        *(int *)oracle_sym("screen_page_base") = 0;
    } else {
        image_color_depth = 8;
        image_buffer_error_code = 0;
        drawpage = 0;
        page2 = 1;
        active_video_page_buffer = updates;
    }
}

static int run_one(const uint8_t *record, int32_t x, int32_t y, int planar,
                   OracleVgaSnapshot *original, OracleVgaSnapshot *ported)
{
    uint8_t *updates = calloc(1, 4096);
    uint32_t args[3] = {(uint32_t)(uintptr_t)record, (uint32_t)x, (uint32_t)y};
    int failed = 0;
    if (!updates) return 1;
    setup_vga(planar);
    setup_image_globals(1, planar, updates);
    oracle_trace_reset();
    oracle_call(oracle_sym("draw_bob_sprite_entry"), 3, args);
    if (save_trace() != 0)
        failed++;
    if (oracle_vga_snapshot(original) != 0)
        failed++;
    setup_vga(planar);
    setup_image_globals(0, planar, updates);
    oracle_trace_reset();
    oracle_port_call((void *)draw_bob_sprite_entry, 3, args);
    if (compare_trace("A6 draw_bob_sprite") != 0)
        failed++;
    if (oracle_vga_snapshot(ported) != 0 ||
        oracle_vga_snapshot_equal(original, ported, "A6 draw_bob_sprite") != 0)
        failed++;
    free(updates);
    return failed;
}

static int test_sprite_renderers(void)
{
    OracleVgaSnapshot *original = malloc(sizeof *original);
    OracleVgaSnapshot *ported = malloc(sizeof *ported);
    uint8_t *record = calloc(1, RECORD_BYTES);
    int failures = 0, i;
    if (!original || !ported || !record || !oracle_vga_window_reserved()) {
        printf("    A6 fixture allocation/VGA trap unavailable\n");
        failures++;
        goto done;
    }
    for (i = 0; i < 24; i++) {
        uint16_t width = (uint16_t)(8 + random_u32(13));
        uint16_t height = (uint16_t)(3 + random_u32(7));
        int32_t x = (int32_t)(8 + random_u32(28)) - (i % 4 == 0 ? 5 : 0);
        int32_t y = (int32_t)(8 + random_u32(22)) - (i % 5 == 0 ? 4 : 0);
        if (i == 1 || i == 2) { x = 14; y = 14; }
        if (i % 3 == 2) width = (uint16_t)((2 + random_u32(4)) * 4);
        if (i % 3 == 0) {
            make_transparent(record, width, height);
            failures += run_one(record, x, y, 0, original, ported);
        } else if (i % 3 == 1) {
            make_rle(record, width, height);
            failures += run_one(record, x, y, 0, original, ported);
        } else {
            make_planar(record, width, height);
            failures += run_one(record, x, y, 1, original, ported);
        }
    }
    make_planar(record, 16, 4);
    failures += run_one(record, 6, 14, 1, original, ported); /* left-edge clipping */
done:
    free(record);
    free(original);
    free(ported);
    return failures;
}

static int test_mutation_sensitivity(void)
{
    OracleVgaSnapshot *original = malloc(sizeof *original);
    OracleVgaSnapshot *ported = malloc(sizeof *ported);
    uint8_t *record = calloc(1, RECORD_BYTES);
    uint8_t *updates = calloc(1, 4096);
    uint32_t args[3];
    int differs;
    if (!original || !ported || !record || !updates) {
        free(original); free(ported); free(record); free(updates);
        return 1;
    }
    make_transparent(record, 4, 2);
    args[0] = (uint32_t)(uintptr_t)record; args[1] = 14; args[2] = 14;
    setup_vga(0); setup_image_globals(1, 0, updates);
    oracle_call(oracle_sym("draw_bob_sprite_entry"), 3, args);
    oracle_vga_snapshot(original);
    record[18] = 0x7f;              /* mutation: turn the first transparent pixel opaque */
    setup_vga(0); setup_image_globals(0, 0, updates);
    oracle_port_call((void *)draw_bob_sprite_entry, 3, args);
    oracle_vga_snapshot(ported);
    differs = memcmp(original->planes, ported->planes, sizeof original->planes) != 0;
    free(original); free(ported); free(record); free(updates);
    return differs ? 0 : 1;
}

static void set_image_region_globals(int original, uint8_t *commands, uint8_t *updates,
                                     unsigned int limit, int planar)
{
    if (original) {
        set_state((uint8_t *)oracle_sym("vga_state"), planar);
        *(short *)oracle_sym("image_color_depth") = 4;
        *(short *)oracle_sym("image_buffer_error_code") = 0;
        *(short *)oracle_sym("drawpage") = 0;
        *(short *)oracle_sym("page2") = 1;
        *(unsigned int *)oracle_sym("image_buffer_record_limit") = limit;
        *(uint8_t **)oracle_sym("image_update_list_start") = commands;
        *(uint8_t **)oracle_sym("image_buffer_cursor") = commands;
        *(uint8_t **)oracle_sym("active_video_page_buffer") = updates;
        *(int *)oracle_sym("render_page_base") = 0;
        *(int *)oracle_sym("screen_page_base") = 0;
    } else {
        set_state(vga_state, planar);
        image_color_depth = 4;
        image_buffer_error_code = 0;
        drawpage = 0;
        page2 = 1;
        image_buffer_record_limit = limit;
        image_update_list_start = commands;
        image_buffer_cursor = commands;
        active_video_page_buffer = updates;
    }
}

static int run_image_region_case(uint8_t *bob, unsigned int command_count,
                                 unsigned int record_limit, int planar)
{
    uint8_t *commands = calloc(command_count + 1u, 10);
    uint8_t *updates = calloc(1, 4096);
    uint8_t *expected_updates = calloc(1, 4096);
    OracleVgaSnapshot *original = malloc(sizeof *original);
    OracleVgaSnapshot *ported = malloc(sizeof *ported);
    uint32_t args[2] = {0, 0};
    unsigned i;
    int failures = 0, original_error, port_error;
    if (!commands || !updates || !expected_updates || !original || !ported) {
        failures++;
        goto done;
    }
    for (i = 0; i < command_count; i++) {
        uint8_t *command = commands + i * 10u;
        set_dword(command, (uint32_t)(uintptr_t)bob);
        set_word(command + 4, (uint16_t)(14 + (i % 3u)));
        set_word(command + 6, (uint16_t)(14 + (i % 2u)));
        set_word(command + 8, 0);
    }
    setup_vga(planar);
    set_image_region_globals(1, commands, updates, record_limit, planar);
    oracle_trace_reset();
    oracle_call(oracle_sym("render_image_region"), 2, args);
    if (save_trace() != 0)
        failures++;
    oracle_vga_snapshot(original);
    original_error = *(short *)oracle_sym("image_buffer_error_code");
    memcpy(expected_updates, updates, 4096);

    memset(updates, 0, 4096);
    setup_vga(planar);
    set_image_region_globals(0, commands, updates, record_limit, planar);
    oracle_trace_reset();
    oracle_port_call((void *)render_image_region, 2, args);
    if (compare_trace("A6 update-list") != 0)
        failures++;
    oracle_vga_snapshot(ported);
    port_error = image_buffer_error_code;
    if (oracle_vga_snapshot_equal(original, ported, "A6 update-list pages") != 0)
        failures++;
    if (memcmp(expected_updates, updates, 4096) != 0) {
        printf("    A6 saved update records differ for %u command(s)\n", command_count);
        failures++;
    }
    if (original_error != port_error) {
        printf("    A6 image_buffer_error_code %04X != %04X\n",
               original_error & 0xffff, port_error & 0xffff);
        failures++;
    }
done:
    free(commands); free(updates); free(expected_updates); free(original); free(ported);
    return failures;
}

static int test_update_list_and_overflow(void)
{
    uint8_t *bob = calloc(1, RECORD_BYTES);
    int failures = 0;
    if (!bob) return 1;
    make_transparent(bob, 8, 4);
    failures += run_image_region_case(bob, 2, 8, 0);
    failures += run_image_region_case(bob, 11, 1, 0); /* historical guard sets 407h */
    make_rle(bob, 8, 4);
    failures += run_image_region_case(bob, 2, 8, 0);
    make_planar(bob, 8, 4);
    failures += run_image_region_case(bob, 2, 8, 1);
    free(bob);
    return failures;
}

void register_m_12a9c_tests(void)
{
    oracle_register("A6 sprite renderers (random clipping and four planes)", test_sprite_renderers);
    oracle_register("A6 update-list records and 407h overflow", test_update_list_and_overflow);
    oracle_register("A6 renderer mutation sensitivity", test_mutation_sensitivity);
}
