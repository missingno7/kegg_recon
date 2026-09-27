/* test_vga.c - VGA memory pipeline and CRTC scan-out reference cases. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

int inp(int port);
int outp(int port, int value);
int outpw(int port, int value);
int measure_pit_channel0(void);

#define PM_PLANE_BYTES (4u * 0x10000u)
#define PM_DAC_BYTES (256u * 3u)
#define PM_STATE_BYTES 65536u

extern void vga_oracle_load_snapshot(const uint8_t *plane_bytes, const uint8_t *crtc_bytes,
                                     const uint8_t *dac_bytes, uint16_t display_start,
                                     uint8_t chained, uint8_t map_mask, uint8_t read_map,
                                     uint8_t seq_clocking, uint8_t write_mode,
                                     uint8_t bit_mask, uint8_t misc);

typedef struct PmSnapshot {
    uint8_t planes[PM_PLANE_BYTES];
    uint8_t crtc[32];
    uint8_t dac[PM_DAC_BYTES];
    uint16_t display_start;
    uint8_t chained, map_mask, read_map, seq_clocking, write_mode, bit_mask, misc;
} PmSnapshot;

typedef struct PmFixture {
    const char *name;
    const char *relative_dir;
    int expected_height;
    uint16_t expected_start;
} PmFixture;

static const PmFixture pm_fixtures[] = {
    {"title 320x400", "native-campaign\\strict-iret-20260824\\snapshot.pfpmsnapshot", 400, 0},
    {"game page 0", "native-campaign\\strict-pushf-20260824\\snapshot.pfpmsnapshot", 200, 0},
    {"game page 1", "native-campaign\\vm_hardening_pm_v12.pfreplay-20260823T100941Z\\snapshot.pfpmsnapshot", 200, 0x4000},
    {"menu page 0", "replays-v2\\pmsession_20260802_220505.pfpmsnapshot", 240, 0},
    {"menu page 1", "replays-v2\\pmsession_20260802_225330.pfpmsnapshot", 240, 0x4b00}
};

static int get_fixture_root(char *root, size_t capacity)
{
    DWORD len = GetEnvironmentVariableA("KEGG_FORGED_ARTIFACTS", root, (DWORD)capacity);
    if (len == 0) {
        static const char default_root[] = "D:\\Games\\DOS\\dos_recosystem\\kegg_forged\\artifacts";
        if (sizeof default_root > capacity)
            return 0;
        memcpy(root, default_root, sizeof default_root);
        return 1;
    }
    return len < capacity;
}

static int fixture_state_path(const PmFixture *fixture, char *path, size_t capacity)
{
    char root[MAX_PATH];
    int length;
    if (!get_fixture_root(root, sizeof root))
        return 0;
    length = snprintf(path, capacity, "%s\\%s\\pm_state.json", root,
                      fixture->relative_dir);
    return length > 0 && (size_t)length < capacity;
}

static int json_value(const char *json, const char *key, const char **value)
{
    char needle[80];
    const char *p;
    if (snprintf(needle, sizeof needle, "\"%s\"", key) <= 0)
        return 0;
    p = strstr(json, needle);
    if (!p)
        return 0;
    p = strchr(p + strlen(needle), ':');
    if (!p)
        return 0;
    do { p++; } while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n');
    *value = p;
    return 1;
}

static int json_number(const char *json, const char *key, unsigned long *out)
{
    const char *p;
    char *end;
    if (!json_value(json, key, &p))
        return 0;
    *out = strtoul(p, &end, 0);
    return end != p;
}

static int json_bool(const char *json, const char *key, uint8_t *out)
{
    const char *p;
    if (!json_value(json, key, &p))
        return 0;
    if (strncmp(p, "true", 4) == 0) { *out = 1; return 1; }
    if (strncmp(p, "false", 5) == 0) { *out = 0; return 1; }
    return 0;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int json_hex(const char *json, const char *key, uint8_t *out, size_t count)
{
    const char *p;
    size_t i;
    if (!json_value(json, key, &p) || *p++ != '"')
        return 0;
    for (i = 0; i < count; i++) {
        int hi = hex_value(p[i * 2]), lo = hex_value(p[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return p[count * 2] == '"';
}

static int load_pm_snapshot(const PmFixture *fixture, PmSnapshot *snapshot)
{
    char path[MAX_PATH], state_path[MAX_PATH], planes_path[MAX_PATH];
    char *json = NULL;
    FILE *f;
    long length;
    unsigned long number;
    int state_len, planes_len;
    int ok = 0;
    if (!get_fixture_root(path, sizeof path))
        return 0;
    state_len = snprintf(state_path, sizeof state_path, "%s\\%s\\pm_state.json", path,
                         fixture->relative_dir);
    planes_len = snprintf(planes_path, sizeof planes_path, "%s\\%s\\pm_planes.bin", path,
                          fixture->relative_dir);
    if (state_len <= 0 || (size_t)state_len >= sizeof state_path ||
        planes_len <= 0 || (size_t)planes_len >= sizeof planes_path)
        return 0;
    f = fopen(state_path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0 || (length = ftell(f)) <= 0 ||
        (unsigned long)length > PM_STATE_BYTES || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    json = (char *)malloc((size_t)length + 1);
    if (!json) { fclose(f); return 0; }
    if (fread(json, 1, (size_t)length, f) != (size_t)length) {
        fclose(f); goto done;
    }
    fclose(f);
    json[length] = 0;
    if (!json_hex(json, "crtc_hex", snapshot->crtc, sizeof snapshot->crtc) ||
        !json_hex(json, "dac_hex", snapshot->dac, sizeof snapshot->dac) ||
        !json_bool(json, "chain4", &snapshot->chained) ||
        !json_number(json, "display_start", &number) || number > 0xffffu)
        goto done;
    snapshot->display_start = (uint16_t)number;
    if (!json_number(json, "map_mask", &number) || number > 0xffu) goto done;
    snapshot->map_mask = (uint8_t)number;
    if (!json_number(json, "read_map", &number) || number > 0xffu) goto done;
    snapshot->read_map = (uint8_t)number;
    if (!json_number(json, "sequencer_clocking_mode", &number) || number > 0xffu) goto done;
    snapshot->seq_clocking = (uint8_t)number;
    if (!json_number(json, "write_mode", &number) || number > 0xffu) goto done;
    snapshot->write_mode = (uint8_t)number;
    if (!json_number(json, "bit_mask", &number) || number > 0xffu) goto done;
    snapshot->bit_mask = (uint8_t)number;
    if (!json_number(json, "misc_output", &number) || number > 0xffu) goto done;
    snapshot->misc = (uint8_t)number;
    f = fopen(planes_path, "rb");
    if (!f) { ok = -1; goto done; }
    ok = fread(snapshot->planes, 1, sizeof snapshot->planes, f) == sizeof snapshot->planes &&
         fgetc(f) == EOF;
    fclose(f);
done:
    free(json);
    return ok;
}

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

static int pm_fixture_files_present(void)
{
    char state_path[MAX_PATH], planes_path[MAX_PATH];
    char *slash;
    if (!fixture_state_path(&pm_fixtures[0], state_path, sizeof state_path) ||
        GetFileAttributesA(state_path) == INVALID_FILE_ATTRIBUTES)
        return 0;
    strcpy(planes_path, state_path);
    slash = strrchr(planes_path, '\\');
    if (!slash)
        return 0;
    strcpy(slash + 1, "pm_planes.bin");
    return GetFileAttributesA(planes_path) != INVALID_FILE_ATTRIBUTES;
}

static int test_pm_snapshot_scanout(void)
{
    PmSnapshot snapshot;
    uint8_t pixels[320 * 400];
    int failures = 0, fixture_index;
    for (fixture_index = 0; fixture_index < (int)(sizeof pm_fixtures / sizeof pm_fixtures[0]);
         fixture_index++) {
        const PmFixture *fixture = &pm_fixtures[fixture_index];
        uint8_t palette[256][3];
        uint32_t vde, stride;
        int expected_height, out_w = 0, out_h = 0, x, y, mismatches = 0;
        int result = load_pm_snapshot(fixture, &snapshot);
        if (result != 1) {
            printf("    %s snapshot %s: %s\n", fixture->name, fixture->relative_dir,
                   result == -1 ? "missing pm_state.json/pm_planes.bin" : "invalid data");
            failures++;
            continue;
        }
        vga_oracle_load_snapshot(snapshot.planes, snapshot.crtc, snapshot.dac,
                                 snapshot.display_start, snapshot.chained, snapshot.map_mask,
                                 snapshot.read_map, snapshot.seq_clocking, snapshot.write_mode,
                                 snapshot.bit_mask, snapshot.misc);
        vga_palette_rgb888(palette);
        vde = snapshot.crtc[0x12] | ((uint32_t)(snapshot.crtc[7] & 0x02) << 7) |
              ((uint32_t)(snapshot.crtc[7] & 0x40) << 3);
        expected_height = (int)((vde + 1) / ((snapshot.crtc[9] & 0x1f) + 1));
        stride = (uint32_t)snapshot.crtc[0x13] * 2u;
        if (expected_height != fixture->expected_height ||
            snapshot.display_start != fixture->expected_start || snapshot.chained ||
            !vga_scanout(pixels, 320, 320, 400, &out_w, &out_h) ||
            out_w != 320 || out_h != expected_height) {
            printf("    %s geometry/start: got %dx%d start %04X, CRTC height %d; "
                   "expected 320x%d start %04X\n",
                   fixture->name, out_w, out_h, snapshot.display_start, expected_height,
                   fixture->expected_height, fixture->expected_start);
            failures++;
            continue;
        }
        for (y = 0; y < expected_height; y++) {
            for (x = 0; x < 320; x++) {
                uint32_t plane_offset = ((uint32_t)snapshot.display_start +
                                         (uint32_t)y * stride + (uint32_t)(x >> 2)) & 0xffffu;
                uint8_t expected = snapshot.planes[(size_t)(x & 3) * 0x10000u + plane_offset];
                uint8_t actual = pixels[(size_t)y * 320u + x];
                if (actual != expected) {
                    if (!mismatches)
                        printf("    %s first mismatch at (%d,%d): got %02X expected %02X\n",
                               fixture->name, x, y, actual, expected);
                    mismatches++;
                }
            }
        }
        for (x = 0; x < 256; x++) {
            int component;
            for (component = 0; component < 3; component++) {
                uint8_t value = snapshot.dac[x * 3 + component];
                uint8_t expected = (uint8_t)((value << 2) | (value >> 4));
                if (palette[x][component] != expected) {
                    printf("    %s DAC[%d][%d]: got %02X expected %02X\n", fixture->name,
                           x, component, palette[x][component], expected);
                    mismatches++;
                    break;
                }
            }
        }
        if (mismatches) {
            printf("    %s: %d/%d scan-out pixels differ\n", fixture->name, mismatches,
                   320 * expected_height);
            failures++;
        }
    }
    return failures;
}

static int measure_mode_timing(const char *name, const uint8_t crtc[25], uint8_t misc,
                               double expected_hz, uint32_t expected_pit_ticks)
{
    uint32_t frame_start;
    uint64_t start_ns, elapsed_ns, deadline;
    double measured_hz;
    int sample, i;
    vga_bios_set_mode(0x13);
    crtc_write(0x11, crtc[0x11] & 0x7f); /* unlock CRTC 0..7 before programming */
    for (i = 0; i < 25; i++)
        crtc_write(i, crtc[i]);
    seq_write(1, 1);                    /* 8-dot characters, as in these mode X timings */
    outp(0x3c2, misc);
    if (wait_for_status(0) || wait_for_status(1)) {
        printf("    %s could not synchronize raster measurement to 3DA retrace\n", name);
        return 1;
    }
    frame_start = vga_frame_counter();
    start_ns = ke_now_ns();
    deadline = start_ns + 2000000000ull;
    while (vga_frame_counter() - frame_start < 20 && ke_now_ns() < deadline)
        SwitchToThread();
    elapsed_ns = ke_now_ns() - start_ns;
    if (vga_frame_counter() - frame_start < 20 || !elapsed_ns) {
        printf("    %s retrace counter did not advance\n", name);
        return 1;
    }
    measured_hz = 20.0 * 1e9 / (double)elapsed_ns;
    if (measured_hz < expected_hz * 0.99 || measured_hz > expected_hz * 1.01) {
        printf("    %s retrace rate %.3f Hz expected %.3f Hz\n", name, measured_hz,
               expected_hz);
        return 1;
    }
    sample = measure_pit_channel0();
    if (sample < (int)expected_pit_ticks - 300 || sample > (int)expected_pit_ticks + 300) {
        printf("    %s PIT/vsync calibration %d ticks expected about %u\n", name, sample,
               expected_pit_ticks);
        return 1;
    }
    printf("    %s raster %.3f Hz, PIT calibration %d ticks\n", name, measured_hz, sample);
    return 0;
}

