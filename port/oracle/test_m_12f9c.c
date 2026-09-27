/* test_m_12f9c.c - differential tests for spans and clipped screen rectangles. */
#include "test_a7_common.h"

void copy_screen_span(uint32_t source_page, uint32_t source_offset,
                      uint32_t destination_page, uint32_t destination_offset,
                      uint32_t byte_count);
void copy_screen_span_entry(uint32_t source_page, uint32_t source_offset,
                            uint32_t destination_page, uint32_t destination_offset,
                            uint32_t byte_count);
void copy_clipped_screen_rectangle(uint32_t source_page,
                                   uint32_t left, uint32_t top,
                                   uint32_t right, uint32_t bottom,
                                   uint32_t destination_page, uint32_t destination_x,
                                   uint32_t destination_y);
void m_12f9c_set_io_observer(a7_io_observer observer);

static uint32_t rng_state = 0x12f9ca7u;
static uint32_t rnd(uint32_t limit)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return limit ? rng_state % limit : 0;
}

static uint8_t *make_arena(void)
{
    uint8_t *p = (uint8_t *)VirtualAlloc(NULL, A7_ARENA_BYTES, MEM_COMMIT | MEM_RESERVE,
                                         PAGE_READWRITE);
    unsigned i;
    if (p)
        for (i = 0; i < A7_ARENA_BYTES; i++) p[i] = (uint8_t)(i * 17u + (i >> 5) + 0x2d);
    return p;
}

static int test_spans_and_rectangles(void)
{
    unsigned char baseline[A7_STATE_BYTES];
    uint8_t *ram = make_arena();
    uint32_t args[8];
    int failures = 0;
    if (!ram) {
        printf("    screen-copy RAM allocation failed\n");
        return 1;
    }

    a7_make_state(baseline, 0, 64, 0);
    a7_set_page(baseline, 0, (uint32_t)(uintptr_t)(ram + 128), 16, 8);
    a7_set_page(baseline, 1, (uint32_t)(uintptr_t)(ram + 1024), 0, 0);
    args[0] = 0; args[1] = 9; args[2] = 1; args[3] = 21; args[4] = 83;
    failures += a7_compare_call("copy_screen_span (RAM, dword and byte tails)",
        a7_original_at(0x12f9c, "copy_screen_span_entry"),
        (void *)copy_screen_span_entry, 5, args, baseline, 0, 0,
        m_12f9c_set_io_observer, ram, A7_ARENA_BYTES, 0, 0);

    a7_make_state(baseline, 0, 64, 0);
    a7_state32(baseline, 0x4a, 4); a7_state32(baseline, 0x4e, 2);
    a7_state32(baseline, 0x52, 54); a7_state32(baseline, 0x56, 40);
    a7_set_page(baseline, 0, (uint32_t)(uintptr_t)(ram + 128), 12, 20);
    a7_set_page(baseline, 1, (uint32_t)(uintptr_t)(ram + 2048), 0x100, 0x80);
    args[0] = 0; args[1] = (uint32_t)-3; args[2] = 0; args[3] = 24; args[4] = 15;
    args[5] = 1; args[6] = 49; args[7] = 37;
    failures += a7_compare_call("copy_clipped_screen_rectangle (RAM, all clip edges)",
        a7_original_at(0x12f9c, "copy_clipped_screen_rectangle"),
        (void *)copy_clipped_screen_rectangle, 8, args, baseline, 0, 0,
        m_12f9c_set_io_observer, ram, A7_ARENA_BYTES, 0, 0);

    a7_make_state(baseline, 1, 64, 1);
    args[0] = 0; args[1] = 0; args[2] = 1; args[3] = 0; args[4] = 64;
    failures += a7_compare_call("copy_screen_span (planar latch copy)",
        a7_original_at(0x12f9c, "copy_screen_span"), (void *)copy_screen_span,
        5, args, baseline, 0, 0, m_12f9c_set_io_observer, NULL, 0, 0, 1);

    a7_make_state(baseline, 0, 64, 0);
    args[0] = 0; args[1] = 2; args[2] = 3; args[3] = 17; args[4] = 10;
    args[5] = 1; args[6] = 8; args[7] = 12;
    failures += a7_compare_call("copy_clipped_screen_rectangle (VGA, chunky)",
        a7_original_at(0x12f9c, "copy_clipped_screen_rectangle"),
        (void *)copy_clipped_screen_rectangle, 8, args, baseline, 0, 0,
        m_12f9c_set_io_observer, NULL, 0, 0, 1);

    a7_make_state(baseline, 1, 64, 1);
    a7_state32(baseline, 0x4a, 3); a7_state32(baseline, 0x4e, 2);
    a7_state32(baseline, 0x52, 51); a7_state32(baseline, 0x56, 38);
    args[0] = 0; args[1] = 1; args[2] = 1; args[3] = 22; args[4] = 14;
    args[5] = 1; args[6] = 10; args[7] = 12;
    failures += a7_compare_call("copy_clipped_screen_rectangle (VGA, planar)",
        a7_original_at(0x12f9c, "copy_clipped_screen_rectangle"),
        (void *)copy_clipped_screen_rectangle, 8, args, baseline, 0, 0,
        m_12f9c_set_io_observer, NULL, 0, 0, 1);

    a7_make_state(baseline, 0, 64, 0);
    a7_set_page(baseline, 0, (uint32_t)(uintptr_t)(ram + 128), 16, 8);
    a7_set_page(baseline, 1, (uint32_t)(uintptr_t)(ram + 1024), 0x100, 0x40);
    for (unsigned i = 0; i < 8; i++) {
        args[0] = 0; args[1] = rnd(192); args[2] = 1; args[3] = rnd(192);
        args[4] = 1u + rnd(256);
        failures += a7_compare_call("copy_screen_span randomized RAM", a7_original_at(0x12f9c, "copy_screen_span_entry"),
            (void *)copy_screen_span, 5, args, baseline, 0, 0,
            m_12f9c_set_io_observer, ram, A7_ARENA_BYTES, 0, 0);
    }

    a7_make_state(baseline, 0, 64, 0);
    a7_state32(baseline, 0x4a, 4); a7_state32(baseline, 0x4e, 2);
    a7_state32(baseline, 0x52, 54); a7_state32(baseline, 0x56, 40);
    a7_set_page(baseline, 0, (uint32_t)(uintptr_t)(ram + 128), 12, 20);
    a7_set_page(baseline, 1, (uint32_t)(uintptr_t)(ram + 2048), 0x100, 0x80);
    for (unsigned i = 0; i < 8; i++) {
        int32_t left = (int32_t)rnd(52), top = (int32_t)rnd(34);
        args[0] = 0; args[1] = (uint32_t)left; args[2] = (uint32_t)top;
        args[3] = (uint32_t)(left + (int32_t)rnd(19));
        args[4] = (uint32_t)(top + (int32_t)rnd(14));
        args[5] = 1; args[6] = rnd(66); args[7] = rnd(48);
        failures += a7_compare_call("copy_clipped_screen_rectangle randomized RAM",
            a7_original_at(0x12f9c, "copy_clipped_screen_rectangle"),
            (void *)copy_clipped_screen_rectangle, 8, args, baseline, 0, 0,
            m_12f9c_set_io_observer, ram, A7_ARENA_BYTES, 0, 0);
    }

    VirtualFree(ram, 0, MEM_RELEASE);
    return failures;
}

void register_m_12f9c_tests(void)
{
    oracle_register("A7 m_12f9c screen spans and rectangles", test_spans_and_rectangles);
}
