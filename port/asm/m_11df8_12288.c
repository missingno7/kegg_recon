/* m_11df8_12288.c - C translation of asm/m_11df8_12288.asm.
 *
 * GIF87a header parsing, GIF sub-block compaction and LZW expansion are kept in the same
 * order as the original. The workspace is the game's 0x6400-byte block: expansion stack at
 * +0, word prefix table at +0x1400, byte suffix table at +0x3c00.
 *
 * QUIRK: the decoder receives no source length, compacts sub-blocks in the source buffer,
 * ignores the interlace bit, and rounds the decoded pixel count down to an even byte count
 * before appending the global palette. These are all part of the original routine's behavior.
 */
#include <stdint.h>

#define GIF87A_SIGNATURE_DWORD 0x38464947u
#define GIF87A_VERSION_WORD 0x6137u
#define GIF_IMAGE_SEPARATOR_BYTE 0x2cu
#define GIF_ERROR_SIGNATURE_DWORD 0x901u
#define GIF_ERROR_VERSION_WORD 0x902u
#define GIF_ERROR_COLOR_TABLE_SIZE 0x905u
#define GIF_ERROR_IMAGE_SEPARATOR 0x906u
#define GIF_ERROR_IMAGE_WIDTH 0x903u
#define GIF_ERROR_IMAGE_HEIGHT 0x904u
#define GIF_ERROR_LOCAL_COLOR_TABLE 0x907u
#define GIF_ERROR_IMAGE_DATA 0x908u
#define GIF_MAX_IMAGE_WIDTH 0x140u
#define GIF_MAX_IMAGE_HEIGHT 0x0c8u
#define GIF_REQUIRED_COLOR_INDEX_BITS 8u
#define GIF_DESCRIPTOR_COLOR_RESOLUTION_SHIFT 4u
#define GIF_DESCRIPTOR_LOW_THREE_BITS_MASK 7u
#define GIF_IMAGE_INTERLACE_FLAG_BIT 6u
#define GIF_ODD_OUTPUT_LENGTH_MASK 1u
#define GIF_RGB_COMPONENTS_PER_ENTRY 3u
#define GIF_BITS_PER_INPUT_BYTE 8u
#define GIF_LZW_MAX_CODE_WIDTH 12u
#define GIF_LZW_CODE_WIDTH_AFTER_CLEAR 9u
#define GIF_LZW_CODE_LIMIT_AFTER_CLEAR 0x200u
#define GIF_LZW_FIRST_CODE_AFTER_CLEAR 0x102u
#define GIF_LZW_MASK_AFTER_CLEAR 0x1ffu
#define GIF_PALETTE_ENTRY_COUNT 0x100u
#define GIF_RGB_TO_VGA_DAC_SHIFT 2u
#define GIF_IMAGE_STATE_DECODED_BYTES_OFFSET 0x10u
#define GIF_LZW_STACK_TO_PREFIX_TABLE_OFFSET 0x1400u
#define GIF_PREFIX_TO_SUFFIX_TABLE_OFFSET 0x2800u

/* _DATA symbols, in the same order and with the same initial bytes as TASM. */
uint16_t gif_reserved_state = 0;
uint16_t gif_lzw_current_code = 0;
uint16_t gif_image_width = 0;
uint16_t gif_image_height = 0;
uint8_t gif_has_global_color_table = 0;
uint8_t gif_color_resolution_bits = 0;
uint16_t gif_color_index_bits = 0;
uint32_t gif_lzw_expansion_stack = 1;
uint32_t gif_lzw_prefix_table = 1;
uint32_t gif_lzw_suffix_table = 1;
uint16_t gif_lzw_code_width = 0;
uint16_t gif_lzw_next_code_width = 0;
uint16_t gif_lzw_first_available_code = 0;
uint16_t gif_lzw_next_code = 0;
uint16_t gif_lzw_code_limit = 0;
uint16_t gif_lzw_code_limit_shadow = 0;
uint16_t gif_lzw_code_mask = 0;
uint16_t gif_color_index_mask = 0;
uint8_t gif_previous_literal = 0;
uint32_t gif_lzw_bit_buffer = 0;
uint16_t gif_lzw_bits_buffered = 0;
uint16_t gif_lzw_saved_codes[3] = { 0, 0, 0 };
uint16_t gif_lzw_clear_code = 0;
uint16_t gif_lzw_end_code = 0;
uint32_t gif_pixel_output_start = 0;
uint32_t gif_compressed_data_cursor = 0;
uint16_t gif_decode_error_code = 0;
uint32_t gif_global_color_table_bytes = 0;
uint32_t gif_global_color_table_source = 0;