static int test_mode_x_raster_timing(void)
{
    static const uint8_t title_crtc[25] = {
        0x5f,0x4f,0x50,0x82,0x54,0x80,0xbf,0x1f,0x00,0x40,
        0x00,0x00,0x00,0x00,0x00,0x31,0x9c,0x0e,0x8f,0x28,
        0x00,0x96,0xb9,0xe3,0xff
    };
    static const uint8_t menu_crtc[25] = {
        0x5f,0x4f,0x50,0x82,0x54,0x80,0x0b,0x3e,0x00,0x41,
        0x00,0x00,0x4b,0x00,0x00,0x31,0xea,0x0c,0xdf,0x28,
        0x00,0xe7,0x04,0xe3,0xff
    };
    uint8_t game_crtc[25];
    int failures = 0;
    vpit_init();
    memcpy(game_crtc, title_crtc, sizeof game_crtc);
    game_crtc[9] = 0x41;                /* mode X 320x200: two scan lines per row */
    failures += measure_mode_timing("320x400", title_crtc, 0x63, 70.0863, 17024);
    failures += measure_mode_timing("320x240", menu_crtc, 0xe3, 59.94, 19904);
    failures += measure_mode_timing("320x200", game_crtc, 0x63, 70.0863, 17024);
    return failures;
}

void register_vga_tests(void)
{
    oracle_register("vga read/write mode reference table", test_read_write_modes);
    oracle_register("vga original video_mode_table scan-out", test_mode_table_scanout);
    oracle_register("vga panning, line compare, maximum scan line", test_panning_split_and_scanline);
    oracle_register("vga display start latched at vertical retrace", test_retrace_start_latch);
    oracle_register("vga mode X VT/VDE raster and PIT calibration", test_mode_x_raster_timing);
    if (pm_fixture_files_present())
        oracle_register("vga kegg_forged snapshot scan-out (three modes and page flips)",
                        test_pm_snapshot_scanout);
    else
        printf("SKIP  vga kegg_forged snapshot scan-out (set KEGG_FORGED_ARTIFACTS when available)\n");
}
