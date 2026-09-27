/* test_vga.c - VGA memory pipeline and CRTC scan-out reference cases. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

int inp(int port);
int outp(int port, int value);
int outpw(int port, int value);

#pragma pack(push, 1)
typedef struct TestVideoModeInfo {
    int32_t mode_id;
    int16_t bios_mode_number;
    int16_t reserved;
    union { int32_t raw; int16_t word; uint8_t bytes[4]; } bios_mode_data;
    uint8_t mode_flags;
    uint8_t storage_class;
    int32_t pixel_width;
    int32_t pixel_height;
    int32_t scanline_bytes;
    int32_t image_bytes;
    int32_t page_buffer_bytes;
} TestVideoModeInfo;

typedef struct TestVgaRegisterPreset {
    uint8_t restore_groups;
    uint8_t reserved[3];
    uint8_t register_bytes[48];
    int32_t mode_id;
} TestVgaRegisterPreset;
#pragma pack(pop)
_Static_assert(sizeof(TestVideoModeInfo) == 34, "historical packed video mode layout");
_Static_assert(sizeof(TestVgaRegisterPreset) == 56, "historical packed VGA preset layout");

typedef struct VgaWriteReference {
    uint8_t mode, cpu, set_reset, enable_set_reset, rotate, operation;
    uint8_t bit_mask, plane_mask;
    uint8_t before[4], latch[4], expected[4];
} VgaWriteReference;

static const VgaWriteReference write_reference[] = {
    {0, 0x96, 0x05, 0x02, 2, 2, 0x3c, 0x0f,
     {0x12, 0x34, 0x56, 0x78}, {0x12, 0x34, 0x56, 0x78}, {0x36, 0x34, 0x76, 0x7c}},
    {1, 0x00, 0x00, 0x00, 0, 0, 0x00, 0x05,
     {0xaa, 0xbb, 0xcc, 0xdd}, {0x11, 0x22, 0x33, 0x44}, {0x11, 0xbb, 0x33, 0xdd}},
    {2, 0x05, 0x00, 0x00, 0, 3, 0x0f, 0x0f,
     {0x0f, 0xf0, 0x55, 0xaa}, {0x0f, 0xf0, 0x55, 0xaa}, {0x00, 0xf0, 0x5a, 0xaa}},
    {3, 0xf0, 0x05, 0x00, 4, 0, 0x7f, 0x0f,
     {0xaa, 0x55, 0x0f, 0xf0}, {0xaa, 0x55, 0x0f, 0xf0}, {0xaf, 0x50, 0x0f, 0xf0}}
};

static void indexed_write(int index_port, int index, int value)
{
    outpw(index_port, ((value & 0xff) << 8) | (index & 0xff));
}

static void crtc_write(int index, int value)
{
    indexed_write(0x3d4, index, value);
}

static void seq_write(int index, int value)
{
    indexed_write(0x3c4, index, value);
}

static void gc_write(int index, int value)
{
    indexed_write(0x3ce, index, value);
}

static void attr_write(int index, int value)
{
    (void)inp(0x3da);                       /* attribute index/data flip-flop -> index */
    outp(0x3c0, 0x20 | (index & 0x1f));
    outp(0x3c0, value & 0xff);
}

static uint8_t gc_read(int index)
{
    gc_write(4, index);
    return (uint8_t)inp(0x3cf);
}

static void set_planar_defaults(void)
{
    vga_bios_set_mode(0x13);
    seq_write(4, 0x06);                    /* unchained, odd/even disabled */
    seq_write(2, 0x0f);
    gc_write(0, 0);
    gc_write(1, 0);
    gc_write(2, 0);
    gc_write(3, 0);
    gc_write(4, 0);
    gc_write(5, 0);
    gc_write(6, 0x05);                    /* A0000h-AFFFFh graphics aperture */
    gc_write(7, 0x0f);
    gc_write(8, 0xff);
}

static void write_plane_byte(uint32_t offset, int plane, uint8_t value)
{
    seq_write(2, 1 << plane);
    vga_mem_write8(0xa0000u + offset, value);
}

