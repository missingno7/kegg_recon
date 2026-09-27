/* test_m_13324.c - differential tests for VGA pixels and spans. */
#include "test_a7_common.h"

uint32_t read_vga_pixel(uint32_t x, uint32_t y);
uint32_t read_vga_pixel_entry(uint32_t x, uint32_t y);
void write_vga_pixel(uint32_t x, uint32_t y, uint32_t color);
void write_vga_pixel_entry(uint32_t x, uint32_t y, uint32_t color);
void fill_vga_span(uint32_t page, uint32_t x, uint32_t length, uint32_t color);
void fill_vga_span_entry(uint32_t page, uint32_t x, uint32_t length, uint32_t color);
void m_13324_set_io_observer(a7_io_observer observer);

static uint32_t rng_state = 0x13324a7u;
static uint32_t rnd(uint32_t limit)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return limit ? rng_state % limit : 0;
}

static int test_pixels_and_spans(void)
{
    unsigned char baseline[A7_STATE_BYTES];
    static const uint8_t fill_span_signature[] = {
        0x60, 0x8d, 0x6c, 0x24, 0x1c, 0x83, 0x7d, 0x10, 0x00
    };
    uint8_t *ram = (uint8_t *)VirtualAlloc(NULL, A7_ARENA_BYTES,
                                          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    uint32_t args[4];
    unsigned i;
    int failures = 0;
    if (!ram) {
        printf("    A7 RAM allocation failed\n");
        return 1;
    }
    memset(ram, 0x5a, A7_ARENA_BYTES);

    a7_make_state(baseline, 0, 64, 0);
    a7_set_page(baseline, 0, (uint32_t)(uintptr_t)(ram + 128), 0, 0);
    args[0] = 7; args[1] = 5; args[2] = 0x1a5;
    failures += a7_compare_call("write_vga_pixel (RAM)", a7_original_at(0x13324, "write_vga_pixel_entry"),
        (void *)write_vga_pixel, 3, args, baseline, 0, 0, m_13324_set_io_observer,
        ram, A7_ARENA_BYTES, 0, 0);

    args[0] = 9; args[1] = 4;
    failures += a7_compare_call("read_vga_pixel (RAM, clamped EAX)", a7_original_at(0x13324, "read_vga_pixel_entry"),
        (void *)read_vga_pixel_entry, 2, args, baseline, 0, 0, m_13324_set_io_observer,
        ram, A7_ARENA_BYTES, 1, 0);

    args[0] = 9; args[1] = 4;
    failures += a7_compare_call("read_vga_pixel (unchained VGA plane)", a7_original_at(0x13324, "read_vga_pixel_entry"),
        (void *)read_vga_pixel_entry, 2, args, baseline, 0, 0, m_13324_set_io_observer,
        NULL, 0, 1, 1);

    args[0] = 0; args[1] = 23; args[2] = 37; args[3] = 0x83;
    failures += a7_compare_call("fill_vga_span (RAM, tail groups)",
        a7_find_code(0x13324, 0x13712, fill_span_signature, sizeof fill_span_signature),
        (void *)fill_vga_span_entry, 4, args, baseline, 0, 0, m_13324_set_io_observer,
        ram, A7_ARENA_BYTES, 0, 0);

    a7_make_state(baseline, 1, 64, 1);
    /* Keep this control case RAM-backed (4 * pointer); the following planar VGA case uses
     * the relocated no-access alias and exercises the original LODSB fault path. */
    a7_set_page(baseline, 0, (uint32_t)(uintptr_t)(ram + 512) << 2, 0, 0);
    args[0] = 11; args[1] = 8;
    failures += a7_compare_call("read_vga_pixel (planar plane select)", a7_original_at(0x13324, "read_vga_pixel"),
        (void *)read_vga_pixel, 2, args, baseline, 0, 0, m_13324_set_io_observer,
        ram, A7_ARENA_BYTES, 1, 0);

    a7_make_state(baseline, 1, 64, 1);
    failures += a7_compare_call("read_vga_pixel (planar VGA plane)", a7_original_at(0x13324, "read_vga_pixel"),
        (void *)read_vga_pixel, 2, args, baseline, 0, 0, m_13324_set_io_observer,
        NULL, 0, 1, 1);

    a7_make_state(baseline, 1, 64, 1);
    args[0] = 11; args[1] = 8; args[2] = 0xe3;
    failures += a7_compare_call("write_vga_pixel (planar plane select)", a7_original_at(0x13324, "write_vga_pixel_entry"),
        (void *)write_vga_pixel_entry, 3, args, baseline, 0, 0, m_13324_set_io_observer,
        NULL, 0, 0, 1);

    args[0] = 0; args[1] = 7; args[2] = 43; args[3] = 0x96;
    failures += a7_compare_call("fill_vga_span (planar groups)",
        a7_find_code(0x13324, 0x13712, fill_span_signature, sizeof fill_span_signature),
        (void *)fill_vga_span, 4, args, baseline, 0, 0, m_13324_set_io_observer,
        NULL, 0, 0, 1);

    a7_make_state(baseline, 1, 64, 1);
    a7_set_page(baseline, 0, (uint32_t)(uintptr_t)(ram + 512) << 2, 0, 0);
    for (i = 0; i < 8; i++) {
        args[0] = rnd(82); args[1] = rnd(82); args[2] = rnd(256);
        failures += a7_compare_call("write_vga_pixel randomized RAM plane", a7_original_at(0x13324, "write_vga_pixel_entry"),
            (void *)write_vga_pixel, 3, args, baseline, 0, 0, m_13324_set_io_observer,
            ram, A7_ARENA_BYTES, 0, 0);
        args[0] = rnd(82); args[1] = rnd(82);
        failures += a7_compare_call("read_vga_pixel randomized RAM plane", a7_original_at(0x13324, "read_vga_pixel_entry"),
            (void *)read_vga_pixel, 2, args, baseline, 0, 0, m_13324_set_io_observer,
            ram, A7_ARENA_BYTES, 1, 0);
        args[0] = 0; args[1] = rnd(80); args[2] = 4u + rnd(120); args[3] = rnd(256);
        failures += a7_compare_call("fill_vga_span randomized RAM plane", a7_find_code(0x13324, 0x13712, fill_span_signature, sizeof fill_span_signature),
            (void *)fill_vga_span, 4, args, baseline, 0, 0, m_13324_set_io_observer,
            ram, A7_ARENA_BYTES, 0, 0);
    }

    VirtualFree(ram, 0, MEM_RELEASE);
    return failures;
}

void register_m_13324_tests(void)
{
    oracle_register("A7 m_13324 pixels and spans", test_pixels_and_spans);
}
