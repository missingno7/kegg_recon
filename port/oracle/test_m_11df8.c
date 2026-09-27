/* test_m_11df8.c - differential GIF LZW tests against the original LE object. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"

extern int decode_gif_image(int, int, int, void *);
extern int decode_gif_image_entry(int, int, int, void *);
extern uint8_t gif_decoded_image_state[24];
extern uint16_t gif_reserved_state, gif_lzw_current_code, gif_image_width, gif_image_height;
extern uint8_t gif_has_global_color_table, gif_color_resolution_bits, gif_previous_literal;
extern uint16_t gif_color_index_bits, gif_lzw_code_width, gif_lzw_next_code_width;
extern uint16_t gif_lzw_first_available_code, gif_lzw_next_code, gif_lzw_code_limit;
extern uint16_t gif_lzw_code_limit_shadow, gif_lzw_code_mask, gif_color_index_mask;
extern uint32_t gif_lzw_expansion_stack, gif_lzw_prefix_table, gif_lzw_suffix_table;
extern uint32_t gif_lzw_bit_buffer, gif_compressed_data_cursor, gif_global_color_table_bytes;
extern uint16_t gif_lzw_bits_buffered, gif_lzw_saved_codes[3], gif_lzw_clear_code;
extern uint16_t gif_lzw_end_code, gif_decode_error_code;
extern uint32_t gif_pixel_output_start, gif_global_color_table_source;

#define SOURCE_PAD 0x10000u
#define OUTPUT_BYTES 0x40000u
#define WORKSPACE_BYTES 0x10000u

static uint32_t rng = 0x31df8224u;
static uint32_t random_below(uint32_t n)
{
    rng = rng * 1664525u + 1013904223u;
    return n ? rng % n : 0;
}

static void reset_port_state(void)
{
    gif_reserved_state = 0;
    gif_lzw_current_code = 0;
    gif_image_width = 0;
    gif_image_height = 0;
    gif_has_global_color_table = 0;
    gif_color_resolution_bits = 0;
    gif_color_index_bits = 0;
    gif_lzw_expansion_stack = 1;
    gif_lzw_prefix_table = 1;
    gif_lzw_suffix_table = 1;
    gif_lzw_code_width = 0;
    gif_lzw_next_code_width = 0;
    gif_lzw_first_available_code = 0;
    gif_lzw_next_code = 0;
    gif_lzw_code_limit = 0;
    gif_lzw_code_limit_shadow = 0;
    gif_lzw_code_mask = 0;
    gif_color_index_mask = 0;
    gif_previous_literal = 0;
    gif_lzw_bit_buffer = 0;
    gif_lzw_bits_buffered = 0;
    memset(gif_lzw_saved_codes, 0, sizeof gif_lzw_saved_codes);
    gif_lzw_clear_code = 0;
    gif_lzw_end_code = 0;
    gif_pixel_output_start = 0;
    gif_compressed_data_cursor = 0;
    gif_decode_error_code = 0;
    gif_global_color_table_bytes = 0;
    gif_global_color_table_source = 0;
    memset(gif_decoded_image_state, 0, 24);
}

static void reset_original_state(void)
{
    uint8_t *object3 = (uint8_t *)oracle_object_base(3);
    uint8_t *data = object3 + 0x8314;       /* manifest g_8314: gif_reserved_state */
    uint8_t *image_state = object3 + 0xe2ac; /* manifest g_e2ac: gif_decoded_image_state */
    memset(data, 0, 0x4c);
    *(uint32_t *)(void *)(data + 0x0c) = 1;
    *(uint32_t *)(void *)(data + 0x10) = 1;
    *(uint32_t *)(void *)(data + 0x14) = 1;
    memset(image_state, 0, 24);
}

/* Give early error paths a deterministic, writable incoming EDI, as the historical caller
 * did with its output buffer already live in a register. The routine itself preserves EDI. */
static __attribute__((noinline)) uint32_t oracle_call_gif(void *fn, uint32_t input,
                                uint32_t output, uint32_t state, uint32_t workspace)
{
    uint32_t result;
    __asm__ volatile (
        "pushl %%edi\n\t"
        "movl %[output], %%edi\n\t"
        "pushl %[workspace]\n\t"
        "pushl %[state]\n\t"
        "pushl %[output]\n\t"
        "pushl %[input]\n\t"
        "call *%[function]\n\t"
        "addl $16, %%esp\n\t"
        "popl %%edi"
        : "=a" (result)
        : [function] "m" (fn), [input] "m" (input), [output] "m" (output),
          [state] "m" (state), [workspace] "m" (workspace)
        : "ecx", "edx", "memory", "cc");
    return result;
}