static int test_read_write_modes(void)
{
    static const uint8_t initial[4] = {0xf0, 0xcc, 0xaa, 0x0f};
    static const struct { uint8_t color, care, expected; } read_reference[] = {
        {0x00, 0x0f, 0x00},
        {0x00, 0x03, 0x03}
    };
    int failures = 0, p, i;
    set_planar_defaults();
    for (p = 0; p < 4; p++)
        write_plane_byte(0x20, p, initial[p]);
    seq_write(2, 0x0f);

    gc_write(5, 0);                        /* read mode 0 */
    for (p = 0; p < 4; p++) {
        uint8_t got;
        gc_write(4, p);
        got = vga_mem_read8(0xa0020u);
        if (got != initial[p]) {
            printf("    read mode 0 plane %d: got %02X expected %02X\n", p, got, initial[p]);
            failures++;
        }
    }
    gc_write(5, 8);                        /* read mode 1: color compare */
    for (i = 0; i < (int)(sizeof read_reference / sizeof read_reference[0]); i++) {
        uint8_t got;
        gc_write(2, read_reference[i].color);
        gc_write(7, read_reference[i].care);
        got = vga_mem_read8(0xa0020u);
        if (got != read_reference[i].expected) {
            printf("    read mode 1 case %d: got %02X expected %02X\n", i, got,
                   read_reference[i].expected);
            failures++;
        }
    }

    for (i = 0; i < (int)(sizeof write_reference / sizeof write_reference[0]); i++) {
        const VgaWriteReference *r = &write_reference[i];
        uint8_t got[4];
        set_planar_defaults();
        for (p = 0; p < 4; p++) {
            write_plane_byte(0x100, p, r->before[p]);
            write_plane_byte(0x200, p, r->latch[p]);
        }
        gc_write(0, r->set_reset);
        gc_write(1, r->enable_set_reset);
        gc_write(2, 0);
        gc_write(3, (r->operation << 3) | r->rotate);
        gc_write(5, r->mode);
        gc_write(7, 0x0f);
        gc_write(8, r->bit_mask);
        seq_write(2, r->plane_mask);
        (void)vga_mem_read8(0xa0200u);      /* load the reference latch bytes */
        vga_mem_write8(0xa0100u, r->cpu);
        gc_write(5, 0);                    /* inspect with ordinary read mode 0 */
        for (p = 0; p < 4; p++) {
            (void)gc_read(p);
            got[p] = vga_mem_read8(0xa0100u);
            if (got[p] != r->expected[p]) {
                printf("    write mode %u plane %d: got %02X expected %02X\n",
                       r->mode, p, got[p], r->expected[p]);
                failures++;
            }
        }
    }
    return failures;
}

static void program_preset(const TestVgaRegisterPreset *preset, int chained)
{
    int i;
    vga_bios_set_mode(0x13);
    crtc_write(0x11, preset->register_bytes[0x11] & 0x7f);
    for (i = 0; i <= 0x18; i++)
        crtc_write(i, preset->register_bytes[i]);
    for (i = 0; i <= 4; i++)
        seq_write(i, preset->register_bytes[0x19 + i]);
    for (i = 0x10; i <= 0x14; i++)
        attr_write(i, preset->register_bytes[0x1e + i - 0x10]);
    for (i = 0; i <= 8; i++)
        seq_write(i, preset->register_bytes[0x23 + i]);
    outp(0x3c2, preset->register_bytes[0x2c]);
    gc_write(0, 0);
    gc_write(1, 0);
    gc_write(2, 0);
    gc_write(3, 0);
    gc_write(4, 0);
    gc_write(5, 0x40);                    /* 256-color shift mode */
    gc_write(6, 0x05);                    /* A0000h-AFFFFh graphics aperture */
    gc_write(7, 0x0f);
    gc_write(8, 0xff);
    seq_write(2, 0x0f);
    seq_write(4, (preset->register_bytes[0x27] & (uint8_t)~0x08) | (chained ? 0x08 : 0));
}

