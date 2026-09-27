/* test_m_0a284.c - differential tests for asm/m_0a284_0a51f.asm. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"

#define FILE_BYTES 8192u
#define PIXEL_BYTES 8192u
#define BODY_BYTES 2048u
/* The manifest has the six data labels but omits the private TASM code labels. */
#define M_0A284_ENTRY_OFFSET 0x0A284u
#define IFF_PIXEL_COUNT_SEED 0x2468ACE0u

typedef struct IffImageLayout {
    uint32_t pixel_buffer;
    uint32_t palette_buffer;
    uint32_t width_pixels;
    uint32_t height_pixels;
    uint32_t format_code;
    uint32_t palette_entries;
    uint32_t file_byte_count;
    uint32_t palette_byte_count;
    uint32_t pixel_count;
} IffImageLayout;

extern uint32_t g_iff_error;
extern uint32_t g_iff_file;
extern uint32_t g_iff_pixels;
extern uint32_t g_iff_pixel_count;
extern uint32_t g_iff_palette_bytes;
extern uint32_t g_iff_decoder_workspace[5];
extern int iff_width_pixels;
extern int iff_height_pixels;
extern int iff_output_byte_count;
extern int iff_decoded_pixel_count;
uint32_t decode_iff_ilbm_image(uint32_t file, uint32_t pixels, IffImageLayout *image);
uint32_t find_iff_chunk(uint32_t chunk_id);

static uint32_t *o_error, *o_file, *o_pixels, *o_pixel_count, *o_palette_bytes;
static uint32_t *o_workspace;
static int *o_width, *o_height, *o_output_count, *o_decoded_count;

typedef struct Fixture {
    uint8_t data[FILE_BYTES];
    uint32_t size;
} Fixture;