static int read_file(const char *path, uint8_t **bytes, uint32_t *length)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    *bytes = (uint8_t *)malloc((size_t)n);
    if (!*bytes || fread(*bytes, 1, (size_t)n, f) != (size_t)n) {
        free(*bytes);
        fclose(f);
        return -1;
    }
    fclose(f);
    *length = (uint32_t)n;
    return 0;
}

static int decode_asset_payload(uint8_t *bytes, uint32_t *length, const char *name)
{
    uint32_t args[2];
    uint32_t trailer;
    args[0] = (uint32_t)(uintptr_t)bytes;
    args[1] = *length;
    trailer = oracle_call((uint8_t *)oracle_object_base(1) + 0x982c, 2, args);
    if (trailer == 0xffffffffu || trailer > *length || memcmp(bytes, "GIF87a", 6) != 0) {
        printf("    %s: original asset decoder failed (trailer=%u, header=%02x%02x%02x%02x%02x%02x)\n",
               name, trailer, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
        return -1;
    }
    *length -= trailer;
    return 0;
}

static int compare_case(const uint8_t *bytes, uint32_t length, int compare_output,
                        int use_entry, uint32_t expected_result, const char *case_name)
{
    uint8_t *original_input = (uint8_t *)VirtualAlloc(NULL, SOURCE_PAD, MEM_RESERVE | MEM_COMMIT,
                                                       PAGE_READWRITE);
    uint8_t *port_input = (uint8_t *)VirtualAlloc(NULL, SOURCE_PAD, MEM_RESERVE | MEM_COMMIT,
                                                   PAGE_READWRITE);
    uint8_t *original_output = (uint8_t *)VirtualAlloc(NULL, OUTPUT_BYTES, MEM_RESERVE | MEM_COMMIT,
                                                        PAGE_READWRITE);
    uint8_t *port_output = (uint8_t *)VirtualAlloc(NULL, OUTPUT_BYTES, MEM_RESERVE | MEM_COMMIT,
                                                    PAGE_READWRITE);
    uint8_t *original_workspace = (uint8_t *)VirtualAlloc(NULL, WORKSPACE_BYTES,
                                                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t *port_workspace = (uint8_t *)VirtualAlloc(NULL, WORKSPACE_BYTES,
                                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t original_state[24], port_state[24];
    uint32_t args[4], original_result, port_result;
    int failures = 0;

    if (!original_input || !port_input || !original_output || !port_output ||
        !original_workspace || !port_workspace) {
        printf("    %s: VirtualAlloc failed\n", case_name);
        failures++;
        goto done;
    }
    memset(original_input, 0, SOURCE_PAD);
    memset(port_input, 0, SOURCE_PAD);
    memcpy(original_input, bytes, length);
    memcpy(port_input, bytes, length);
    memset(original_output, 0xa5, OUTPUT_BYTES);
    memset(port_output, 0xa5, OUTPUT_BYTES);
    memset(original_workspace, 0xcc, WORKSPACE_BYTES);
    memset(port_workspace, 0xcc, WORKSPACE_BYTES);
    memset(original_state, 0x5a, sizeof original_state);
    memset(port_state, 0x5a, sizeof port_state);

    reset_original_state();
    *(uint32_t *)(void *)((uint8_t *)oracle_object_base(3) + 0x8357) = 768;
    *(uint32_t *)(void *)((uint8_t *)oracle_object_base(3) + 0x835b) =
        (uint32_t)(uintptr_t)(original_input + 0x100);
    args[0] = (uint32_t)(uintptr_t)original_input;
    args[1] = (uint32_t)(uintptr_t)original_output;
    args[2] = (uint32_t)(uintptr_t)original_state;
    args[3] = (uint32_t)(uintptr_t)original_workspace;
    original_result = oracle_call_gif(oracle_sym("decode_gif_image"), args[0], args[1], args[2], args[3]);

    reset_port_state();
    gif_global_color_table_bytes = 768;
    gif_global_color_table_source = (uint32_t)(uintptr_t)(port_input + 0x100);
    if (use_entry)
        port_result = (uint32_t)decode_gif_image_entry((int)(uintptr_t)port_input,
            (int)(uintptr_t)port_output, (int)(uintptr_t)port_state, port_workspace);
    else
        port_result = (uint32_t)decode_gif_image((int)(uintptr_t)port_input,
            (int)(uintptr_t)port_output, (int)(uintptr_t)port_state, port_workspace);

    if (original_result != port_result || original_result != expected_result ||
        *(uint16_t *)((uint8_t *)oracle_object_base(3) + 0x8355) != gif_decode_error_code) {
        printf("    %s: return/error expected=%03x original=%03x/%03x port=%03x/%03x\n",
               case_name, expected_result,
               original_result, *(uint16_t *)((uint8_t *)oracle_object_base(3) + 0x8355),
               port_result, gif_decode_error_code);
        failures++;
    }
    if (compare_output) {
        uint32_t original_pixel_bytes = *(uint32_t *)(void *)(original_state + 4) -
                                        (uint32_t)(uintptr_t)original_output;
        uint32_t port_pixel_bytes = *(uint32_t *)(void *)(port_state + 4) -
                                   (uint32_t)(uintptr_t)port_output;
        uint32_t palette_bytes = *(uint32_t *)(void *)(original_state + 0x10) *
                                 *(uint32_t *)(void *)(original_state + 0x14);
        uint32_t port_palette_bytes = *(uint32_t *)(void *)(port_state + 0x10) *
                                     *(uint32_t *)(void *)(port_state + 0x14);
        uint32_t total = original_pixel_bytes + palette_bytes;
        if (original_pixel_bytes > OUTPUT_BYTES || port_pixel_bytes > OUTPUT_BYTES ||
            total > OUTPUT_BYTES || port_pixel_bytes + port_palette_bytes > OUTPUT_BYTES) {
            printf("    %s: output bounds orig=%u+%u port=%u+%u\n", case_name,
                   original_pixel_bytes, palette_bytes, port_pixel_bytes, port_palette_bytes);
            failures++;
        } else if (*(uint32_t *)(void *)original_state != (uint32_t)(uintptr_t)original_output ||
                   *(uint32_t *)(void *)port_state != (uint32_t)(uintptr_t)port_output ||
                   original_pixel_bytes != port_pixel_bytes || palette_bytes != port_palette_bytes ||
                   memcmp(original_state + 8, port_state + 8, 16) != 0 ||
                   memcmp(original_output, port_output, total) != 0 ||
                   memcmp((uint8_t *)oracle_object_base(3) + 0xe2ac, gif_decoded_image_state, 24) != 0 ||
                   memcmp(original_input, port_input, length) != 0 ||
                   memcmp(original_workspace, port_workspace, 0x6400) != 0) {
            uint32_t at = 0;
            uint8_t *original_module = (uint8_t *)oracle_object_base(3) + 0x8314;
            uint32_t original_palette = *(uint32_t *)(void *)(original_module + 0x47);
            uint32_t port_palette = gif_global_color_table_source;
            while (at < total && original_output[at] == port_output[at]) at++;
            printf("    %s: diff pixel=%u/%u palette=%u/%u dims=%u,%u/%u,%u out@%u=%02x/%02x src=%d work=%d glob=%d gct=%u/%u palptr=%08x/%08x pal0=%02x/%02x outbase=%08x/%08x\n",
                   case_name, original_pixel_bytes, port_pixel_bytes, palette_bytes,
                   port_palette_bytes, *(uint32_t *)(void *)(original_state + 8),
                   *(uint32_t *)(void *)(original_state + 12),
                   *(uint32_t *)(void *)(port_state + 8), *(uint32_t *)(void *)(port_state + 12),
                   at, original_output[at], port_output[at],
                   memcmp(original_input, port_input, length) != 0,
                   memcmp(original_workspace, port_workspace, 0x6400) != 0,
                   memcmp((uint8_t *)oracle_object_base(3) + 0xe2ac, gif_decoded_image_state, 24) != 0,
                   *(uint8_t *)(void *)(original_module + 8), gif_has_global_color_table,
                   original_palette - (uint32_t)(uintptr_t)original_input, port_palette - (uint32_t)(uintptr_t)port_input,
                   original_input[original_palette - (uint32_t)(uintptr_t)original_input],
                   port_input[port_palette - (uint32_t)(uintptr_t)port_input],
                   *(uint32_t *)(void *)original_state, *(uint32_t *)(void *)port_state);
            failures++;
        }
    }

done:
    if (original_input) VirtualFree(original_input, 0, MEM_RELEASE);
    if (port_input) VirtualFree(port_input, 0, MEM_RELEASE);
    if (original_output) VirtualFree(original_output, 0, MEM_RELEASE);
    if (port_output) VirtualFree(port_output, 0, MEM_RELEASE);
    if (original_workspace) VirtualFree(original_workspace, 0, MEM_RELEASE);
    if (port_workspace) VirtualFree(port_workspace, 0, MEM_RELEASE);
    return failures;
}

static int test_asset(const char *name)
{
    uint8_t *bytes = NULL;
    uint32_t length = 0;
    char path[128];
    int failures;
    snprintf(path, sizeof path, "assets/%s", name);
    if (read_file(path, &bytes, &length) != 0) {
        printf("    cannot read %s\n", path);
        return 1;
    }
    if (decode_asset_payload(bytes, &length, name) != 0) {
        free(bytes);
        return 1;
    }
    failures = compare_case(bytes, length, 1, strcmp(name, "KE_SCORE.GIF") == 0, 0, name);
    free(bytes);
    return failures;
}

static int test_fuzzed_truncations(void)
{
    static const char *const names[] = {
        "KE_MENU.GIF", "KE_MONST.GIF", "KE_ORDER.GIF", "KE_SCORE.GIF"
    };
    int failures = 0;
    unsigned i, j;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        uint8_t *bytes = NULL;
        uint32_t length = 0, separator;
        char path[128], label[96];
        snprintf(path, sizeof path, "assets/%s", names[i]);
        if (read_file(path, &bytes, &length) != 0) {
            printf("    cannot read %s\n", path);
            failures++;
            continue;
        }
        if (decode_asset_payload(bytes, &length, names[i]) != 0) {
            failures++;
            free(bytes);
            continue;
        }
        /* GIF87a header has a 256-color global palette in these fixtures. Truncate at
         * random points through the header/palette, before its image separator. */
        separator = 13u + 256u * 3u;
        if (separator >= length || bytes[separator] != 0x2c) {
            printf("    %s: unexpected GIF header layout\n", names[i]);
            failures++;
            free(bytes);
            continue;
        }
        for (j = 0; j < 64; j++) {
            uint32_t cut = 13u + random_below(separator - 12u);
            snprintf(label, sizeof label, "%s truncation %u", names[i], cut);
            failures += compare_case(bytes, cut, 0, 0, 0x906, label);
        }
        free(bytes);
    }
    return failures;
}

static int test_malformed_headers(void)
{
    uint8_t *bytes = NULL;
    uint32_t length = 0;
    int failures = 0;
    char path[128];
    const char *name = "assets/KE_MENU.GIF";
    uint8_t *mutant;
    static const struct { uint32_t offset; uint8_t value; uint32_t error; const char *label; } cases[] = {
        { 0, 0, 0x901, "signature error" },
        { 4, '8', 0x902, "version error" },
        { 10, 0x70, 0x905, "color table size error" },
        { 13 + 768, 0, 0x906, "image separator error" },
        { 13 + 768 + 5, 0x41, 0, "unchecked image width compare" },
        { 13 + 768 + 7, 0xc9, 0, "unchecked image height compare" },
        { 13 + 768 + 9, 0x80, 0x907, "local table error" }
    };
    unsigned i;
    snprintf(path, sizeof path, "%s", name);
    if (read_file(path, &bytes, &length) != 0)
        return 1;
    if (decode_asset_payload(bytes, &length, name) != 0) {
        free(bytes);
        return 1;
    }
    mutant = (uint8_t *)malloc(length);
    if (!mutant) {
        free(bytes);
        return 1;
    }
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        memcpy(mutant, bytes, length);
        mutant[cases[i].offset] = cases[i].value;
        failures += compare_case(mutant, length, 0, 0, cases[i].error, cases[i].label);
    }
    free(mutant);
    free(bytes);
    return failures;
}

static int test_gif_assets(void)
{
    int failures = 0;
    failures += test_asset("KE_MENU.GIF");
    failures += test_asset("KE_MONST.GIF");
    failures += test_asset("KE_ORDER.GIF");
    failures += test_asset("KE_SCORE.GIF");
    failures += test_fuzzed_truncations();
    failures += test_malformed_headers();
    return failures;
}

void register_m_11df8_tests(void)
{
    oracle_register("m_11df8 GIF assets, LZW, palette and state", test_gif_assets);
}