static int test_mode_table_scanout(void)
{
    static const int mode_ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t pixels[400 * 480];
    int failures = 0, mode_index;
    const TestVideoModeInfo *modes = (const TestVideoModeInfo *)oracle_sym("video_mode_table");
    const TestVgaRegisterPreset *presets =
        (const TestVgaRegisterPreset *)oracle_sym("vga_register_presets");
    if (!modes || !presets) {
        printf("    original video mode data symbols are unavailable\n");
        return 1;
    }
    for (mode_index = 0; mode_index < (int)(sizeof mode_ids / sizeof mode_ids[0]); mode_index++) {
        const TestVideoModeInfo *mode = NULL;
        const TestVgaRegisterPreset *preset = NULL;
        int i, chained;
        for (i = 0; i < 16 && modes[i].mode_id != -1; i++)
            if (modes[i].mode_id == mode_ids[mode_index]) mode = &modes[i];
        for (i = 0; i < 14 && presets[i].mode_id != -1; i++)
            if (presets[i].mode_id == mode_ids[mode_index]) preset = &presets[i];
        if (!mode || !preset) {
            printf("    original mode/preset missing for id %d\n", mode_ids[mode_index]);
            failures++;
            continue;
        }
        for (chained = 0; chained <= 1; chained++) {
            int w = 0, h = 0, x;
            program_preset(preset, chained);
            /* A row of distinct pixels verifies both packed and planar scan-out paths. */
            if (chained) {
                for (x = 0; x < mode->pixel_width; x++)
                    vga_mem_write8(0xa0000u + (uint32_t)x, (uint8_t)(x * 13 + mode->mode_id));
            } else {
                for (x = 0; x < mode->pixel_width; x++) {
                    seq_write(2, 1 << (x & 3));
                    vga_mem_write8(0xa0000u + (uint32_t)(x >> 2),
                                   (uint8_t)(x * 13 + mode->mode_id));
                }
                seq_write(2, 0x0f);
            }
            if (!vga_scanout(pixels, 400, 400, 480, &w, &h) ||
                w != mode->pixel_width || h != mode->pixel_height) {
                printf("    mode %d %s: got %dx%d expected %dx%d\n", mode->mode_id,
                       chained ? "chained" : "unchained", w, h,
                       mode->pixel_width, mode->pixel_height);
                failures++;
                continue;
            }
            for (x = 0; x < mode->pixel_width; x++) {
                uint8_t expected = (uint8_t)(x * 13 + mode->mode_id);
                if (pixels[x] != expected) {
                    printf("    mode %d %s pixel %d: got %02X expected %02X\n",
                           mode->mode_id, chained ? "chained" : "unchained", x,
                           pixels[x], expected);
                    failures++;
                    break;
                }
            }
        }
    }
    return failures;
}

static void configure_small_mode(int chained)
{
    int i;
    vga_bios_set_mode(0x13);
    crtc_write(0x11, 0x0e);               /* unlock CRTC registers 0..7 */
    crtc_write(0x01, 3);                  /* 16 pixels */
    crtc_write(0x06, 0xbf);
    crtc_write(0x07, 0x01);               /* 449-line frame; low display/retrace fields */
    crtc_write(0x09, 0);                  /* one scan per character row */
    crtc_write(0x10, 8);                  /* vertical retrace starts at line 8 */
    crtc_write(0x11, 0x0a);               /* two-line pulse; unlocked */
    crtc_write(0x12, 7);                  /* eight displayed lines */
    crtc_write(0x13, 2);                  /* 16 stream bytes per line */
    crtc_write(0x14, 0x40);               /* doubleword addressing */
    crtc_write(0x17, 0x40);
    crtc_write(0x18, 0xff);               /* line compare disabled */
    seq_write(4, chained ? 0x0e : 0x06);
    seq_write(2, 0x0f);
    gc_write(0, 0); gc_write(1, 0); gc_write(2, 0); gc_write(3, 0);
    gc_write(4, 0); gc_write(5, 0x40); gc_write(6, 0x05);
    gc_write(7, 0x0f); gc_write(8, 0xff);
    attr_write(0x10, 0x41);
    attr_write(0x13, 0);
    for (i = 0; i < 8 * 16; i++)
        vga_mem_write8(0xa0000u + (uint32_t)i, (uint8_t)(i + 1));
}