static uint32_t rng_state = 0xA2C28451u;
static uint32_t random_u32(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static void put_be16(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void put_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint32_t append_chunk(uint8_t *out, uint32_t offset, const char id[4],
                             const uint8_t *payload, uint32_t size)
{
    memcpy(out + offset, id, 4);
    put_be32(out + offset + 4, size);
    if (size)
        memcpy(out + offset + 8, payload, size);
    offset += 8 + size;
    if (size & 1u)
        out[offset++] = 0;
    return offset;
}

static uint32_t encode_byterun1(const uint8_t *pixels, uint32_t size, uint8_t *out)
{
    uint32_t in = 0, used = 0;
    out[used++] = 0x80;                         /* Exercise the ByteRun1 no-op control. */
    while (in < size) {
        uint32_t run = 1;
        while (in + run < size && pixels[in + run] == pixels[in] && run < 128)
            run++;
        if (run >= 3) {
            out[used++] = (uint8_t)(257u - run);
            out[used++] = pixels[in];
            in += run;
        } else {
            uint32_t begin = in;
            in += run;
            while (in < size && in - begin < 128) {
                run = 1;
                while (in + run < size && pixels[in + run] == pixels[in] && run < 3)
                    run++;
                if (run >= 3)
                    break;
                if (in - begin + run > 128)
                    break;
                in += run;
            }
            out[used++] = (uint8_t)(in - begin - 1);
            memcpy(out + used, pixels + begin, in - begin);
            used += in - begin;
        }
    }
    return used;
}

static void make_fixture(Fixture *fixture, uint32_t width, uint32_t height,
                         int compression, int include_body, int include_header,
                         int include_palette, int invalid_form, const uint8_t *body_override,
                         uint32_t body_override_size)
{
    uint8_t header[20] = {0};
    uint8_t body[BODY_BYTES];
    uint8_t palette[768];
    uint32_t pixel_count = width * height;
    uint32_t body_size = 0, offset = 12, i;
    memset(fixture->data, 0, sizeof fixture->data);
    memcpy(fixture->data, invalid_form ? "NOPE" : "FORM", 4);
    memcpy(fixture->data + 8, "ILBM", 4);

    put_be16(header, width);
    put_be16(header + 2, height);
    header[10] = (uint8_t)compression;
    if (include_header)
        offset = append_chunk(fixture->data, offset, "BMHD", header, sizeof header);
    if (include_palette) {
        for (i = 0; i < sizeof palette; i++)
            palette[i] = (uint8_t)((i * 37u + width * 11u + height * 7u) & 0xFFu);
        offset = append_chunk(fixture->data, offset, "CMAP", palette, sizeof palette);
    }
    if (include_body) {
        if (body_override) {
            memcpy(body, body_override, body_override_size);
            body_size = body_override_size;
        } else {
            for (i = 0; i < pixel_count; i++) {
                uint32_t value = random_u32();
                body[i] = (uint8_t)((i % 29u < 4u) ? 0x55u : (value >> 19));
            }
            if (compression)
                body_size = encode_byterun1(body, pixel_count, body + 512);
            else
                body_size = pixel_count;
            if (compression) {
                memmove(body, body + 512, body_size);
            }
        }
        offset = append_chunk(fixture->data, offset, "BODY", body, body_size);
    }
    put_be32(fixture->data + 4, offset - 8);
    fixture->size = offset;
}

static void seed_state(uint32_t error_seed)
{
    static const uint32_t workspace_seed[5] = {
        0xAAAABBBB, 0xCCCCDDDD, 0xEEEEFFFF, 0x12345678, 0x87654321
    };
    unsigned i;
    *o_error = g_iff_error = error_seed;
    *o_file = g_iff_file = 0;
    *o_pixels = g_iff_pixels = 0;
    *o_pixel_count = g_iff_pixel_count = IFF_PIXEL_COUNT_SEED;
    *o_palette_bytes = g_iff_palette_bytes = 0x11223344u;
    for (i = 0; i < 5; i++)
        o_workspace[i] = g_iff_decoder_workspace[i] = workspace_seed[i];
    *o_width = iff_width_pixels = 0x1234;
    *o_height = iff_height_pixels = 0x4321;
    *o_output_count = iff_output_byte_count = 0x24681357;
    *o_decoded_count = iff_decoded_pixel_count = 0x76543210;
}

static int compare_case(const char *name, const Fixture *fixture, uint32_t error_seed,
                        uint32_t expected_status, int check_output_count)
{
    uint8_t *file_original = (uint8_t *)VirtualAlloc(NULL, FILE_BYTES, MEM_RESERVE | MEM_COMMIT,
                                                     PAGE_READWRITE);
    uint8_t *file_port = (uint8_t *)VirtualAlloc(NULL, FILE_BYTES, MEM_RESERVE | MEM_COMMIT,
                                                 PAGE_READWRITE);
    uint8_t *pixels_original = (uint8_t *)VirtualAlloc(NULL, PIXEL_BYTES,
                                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t *pixels_port = (uint8_t *)VirtualAlloc(NULL, PIXEL_BYTES,
                                                   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    IffImageLayout original_image, port_image;
    uint32_t args[3], original_status, port_status;
    uint32_t original_count_offset = 0, port_count_offset = 0;
    int failures = 0;
    uint32_t i;

    if (!file_original || !file_port || !pixels_original || !pixels_port) {
        printf("    %s: VirtualAlloc failed\n", name);
        failures++;
        goto done;
    }
    memset(file_original, 0, FILE_BYTES);
    memset(file_port, 0, FILE_BYTES);
    memcpy(file_original, fixture->data, fixture->size);
    memcpy(file_port, fixture->data, fixture->size);
    for (i = 0; i < PIXEL_BYTES; i++)
        pixels_original[i] = pixels_port[i] = (uint8_t)(0x5Du + i * 13u);
    memset(&original_image, 0xCC, sizeof original_image);
    memset(&port_image, 0xCC, sizeof port_image);
    seed_state(error_seed);

    args[0] = (uint32_t)(uintptr_t)file_original;
    args[1] = (uint32_t)(uintptr_t)pixels_original;
    args[2] = (uint32_t)(uintptr_t)&original_image;
    original_status = oracle_call((uint8_t *)oracle_object_base(1) + M_0A284_ENTRY_OFFSET,
                                  3, args);
    port_status = decode_iff_ilbm_image((uint32_t)(uintptr_t)file_port,
                                        (uint32_t)(uintptr_t)pixels_port, &port_image);

    if (original_status != expected_status || port_status != expected_status) {
        printf("    %s: status expected %08X, original %08X, port %08X\n",
               name, expected_status, original_status, port_status);
        failures++;
    }
    if (memcmp(file_original, file_port, FILE_BYTES) != 0) {
        printf("    %s: file buffers differ\n", name);
        failures++;
    }
    if (memcmp(pixels_original, pixels_port, PIXEL_BYTES) != 0) {
        printf("    %s: pixel buffers differ\n", name);
        failures++;
    }
    if (*o_error != g_iff_error || *o_pixel_count != g_iff_pixel_count ||
        *o_palette_bytes != g_iff_palette_bytes || *o_width != iff_width_pixels ||
        *o_height != iff_height_pixels || *o_decoded_count != iff_decoded_pixel_count) {
        printf("    %s: decoder globals differ\n", name);
        failures++;
    }
    if (*o_file - (uint32_t)(uintptr_t)file_original !=
            g_iff_file - (uint32_t)(uintptr_t)file_port ||
        *o_pixels - (uint32_t)(uintptr_t)pixels_original !=
            g_iff_pixels - (uint32_t)(uintptr_t)pixels_port) {
        printf("    %s: published input pointers differ\n", name);
        failures++;
    }
    for (i = 0; i < 5; i++) {
        uint32_t original_value = o_workspace[i];
        uint32_t port_value = g_iff_decoder_workspace[i];
        if (i == 0 && *o_pixel_count != IFF_PIXEL_COUNT_SEED) {
            original_value -= (uint32_t)(uintptr_t)pixels_original;
            port_value -= (uint32_t)(uintptr_t)pixels_port;
        }
        if (original_value != port_value) {
            printf("    %s: workspace[%u] differs (%08X vs %08X)\n",
                   name, i, original_value, port_value);
            failures++;
            break;
        }
    }
    if (check_output_count) {
        original_count_offset = (uint32_t)*o_output_count;
        port_count_offset = (uint32_t)iff_output_byte_count;
        if (original_count_offset >= (uint32_t)(uintptr_t)pixels_original &&
            original_count_offset < (uint32_t)(uintptr_t)pixels_original + PIXEL_BYTES)
            original_count_offset -= (uint32_t)(uintptr_t)pixels_original;
        if (port_count_offset >= (uint32_t)(uintptr_t)pixels_port &&
            port_count_offset < (uint32_t)(uintptr_t)pixels_port + PIXEL_BYTES)
            port_count_offset -= (uint32_t)(uintptr_t)pixels_port;
        if (original_count_offset != port_count_offset) {
            printf("    %s: output count differs (%08X vs %08X)\n",
                   name, original_count_offset, port_count_offset);
            failures++;
        }
    }
    if (original_image.pixel_buffer - (uint32_t)(uintptr_t)pixels_original !=
            port_image.pixel_buffer - (uint32_t)(uintptr_t)pixels_port ||
        original_image.palette_buffer - (uint32_t)(uintptr_t)pixels_original !=
            port_image.palette_buffer - (uint32_t)(uintptr_t)pixels_port ||
        memcmp(&original_image.width_pixels, &port_image.width_pixels,
               sizeof original_image - 2 * sizeof(uint32_t)) != 0 ||
        original_image.pixel_count != port_image.pixel_count) {
        printf("    %s: image descriptor differs\n", name);
        failures++;
    }

done:
    if (file_original) VirtualFree(file_original, 0, MEM_RELEASE);
    if (file_port) VirtualFree(file_port, 0, MEM_RELEASE);
    if (pixels_original) VirtualFree(pixels_original, 0, MEM_RELEASE);
    if (pixels_port) VirtualFree(pixels_port, 0, MEM_RELEASE);
    return failures;
}

static int test_iff_valid_cases(void)
{
    Fixture fixture;
    int failures = 0, i;
    make_fixture(&fixture, 32, 2, 0, 1, 1, 1, 0, NULL, 0);
    failures += compare_case("raw 32x2", &fixture, 0, 0, 1);
    make_fixture(&fixture, 17, 2, 0, 1, 1, 1, 0, NULL, 0);
    failures += compare_case("raw odd width 17x2", &fixture, 0, 0, 1);
    make_fixture(&fixture, 17, 2, 1, 1, 1, 1, 0, NULL, 0);
    failures += compare_case("ByteRun1 odd width 17x2", &fixture, 0, 0, 1);
    make_fixture(&fixture, 33, 2, 1, 1, 1, 1, 0, NULL, 0);
    failures += compare_case("ByteRun1 odd width 33x2", &fixture, 0, 0, 1);
    make_fixture(&fixture, 32, 2, 1, 1, 1, 1, 0, NULL, 0);
    failures += compare_case("ByteRun1 stale error preserved", &fixture, 0x12345678u,
                             0x12345678u, 1);
    for (i = 0; i < 40; i++) {
        uint32_t width = (i & 1) ? 17u + 2u * (random_u32() % 24u)
                                 : 16u * (1u + random_u32() % 4u);
        uint32_t height = 1u + random_u32() % 3u;
        int compression = (int)(random_u32() & 1u);
        char case_name[64];
        make_fixture(&fixture, width, height, compression, 1, 1, 1, 0, NULL, 0);
        snprintf(case_name, sizeof case_name, "random %s %ux%u",
                 compression ? "ByteRun1" : "raw", width, height);
        failures += compare_case(case_name, &fixture, 0, 0, 1);
    }
    return failures;
}

static int test_iff_missing_chunks(void)
{
    Fixture fixture;
    int failures = 0;
    make_fixture(&fixture, 16, 1, 0, 0, 0, 0, 0, NULL, 0);
    failures += compare_case("missing BODY", &fixture, 0, 0x802, 0);
    make_fixture(&fixture, 16, 1, 0, 1, 0, 0, 0, NULL, 0);
    failures += compare_case("missing BMHD", &fixture, 0, 0x803, 1);
    make_fixture(&fixture, 16, 1, 0, 1, 1, 0, 0, NULL, 0);
    failures += compare_case("missing CMAP", &fixture, 0, 0x804, 1);
    make_fixture(&fixture, 16, 1, 0, 0, 0, 0, 1, NULL, 0);
    failures += compare_case("invalid FORM", &fixture, 0, 0x801, 0);
    return failures;
}

static int test_iff_byterun_overrun(void)
{
    Fixture fixture;
    uint8_t bad_body[] = {0x81, 0x55};             /* Repeat 128 bytes into 16 output bytes. */
    int failures;
    make_fixture(&fixture, 16, 1, 1, 1, 1, 1, 0, bad_body, sizeof bad_body);
    failures = compare_case("ByteRun1 overrun", &fixture, 0, 0x805, 1);
    return failures;
}

static int test_find_iff_chunk_public(void)
{
    Fixture fixture;
    uint32_t expected = 0;
    uint8_t *p;
    uint32_t actual;
    make_fixture(&fixture, 16, 1, 0, 1, 1, 1, 0, NULL, 0);
    p = fixture.data + 12;
    while ((uint32_t)(p - fixture.data) < fixture.size) {
        uint32_t size = ((uint32_t)p[4] << 24) | ((uint32_t)p[5] << 16) |
                        ((uint32_t)p[6] << 8) | p[7];
        if (memcmp(p, "BODY", 4) == 0) {
            expected = (uint32_t)(p + 4 - fixture.data);
            break;
        }
        p += 8 + size + (size & 1u);
    }
    g_iff_file = (uint32_t)(uintptr_t)fixture.data;
    actual = find_iff_chunk(0x424F4459u);
    if (actual != (uint32_t)(uintptr_t)(fixture.data + expected)) {
        printf("    find_iff_chunk: expected offset %u, got %u\n", expected,
               actual - (uint32_t)(uintptr_t)fixture.data);
        return 1;
    }
    return 0;
}

static int test_symbols_present(void)
{
    printf("    required m_0a284 symbols are missing from ke_symbols.txt\n");
    return 1;
}

void register_m_0a284_tests(void)
{
    o_error = (uint32_t *)oracle_sym("g_iff_error");
    o_file = (uint32_t *)oracle_sym("g_iff_file");
    o_pixels = (uint32_t *)oracle_sym("g_iff_pixels");
    o_pixel_count = (uint32_t *)oracle_sym("g_iff_pixel_count");
    o_palette_bytes = (uint32_t *)oracle_sym("g_iff_palette_bytes");
    o_workspace = (uint32_t *)oracle_sym("g_iff_decoder_workspace");
    o_width = (int *)oracle_sym("iff_width_pixels");
    o_height = (int *)oracle_sym("iff_height_pixels");
    o_output_count = (int *)oracle_sym("iff_output_byte_count");
    o_decoded_count = (int *)oracle_sym("iff_decoded_pixel_count");
    if (!o_error || !o_file || !o_pixels || !o_pixel_count || !o_palette_bytes ||
        !o_workspace || !o_width || !o_height || !o_output_count || !o_decoded_count) {
        oracle_register("m_0a284 symbols present", test_symbols_present);
        return;
    }
    oracle_register("m_0a284 raw, ByteRun1, odd widths, globals", test_iff_valid_cases);
    oracle_register("m_0a284 missing chunks and FORM error codes", test_iff_missing_chunks);
    oracle_register("m_0a284 ByteRun1 overrun", test_iff_byterun_overrun);
    oracle_register("m_0a284 find_iff_chunk C interface", test_find_iff_chunk_public);
}