extern uint8_t gif_decoded_image_state[24];

static uint16_t read_word(const uint8_t **cursor)
{
    uint16_t value = (uint16_t)(*cursor)[0] | (uint16_t)((uint16_t)(*cursor)[1] << 8);
    *cursor += 2;
    return value;
}

static uint16_t read_lzw_code(uint32_t *bit_buffer, uint16_t *bits_buffered,
                              uint32_t *cursor, uint16_t code_width, uint16_t code_mask)
{
    uint32_t value = *bit_buffer;
    uint16_t bits = *bits_buffered;
    const uint8_t *input = (const uint8_t *)(uintptr_t)*cursor;
    if (bits < code_width) {
        value += (uint32_t)*input++ << bits;
        bits = (uint16_t)(bits + GIF_BITS_PER_INPUT_BYTE);
        if (bits < code_width) {
            value += (uint32_t)*input++ << bits;
            bits = (uint16_t)(bits + GIF_BITS_PER_INPUT_BYTE);
        }
        *cursor = (uint32_t)(uintptr_t)input;
    }
    {
        uint16_t code = (uint16_t)value & code_mask;
        bits = (uint16_t)(bits - code_width);
        value >>= code_width;
        *bit_buffer = value;
        *bits_buffered = bits;
        return code;
    }
}

static uint32_t finish_gif_decode(uint32_t output_start, uint32_t state_address,
                                  uint32_t output_cursor)
{
    uint32_t decoded_bytes = output_cursor - output_start;
    uint8_t *state = (uint8_t *)(uintptr_t)state_address;
    uint8_t *palette_source = (uint8_t *)(uintptr_t)gif_global_color_table_source;
    uint8_t *output = (uint8_t *)(uintptr_t)output_cursor;
    uint32_t palette_bytes = gif_global_color_table_bytes;
    uint32_t remaining = palette_bytes;

    decoded_bytes &= ~GIF_ODD_OUTPUT_LENGTH_MASK;
    output_cursor -= (output_cursor - output_start) & GIF_ODD_OUTPUT_LENGTH_MASK;
    output = (uint8_t *)(uintptr_t)output_cursor;
    *(uint32_t *)(void *)(gif_decoded_image_state + GIF_IMAGE_STATE_DECODED_BYTES_OFFSET) = decoded_bytes;
    *(uint32_t *)(void *)(gif_decoded_image_state + GIF_IMAGE_STATE_DECODED_BYTES_OFFSET) += palette_bytes;

    *(uint32_t *)(void *)(state + 0x00) = output_start;
    *(uint32_t *)(void *)(state + 0x04) = output_cursor;
    *(uint32_t *)(void *)(state + 0x08) = gif_image_width;
    *(uint32_t *)(void *)(state + 0x0c) = gif_image_height;
    *(uint32_t *)(void *)(state + 0x14) = GIF_PALETTE_ENTRY_COUNT;
    *(uint32_t *)(void *)(state + 0x10) = GIF_RGB_COMPONENTS_PER_ENTRY;
    /* TASM uses LOOP at the label: zero ECX still executes once, then wraps to FFFFFFFFh. */
    do {
        *output++ = (uint8_t)(*palette_source++ >> GIF_RGB_TO_VGA_DAC_SHIFT);
        remaining--;
    } while (remaining != 0);
    return gif_decode_error_code;
}

