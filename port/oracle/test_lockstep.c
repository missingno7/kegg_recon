/* Regression tests for the divergences found by the lockstep state diff (docs/port/lockstep.md).
 *
 *   L1  measure_pit_channel0: complete port I/O sequence (incl. the 3DAh polls of the C helper
 *       wait_for_vsync) and result equal the original on the deterministic lockstep clock.
 *   L2/L5/L6/L7  kind-5 planar BOBs with widths that are not multiples of 4, every x phase, left
 *       clips of 1..7 pixels and fully clipped sprites: four planes and the renderer globals.
 *   L4  encoded background restore (replay_sprite_update_list, kind 5 unclipped records).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

extern unsigned char vga_state[];
extern short image_color_depth, image_buffer_error_code, drawpage, page2;
extern unsigned char *active_video_page_buffer;
extern unsigned char *image_buffer_cursor, *image_update_list_start;
extern unsigned int image_buffer_record_limit;
extern uint32_t sprite_clip_top, sprite_clip_bottom, sprite_clip_left, sprite_clip_right;
extern uint32_t visible_sprite_width, vga_row_advance, vga_plane_index;
void draw_bob_sprite_entry(const uint8_t *record, int32_t x, int32_t y);
void render_image_region(int x, int y);
void replay_sprite_update_list(int unused);
int measure_pit_channel0(void);
unsigned outpw(int port, int value);

#define RECORD_BYTES 8192

/* ---- L1 ------------------------------------------------------------------------------- */
#define IO_CAP 200000
typedef struct IoEvent { char kind; uint16_t port; uint32_t value; } IoEvent;
static IoEvent *io_events;
static int io_count;

static void record_io(char kind, uint16_t port, uint32_t value)
{
    if (io_count < IO_CAP) {
        io_events[io_count].kind = kind;
        io_events[io_count].port = port;
        io_events[io_count].value = value;
    }
    io_count++;
}

static uint32_t run_measure(int original)
{
    uint32_t result;
    vhw_lockstep_ns = 1000000000ull;
    vpit_init();
    vga_bios_set_mode(0x13);
    io_count = 0;
    vhw_io_trace_hook = record_io;
    result = original ? oracle_call(oracle_sym("measure_pit_channel0"), 0, NULL)
                      : (uint32_t)measure_pit_channel0();
    vhw_io_trace_hook = NULL;
    return result;
}

static int test_measure_pit_io_sequence(void)
{
    IoEvent *expected = calloc(IO_CAP, sizeof *expected);
    int failures = 0, expected_count, i;
    uint32_t want, got;
    io_events = calloc(IO_CAP, sizeof *io_events);
    if (!expected || !io_events) {
        free(expected);
        free(io_events);
        return 1;
    }
    vhw_lockstep = 1;
    want = run_measure(1);
    expected_count = io_count;
    memcpy(expected, io_events, sizeof *expected * (size_t)(io_count < IO_CAP ? io_count : IO_CAP));
    got = run_measure(0);
    vhw_lockstep = 0;
    vcpu_sti();
    if (got != want) {
        printf("    L1 measure_pit_channel0 = %u, original %u\n", got, want);
        failures++;
    }
    if (io_count != expected_count) {
        printf("    L1 %d port accesses, original %d\n", io_count, expected_count);
        failures++;
    }
    for (i = 0; i < io_count && i < expected_count && i < IO_CAP; i++)
        if (io_events[i].kind != expected[i].kind || io_events[i].port != expected[i].port ||
            io_events[i].value != expected[i].value) {
            printf("    L1 access %d: %c %04X %X, original %c %04X %X\n", i, io_events[i].kind,
                   io_events[i].port, io_events[i].value, expected[i].kind, expected[i].port,
                   expected[i].value);
            failures++;
            break;
        }
    free(expected);
    free(io_events);
    io_events = NULL;
    return failures;
}

