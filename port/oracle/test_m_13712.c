/* test_m_13712.c - differential coverage for the planar-row helper via its original tail jump. */
#include "test_a7_common.h"

void fill_clipped_vga_rectangle(uint32_t page, uint32_t left, uint32_t top,
                                uint32_t right, uint32_t bottom, uint32_t color);
void m_13324_set_io_observer(a7_io_observer observer);
void m_13712_set_io_observer(a7_io_observer observer);

static void set_planar_observers(a7_io_observer observer)
{
    m_13324_set_io_observer(observer);
    m_13712_set_io_observer(observer);
}

static uint32_t rng_state = 0x13712a7u;
static uint32_t rnd(uint32_t n)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return n ? rng_state % n : 0;
}

static int test_planar_rows(void)
{
    unsigned char baseline[A7_STATE_BYTES];
    uint32_t args[6];
    unsigned i;
    int failures = 0;
    void *original = a7_original_at(0x13324, "fill_clipped_vga_rectangle");
    a7_make_state(baseline, 0, 64, 0);
    args[0] = 0; args[1] = 5; args[2] = 3; args[3] = 19; args[4] = 9; args[5] = 0x6d;
    failures += a7_compare_call("fill_planar_video_rows via clipped rectangle", original,
        (void *)fill_clipped_vga_rectangle, 6, args, baseline, 0, 0,
        set_planar_observers, NULL, 0, 0, 1);

    /* Fuzz ordering and clipping at each edge; original and C tail-jump adapter share the
     * inherited width/height frame semantics through fill_clipped_vga_rectangle. */
    for (i = 0; i < 12; i++) {
        int32_t x0 = (int32_t)rnd(90) - 12, y0 = (int32_t)rnd(88) - 10;
        int32_t x1 = x0 + (int32_t)rnd(28) - 5, y1 = y0 + (int32_t)rnd(22) - 4;
        args[0] = 0; args[1] = (uint32_t)x0; args[2] = (uint32_t)y0;
        args[3] = (uint32_t)x1; args[4] = (uint32_t)y1; args[5] = rnd(256);
        failures += a7_compare_call("fill_planar_video_rows randomized clip", original,
            (void *)fill_clipped_vga_rectangle, 6, args, baseline, 0, 0,
            set_planar_observers, NULL, 0, 0, 1);
    }

    a7_make_state(baseline, 1, 64, 1);
    args[0] = 0; args[1] = 4; args[2] = 6; args[3] = 23; args[4] = 11; args[5] = 0x2b;
    failures += a7_compare_call("fill_clipped_vga_rectangle planar addressing", original,
        (void *)fill_clipped_vga_rectangle, 6, args, baseline, 0, 0,
        set_planar_observers, NULL, 0, 0, 1);
    return failures;
}

void register_m_13712_tests(void)
{
    oracle_register("A7 m_13712 planar rows and clipped rectangle", test_planar_rows);
}
