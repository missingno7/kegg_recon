/* test_m_12f30.c - differential tests for chunky scanline planarization. */
#include "test_a7_common.h"

void copy_chunky_scanline_to_vga(uint32_t source, uint32_t destination, uint32_t byte_count);
void copy_chunky_scanline_to_vga_entry(uint32_t source, uint32_t destination, uint32_t byte_count);
void m_12f30_set_io_observer(a7_io_observer observer);

static uint32_t rng_state = 0x12f30a7u;
static uint32_t rnd(uint32_t limit)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return limit ? rng_state % limit : 0;
}

static int test_copy_scanline(void)
{
    unsigned char baseline[A7_STATE_BYTES];
    uint8_t *source = (uint8_t *)VirtualAlloc(NULL, 1024, MEM_COMMIT | MEM_RESERVE,
                                               PAGE_READWRITE);
    uint32_t args[3];
    unsigned i;
    int failures = 0;
    if (!source) {
        printf("    scanline source allocation failed\n");
        return 1;
    }
    for (i = 0; i < 1024; i++)
        source[i] = (uint8_t)(i * 37u + (i >> 2) + 0x59u);
    a7_make_state(baseline, 0, 64, 0);
    args[0] = (uint32_t)(uintptr_t)(source + 13);
    args[1] = 0xa0011u;
    args[2] = 160;
    failures += a7_compare_call("copy_chunky_scanline_to_vga (160 pixels)",
        a7_original_at(0x12f30, "copy_chunky_scanline_to_vga_entry"),
        (void *)copy_chunky_scanline_to_vga_entry, 3, args, baseline, 0, 0,
        m_12f30_set_io_observer, source, 1024, 0, 1);

    args[2] = 43; /* the original processes floor(count/4) samples per plane */
    failures += a7_compare_call("copy_chunky_scanline_to_vga (non-multiple of four)",
        a7_original_at(0x12f30, "copy_chunky_scanline_to_vga"),
        (void *)copy_chunky_scanline_to_vga, 3, args, baseline, 0, 0,
        m_12f30_set_io_observer, source, 1024, 0, 1);
    for (i = 0; i < 6; i++) {
        args[0] = (uint32_t)(uintptr_t)(source + rnd(32));
        args[1] = 0xa0020u + rnd(96);
        args[2] = 4u + rnd(240);
        failures += a7_compare_call("copy_chunky_scanline_to_vga randomized length",
            a7_original_at(0x12f30, "copy_chunky_scanline_to_vga_entry"),
            (void *)copy_chunky_scanline_to_vga_entry, 3, args, baseline, 0, 0,
            m_12f30_set_io_observer, source, 1024, 0, 1);
    }
    VirtualFree(source, 0, MEM_RELEASE);
    return failures;
}

void register_m_12f30_tests(void)
{
    oracle_register("A7 m_12f30 chunky scanline copy", test_copy_scanline);
}