/* ---- kind-5 fixtures ------------------------------------------------------------------ */
static void set_word(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void set_dword(uint8_t *p, uint32_t v)
{
    set_word(p, (uint16_t)v);
    set_word(p + 2, (uint16_t)(v >> 16));
}

static void set_state(uint8_t *state)
{
    memset(state, 0, 100);
    set_word(state, 1);                         /* planar */
    set_dword(state + 2, 0x280000u);
    set_dword(state + 6, 0x280000u);
    set_dword(state + 0x16, 0x4000u);           /* page 1 two rows of blocks further on */
    set_dword(state + 0x3a, 320);
    set_dword(state + 0x3e, 200);
    set_dword(state + 0x4a, 10);                /* view left, top, right, bottom */
    set_dword(state + 0x4e, 10);
    set_dword(state + 0x52, 79);
    set_dword(state + 0x56, 49);
}

static void setup_vga(void)
{
    unsigned plane, offset;
    vga_bios_set_mode(0x13);
    outpw(0x3c4, 0x0604);
    outpw(0x3ce, 0x0005);
    outpw(0x3ce, 0x0506);
    outpw(0x3ce, 0xff08);
    for (plane = 0; plane < 4; plane++) {
        outpw(0x3c4, (int)((1u << plane) << 8) | 2);
        for (offset = 0; offset < 0x8000; offset++)
            vga_mem_write8(0xa0000u + offset, (uint8_t)(plane * 37u + offset * 3u + (offset >> 7)));
    }
}

/* Planar record: four per-plane RLE streams (runs with bytes) and the pixel-run stream the
 * background restore uses (run lengths only). */
static uint8_t *make_kind5(uint8_t *record, uint16_t width, uint16_t height, unsigned seed)
{
    uint8_t *table = record + 18;
    uint32_t offset = 0, row;
    unsigned plane;
    set_word(record + 2, width);
    set_word(record + 4, height);
    set_word(record + 6, 28);
    set_word(record + 8, 5);
    for (plane = 0; plane < 4; plane++) {
        uint32_t samples = (width + 3u - plane) / 4u;
        set_word(table + plane * 2u, (uint16_t)offset);
        for (row = 0; row < height; row++) {
            uint32_t x = 0, runs_at = offset++;
            unsigned runs = 0;
            while (x < samples) {
                uint32_t n = 1u + (x + row + plane + seed) % 3u;
                if (n > samples - x) n = samples - x;
                runs++;
                if ((x + row + seed) % 4u == 3u) {
                    record[28 + offset++] = (uint8_t)(0u - n);
                } else {
                    uint32_t i;
                    record[28 + offset++] = (uint8_t)n;
                    for (i = 0; i < n; i++)
                        record[28 + offset++] = (uint8_t)(0x21u + plane * 31u + row * 7u + x + i);
                }
                x += n;
            }
            record[28 + runs_at] = (uint8_t)runs;
        }
    }
    set_word(table + 8, (uint16_t)offset);
    for (row = 0; row < height; row++) {
        uint32_t x = 0, runs_at = offset++;
        unsigned runs = 0;
        while (x < width) {
            uint32_t n = 1u + (x * 3u + row + seed) % 5u;
            if (n > width - x) n = width - x;
            runs++;
            record[28 + offset++] = (uint8_t)(((x + row) % 3u == 2u) ? 0u - n : n);
            x += n;
        }
        record[28 + runs_at] = (uint8_t)runs;
    }
    return record;
}

typedef struct SpriteGlobals { uint32_t v[7]; } SpriteGlobals;
static const char *const global_names[7] = {
    "sprite_clip_top", "sprite_clip_bottom", "sprite_clip_left", "sprite_clip_right",
    "visible_sprite_width", "vga_row_advance", "vga_plane_index"
};

/* The last two are internal labels of asm/m_12a9c_12f30.asm (_DATA order after
 * sprite_source_column 8376h): not manifest symbols. */
static uint32_t *orig_global(int i)
{
    if (i == 5)
        return (uint32_t *)((uint8_t *)oracle_object_base(3) + 0x836c);   /* vga_row_advance */
    if (i == 6)
        return (uint32_t *)((uint8_t *)oracle_object_base(3) + 0x838c);   /* vga_plane_index */
    return (uint32_t *)oracle_sym(global_names[i]);
}

static void read_globals(int original, SpriteGlobals *g)
{
    int i;
    uint32_t *port[7] = {&sprite_clip_top, &sprite_clip_bottom, &sprite_clip_left,
                         &sprite_clip_right, &visible_sprite_width, &vga_row_advance,
                         &vga_plane_index};
    for (i = 0; i < 7; i++)
        g->v[i] = original ? *orig_global(i) : *port[i];
}

static void setup_globals(int original, int depth, uint8_t *commands, uint8_t *updates)
{
    int i;
    uint32_t *port[7] = {&sprite_clip_top, &sprite_clip_bottom, &sprite_clip_left,
                         &sprite_clip_right, &visible_sprite_width, &vga_row_advance,
                         &vga_plane_index};
    if (original) {
        set_state((uint8_t *)oracle_sym("vga_state"));
        *(short *)oracle_sym("image_color_depth") = (short)depth;
        *(short *)oracle_sym("image_buffer_error_code") = 0;
        *(short *)oracle_sym("drawpage") = 0;
        *(short *)oracle_sym("page2") = 1;
        *(unsigned int *)oracle_sym("image_buffer_record_limit") = 64;
        *(uint8_t **)oracle_sym("image_update_list_start") = commands;
        *(uint8_t **)oracle_sym("image_buffer_cursor") = commands;
        *(uint8_t **)oracle_sym("active_video_page_buffer") = updates;
        for (i = 0; i < 7; i++)
            *orig_global(i) = 0x5a5a5a5au;
    } else {
        set_state(vga_state);
        image_color_depth = (short)depth;
        image_buffer_error_code = 0;
        drawpage = 0;
        page2 = 1;
        image_buffer_record_limit = 64;
        image_update_list_start = commands;
        image_buffer_cursor = commands;
        active_video_page_buffer = updates;
        for (i = 0; i < 7; i++)
            *port[i] = 0x5a5a5a5au;
    }
}

static int compare_side(const char *label, OracleVgaSnapshot *a, OracleVgaSnapshot *b,
                        const SpriteGlobals *ga, const SpriteGlobals *gb)
{
    int failures = 0, i;
    if (oracle_vga_snapshot_equal(a, b, label) != 0)
        failures++;
    for (i = 0; i < 7; i++)
        if (ga->v[i] != gb->v[i]) {
            printf("    %s: %s %08X, original %08X\n", label, global_names[i], gb->v[i], ga->v[i]);
            failures++;
        }
    return failures;
}

static int draw_case(const uint8_t *record, int32_t x, int32_t y, OracleVgaSnapshot *o,
                     OracleVgaSnapshot *p, const char *label)
{
    uint32_t args[3] = {(uint32_t)(uintptr_t)record, (uint32_t)x, (uint32_t)y};
    uint8_t *updates = calloc(1, 4096);
    SpriteGlobals go, gp;
    int failures;
    setup_vga();
    setup_globals(1, 8, NULL, updates);
    oracle_call(oracle_sym("draw_bob_sprite_entry"), 3, args);
    read_globals(1, &go);
    oracle_vga_snapshot(o);
    setup_vga();
    setup_globals(0, 8, NULL, updates);
    oracle_port_call((void *)draw_bob_sprite_entry, 3, args);
    read_globals(0, &gp);
    oracle_vga_snapshot(p);
    failures = compare_side(label, o, p, &go, &gp);
    free(updates);
    return failures;
}

static int test_kind5_widths_and_clips(void)
{
    OracleVgaSnapshot *o = malloc(sizeof *o), *p = malloc(sizeof *p);
    uint8_t *record = calloc(1, RECORD_BYTES);
    int failures = 0, cases = 0;
    uint16_t width;
    char label[64];
    if (!o || !p || !record)
        return 1;
    for (width = 5; width <= 19; width += 2) {
        int32_t x;
        make_kind5(record, width, 6, width);
        for (x = 3; x <= 21; x++) {         /* x < 10: left clip 1..7; x & 3: every phase */
            snprintf(label, sizeof label, "L2/L6 kind5 w%u x%d", width, x);
            failures += draw_case(record, x, 20, o, p, label);
            cases++;
            if (failures > 4)
                goto done;
        }
        for (x = 64; x <= 80; x++) {        /* right bound 79: L7 right-clipped path */
            snprintf(label, sizeof label, "L7 kind5 w%u x%d right clip", width, x);
            failures += draw_case(record, x, 20, o, p, label);
            if (failures > 4)
                goto done;
        }
        snprintf(label, sizeof label, "L5 kind5 w%u fully clipped below", width);
        failures += draw_case(record, 30, 52, o, p, label);        /* top 52 > bottom 49 */
        snprintf(label, sizeof label, "L5 kind5 w%u bottom clip", width);
        failures += draw_case(record, 30, 46, o, p, label);
        snprintf(label, sizeof label, "L5 kind5 w%u left+right", width);
        failures += draw_case(record, 7, 20, o, p, label);
    }
done:
    if (failures)
        printf("    kind-5 cases run: %d\n", cases);
    free(o);
    free(p);
    free(record);
    return failures;
}

/* Draw (depth 4 keeps update records), repaint, then restore both pages' backgrounds. */
static int restore_case(const uint8_t *record, int32_t x, int32_t y, OracleVgaSnapshot *o,
                        OracleVgaSnapshot *p, const char *label)
{
    uint8_t *commands = calloc(4, 10), *updates = calloc(1, 4096);
    uint32_t args[2] = {0, 0}, zero = 0;
    SpriteGlobals go, gp;
    int failures;
    set_dword(commands, (uint32_t)(uintptr_t)record);
    set_word(commands + 4, (uint16_t)x);
    set_word(commands + 6, (uint16_t)y);
    setup_vga();
    setup_globals(1, 4, commands, updates);
    oracle_call(oracle_sym("render_image_region"), 2, args);
    oracle_call(oracle_sym("replay_sprite_update_list_entry"), 1, &zero);
    read_globals(1, &go);
    oracle_vga_snapshot(o);
    memset(updates, 0, 4096);
    setup_vga();
    setup_globals(0, 4, commands, updates);
    oracle_port_call((void *)render_image_region, 2, args);
    oracle_port_call((void *)replay_sprite_update_list, 1, &zero);
    read_globals(0, &gp);
    oracle_vga_snapshot(p);
    failures = compare_side(label, o, p, &go, &gp);
    free(commands);
    free(updates);
    return failures;
}

static int test_kind5_background_restore(void)
{
    OracleVgaSnapshot *o = malloc(sizeof *o), *p = malloc(sizeof *p);
    uint8_t *record = calloc(1, RECORD_BYTES);
    int failures = 0;
    uint16_t width;
    char label[64];
    if (!o || !p || !record)
        return 1;
    for (width = 6; width <= 17 && failures < 4; width += 1) {
        make_kind5(record, width, 5, width * 3u);
        snprintf(label, sizeof label, "L4 restore w%u x%d", width, 20 + width % 4);
        failures += restore_case(record, 20 + width % 4, 18, o, p, label);
        snprintf(label, sizeof label, "L4 restore w%u top-clipped", width);
        failures += restore_case(record, 24, 8, o, p, label);
    }
    free(o);
    free(p);
    free(record);
    return failures;
}

void register_lockstep_tests(void)
{
    oracle_register("L1 measure_pit_channel0 I/O sequence on the lockstep clock",
                    test_measure_pit_io_sequence);
    oracle_register("L2/L5/L6/L7 kind-5 BOB widths, phases and clipping globals",
                    test_kind5_widths_and_clips);
    oracle_register("L4 kind-5 encoded background restore", test_kind5_background_restore);
}