int decode_gif_image(int source_address, int output_address, int image_state_address,
                     void *workspace)
{
    uint32_t output_start = (uint32_t)output_address;
    uint32_t state_address = (uint32_t)image_state_address;
    uint32_t output_cursor = output_start;
    const uint8_t *source = (const uint8_t *)(uintptr_t)(uint32_t)source_address;
    uint8_t *mutable_source = (uint8_t *)(uintptr_t)(uint32_t)source_address;
    uint8_t packed, min_code_size;
    uint16_t code, current_code, base_literal;
    uint32_t stack_address, prefix_address, suffix_address;
    uint32_t stack_cursor;

    stack_address = (uint32_t)(uintptr_t)workspace;
    prefix_address = stack_address + GIF_LZW_STACK_TO_PREFIX_TABLE_OFFSET;
    suffix_address = prefix_address + GIF_PREFIX_TO_SUFFIX_TABLE_OFFSET;
    gif_lzw_expansion_stack = stack_address;
    gif_lzw_prefix_table = prefix_address;
    gif_lzw_suffix_table = suffix_address;
    gif_lzw_code_width = GIF_LZW_MAX_CODE_WIDTH;
    gif_pixel_output_start = output_start;
    gif_reserved_state = 0;
    gif_decode_error_code = GIF_ERROR_SIGNATURE_DWORD;
    if (*(const uint32_t *)(const void *)source != GIF87A_SIGNATURE_DWORD)
        return (int)finish_gif_decode(output_start, state_address, output_cursor);
    source += 4;
    gif_decode_error_code = GIF_ERROR_VERSION_WORD;
    if (read_word(&source) != GIF87A_VERSION_WORD)
        return (int)finish_gif_decode(output_start, state_address, output_cursor);

    gif_image_width = read_word(&source);
    gif_image_height = read_word(&source);
    packed = *source++;
    gif_has_global_color_table = (uint8_t)((int8_t)packed < 0);
    gif_color_resolution_bits = (uint8_t)(((packed >> GIF_DESCRIPTOR_COLOR_RESOLUTION_SHIFT) &
                                           GIF_DESCRIPTOR_LOW_THREE_BITS_MASK) + 1u);
    gif_color_index_bits = (uint16_t)((packed & GIF_DESCRIPTOR_LOW_THREE_BITS_MASK) + 1u);
    gif_decode_error_code = GIF_ERROR_COLOR_TABLE_SIZE;
    if (gif_color_index_bits != GIF_REQUIRED_COLOR_INDEX_BITS)
        return (int)finish_gif_decode(output_start, state_address, output_cursor);
    source += 2; /* background index and pixel aspect ratio */
    if (gif_has_global_color_table) {
        gif_global_color_table_bytes = (1u << gif_color_index_bits) * GIF_RGB_COMPONENTS_PER_ENTRY;
        gif_global_color_table_source = (uint32_t)(uintptr_t)source;
        source += gif_global_color_table_bytes;
    }

    gif_decode_error_code = GIF_ERROR_IMAGE_SEPARATOR;
    if (*source++ != GIF_IMAGE_SEPARATOR_BYTE)
        return (int)finish_gif_decode(output_start, state_address, output_cursor);
    source += 4; /* left and top offsets */
    {
        uint16_t descriptor_width = read_word(&source);
        if (gif_image_width == 0)
            gif_image_width = descriptor_width;
        gif_decode_error_code = GIF_ERROR_IMAGE_WIDTH;
        (void)descriptor_width; /* QUIRK: CMP sets flags, but the assembly never branches on it. */
    }
    {
        uint16_t descriptor_height = read_word(&source);
        if (gif_image_height == 0)
            gif_image_height = descriptor_height;
        gif_decode_error_code = GIF_ERROR_IMAGE_HEIGHT;
        (void)descriptor_height; /* QUIRK: as above, this second dimension compare is dead. */
    }
    gif_decode_error_code = GIF_ERROR_LOCAL_COLOR_TABLE;
    packed = *source++;
    min_code_size = *source++;
    if ((int8_t)packed < 0)
        return (int)finish_gif_decode(output_start, state_address, output_cursor);
    /* QUIRK: the interlace bit is tested by BT but never changes the decode order. */
    (void)((packed >> GIF_IMAGE_INTERLACE_FLAG_BIT) & 1u);

    /* Compact the GIF data sub-blocks over their length bytes in place. */
    {
        uint8_t *subblock_cursor = mutable_source + (source - (const uint8_t *)(uintptr_t)(uint32_t)source_address);
        uint8_t *compact_start = subblock_cursor;
        uint8_t *read_cursor = subblock_cursor;
        uint8_t *write_cursor = subblock_cursor;
        uint8_t block_length = *read_cursor++;
        while (block_length != 0) {
            uint8_t left = block_length;
            while (left--) *write_cursor++ = *read_cursor++;
            block_length = *read_cursor++;
        }
        /* ESI is restored to the first length byte; that byte now holds compacted data. */
        subblock_cursor = compact_start;
        source = subblock_cursor;
    }
    min_code_size = source[-1];
    gif_lzw_code_width = (uint16_t)(min_code_size + 1u);
    gif_lzw_next_code_width = gif_lzw_code_width;
    gif_lzw_clear_code = (uint16_t)(1u << min_code_size);
    gif_lzw_end_code = (uint16_t)(gif_lzw_clear_code + 1u);
    gif_lzw_first_available_code = (uint16_t)(gif_lzw_end_code + 1u);
    gif_lzw_next_code = gif_lzw_first_available_code;
    gif_lzw_code_limit = (uint16_t)(1u << gif_lzw_code_width);
    gif_lzw_code_limit_shadow = gif_lzw_code_limit;
    gif_lzw_code_mask = (uint16_t)(gif_lzw_code_limit - 1u);
    gif_color_index_mask = (uint16_t)((1u << gif_color_index_bits) - 1u);
    gif_lzw_bit_buffer = 0;
    gif_lzw_bits_buffered = 0;
    gif_compressed_data_cursor = (uint32_t)(uintptr_t)source;
    stack_cursor = stack_address;
    gif_decode_error_code = 0;

    for (;;) {
        uint16_t bits = gif_lzw_bits_buffered;
        uint32_t cursor = gif_compressed_data_cursor;
        uint16_t width = gif_lzw_code_width;
        code = read_lzw_code(&gif_lzw_bit_buffer, &bits, &cursor, width, gif_lzw_code_mask);
        gif_lzw_bits_buffered = bits;
        gif_compressed_data_cursor = cursor;
        if (code == gif_lzw_end_code)
            break;
        if (code == gif_lzw_clear_code) {
            gif_lzw_code_width = GIF_LZW_CODE_WIDTH_AFTER_CLEAR;
            gif_lzw_code_limit = GIF_LZW_CODE_LIMIT_AFTER_CLEAR;
            gif_lzw_next_code = GIF_LZW_FIRST_CODE_AFTER_CLEAR;
            gif_lzw_code_mask = GIF_LZW_MASK_AFTER_CLEAR;
            bits = gif_lzw_bits_buffered;
            cursor = gif_compressed_data_cursor;
            code = read_lzw_code(&gif_lzw_bit_buffer, &bits, &cursor,
                                 gif_lzw_code_width, gif_lzw_code_mask);
            gif_lzw_bits_buffered = bits;
            gif_compressed_data_cursor = cursor;
            gif_lzw_saved_codes[0] = code;
            gif_previous_literal = (uint8_t)code;
            *(uint8_t *)(uintptr_t)output_cursor++ = (uint8_t)code;
            continue;
        }

        gif_lzw_current_code = code;
        current_code = code;
        if (code >= gif_lzw_next_code) {
            code = gif_lzw_saved_codes[0];
            *(uint8_t *)(uintptr_t)stack_cursor++ = gif_previous_literal;
        }
        base_literal = gif_color_index_mask;
        while (code > base_literal) {
            uint8_t suffix = *(uint8_t *)(uintptr_t)(gif_lzw_suffix_table + code);
            *(uint8_t *)(uintptr_t)stack_cursor++ = suffix;
            code = *(uint16_t *)(uintptr_t)(gif_lzw_prefix_table + (uint32_t)code * 2u);
        }
        code &= base_literal;
        gif_previous_literal = (uint8_t)code;
        *(uint8_t *)(uintptr_t)output_cursor++ = (uint8_t)code;
        if (stack_cursor != stack_address) {
            do {
                stack_cursor--;
                *(uint8_t *)(uintptr_t)output_cursor++ = *(uint8_t *)(uintptr_t)stack_cursor;
            } while (stack_cursor != stack_address);
            code = gif_previous_literal;
        }
        *(uint8_t *)(uintptr_t)(gif_lzw_suffix_table + gif_lzw_next_code) = (uint8_t)code;
        *(uint16_t *)(uintptr_t)(gif_lzw_prefix_table + (uint32_t)gif_lzw_next_code * 2u) =
            gif_lzw_saved_codes[0];
        gif_lzw_saved_codes[0] = current_code;
        gif_lzw_next_code++;
        if (gif_lzw_next_code == gif_lzw_code_limit && gif_lzw_code_width != GIF_LZW_MAX_CODE_WIDTH) {
            gif_lzw_code_width++;
            gif_lzw_code_limit = (uint16_t)(gif_lzw_code_limit << 1);
            gif_lzw_code_mask = (uint16_t)(gif_lzw_code_limit - 1u);
        }
    }

    return (int)finish_gif_decode(output_start, state_address, output_cursor);
}

int decode_gif_image_entry(int source_address, int output_address, int image_state_address,
                            void *workspace)
{
    return decode_gif_image(source_address, output_address, image_state_address, workspace);
}