static int test_panning_split_and_scanline(void)
{
    uint8_t pixels[400 * 480];
    int w = 0, h = 0, failures = 0;
    configure_small_mode(1);
    if (!vga_scanout(pixels, 400, 400, 480, &w, &h) || w != 16 || h != 8 || pixels[0] != 1)
        failures++;
    attr_write(0x13, 4);                  /* 256-color pel pan value 4 -> two pixels */
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (pixels[0] != 3 || pixels[1] != 4)
        failures++;
    crtc_write(8, 0x20);                  /* byte pan adds one dword (four pixels) */
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (pixels[0] != 7 || pixels[1] != 8)
        failures++;
    crtc_write(8, 0);
    attr_write(0x10, 0x61);               /* line compare resets bottom-window panning */
    crtc_write(0x18, 3);
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (pixels[0] != 3 || pixels[4 * 400] != 1)
        failures++;

    configure_small_mode(1);
    crtc_write(8, 1);                     /* preset row scan advances by one scan line */
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (!w || pixels[0] != 17)
        failures++;
    configure_small_mode(1);
    crtc_write(0x09, 1);                  /* two scan lines per character row */
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (!w || w != 16 || h != 4 || pixels[400] != 17)
        failures++;
    crtc_write(0x09, 0x81);               /* plus CRTC double-scan: four scan lines */
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (!w || w != 16 || h != 2 || pixels[400] != 17)
        failures++;
    if (failures)
        printf("    panning/split/max-scan reference checks: %d failure(s)\n", failures);
    return failures;
}

static int wait_for_status(int set)
{
    uint64_t deadline = ke_now_ns() + 2000000000ull;
    while (!!(inp(0x3da) & 8) != set) {
        if (ke_now_ns() >= deadline)
            return -1;
        SwitchToThread();
    }
    return 0;
}

static int test_retrace_start_latch(void)
{
    uint8_t pixels[400 * 480];
    uint32_t frame;
    uint64_t deadline;
    int w = 0, h = 0, failures = 0, i;
    configure_small_mode(1);
    crtc_write(0x10, 8);
    crtc_write(0x11, 0x00);               /* eight-line retrace pulse, unlocked */
    for (i = 0; i < 16; i++) {
        vga_mem_write8(0xa0000u + (uint32_t)i, 0x11);
        vga_mem_write8(0xa0040u + (uint32_t)i, 0x77);
    }
    if (wait_for_status(0) || wait_for_status(1) || wait_for_status(0)) {
        printf("    could not observe a complete vertical retrace pulse\n");
        return 1;
    }
    frame = vga_frame_counter();
    crtc_write(0x0c, 0);
    crtc_write(0x0d, 16);                /* selects the second pixel row at next retrace */
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (pixels[0] != 0x11) {
        printf("    display start changed before retrace: %02X\n", pixels[0]);
        failures++;
    }
    deadline = ke_now_ns() + 2000000000ull;
    while (vga_frame_counter() == frame && ke_now_ns() < deadline)
        SwitchToThread();
    if (vga_frame_counter() == frame) {
        printf("    vertical retrace counter did not advance\n");
        failures++;
    }
    vga_scanout(pixels, 400, 400, 480, &w, &h);
    if (pixels[0] != 0x77) {
        printf("    display start was not latched at retrace: got %02X expected 77\n", pixels[0]);
        failures++;
    }
    return failures;
}

void register_vga_tests(void)
{
    oracle_register("vga read/write mode reference table", test_read_write_modes);
    oracle_register("vga original video_mode_table scan-out", test_mode_table_scanout);
    oracle_register("vga panning, line compare, maximum scan line", test_panning_split_and_scanline);
    oracle_register("vga display start latched at vertical retrace", test_retrace_start_latch);
}
