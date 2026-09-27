/* m_12288_12680.c - C translation of the four-plane BOB renderer. */
#include <stdint.h>
#include "sprite_records.h"
#include "../vhw/vhw.h"

#define SEQ_INDEX_PORT 0x3c4
#define SEQ_MAP_MASK_INDEX 2

extern uint32_t sprite_clip_top, sprite_clip_left, sprite_clip_right;
extern uint32_t visible_sprite_width, vga_row_advance, background_plane_delta;
extern uint32_t vga_plane_index, sprite_row_width_remaining;
extern uint16_t sprite_source_column;
extern uint8_t current_vga_plane_mask, first_vga_plane_mask;
extern int render_page_base, screen_page_base;
extern uint8_t vga_state[];

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t pixel_to_planar_address(uint32_t pixel_address)
{
    if (pixel_address >= 0xa0000u && pixel_address < 0xc0000u)
        return 0xa0000u + ((pixel_address - 0xa0000u) >> 2);
    return pixel_address >> 2;
}

static void select_plane(unsigned plane)
{
    unsigned i;
    current_vga_plane_mask = 0x11;
    for (i = 0; i < (plane & 3u); i++)
        current_vga_plane_mask = (uint8_t)((current_vga_plane_mask << 1) |
                                           (current_vga_plane_mask >> 7));
    sprite_outpw(SEQ_INDEX_PORT, (uint16_t)((current_vga_plane_mask << 8) | SEQ_MAP_MASK_INDEX));
}

static uint8_t *skip_plane_rows(uint8_t *source, unsigned rows)
{
    return sprite_skip_rle_rows(source, rows);
}

static void draw_plane_stream(const uint8_t *stream, uint32_t width, uint32_t height,
                              uint32_t pixel_dest, unsigned stream_plane,
                              unsigned first_plane, uint32_t left, uint32_t stride)
{
    const uint8_t *source = skip_plane_rows((uint8_t *)(uintptr_t)stream, sprite_clip_top);
    uint32_t y;
    uint32_t origin_x = pixel_dest - left;
    uint32_t visible_right = left + width;
    for (y = 0; y < height; y++) {
        unsigned runs = *source++;
        uint32_t sample = 0;
        while (runs--) {
            int8_t run = (int8_t)*source++;
            uint32_t count = run < 0 ? (uint32_t)(-(int)run) : (uint32_t)run;
            if (run > 0) {
                uint32_t j;
                for (j = 0; j < count; j++) {
                    uint32_t sprite_x = stream_plane + 4u * (sample + j);
                    if (sprite_x >= left && sprite_x < visible_right) {
                        uint32_t pixel = origin_x + sprite_x + y * stride;
                        unsigned plane = (first_plane + (stream_plane + 4u * (sample + j) - left)) & 3u;
                        uint32_t cpu = pixel_to_planar_address(pixel);
                        (void)plane; /* The outer pass selected this register mask. */
                        sprite_vga_write_linear(cpu, source[j]);
                    }
                }
                source += count;
            }
            sample += count;
        }
    }
}

static void draw_unclipped_plane_stream(const uint8_t *stream, uint32_t width,
                                        uint32_t height, uint32_t first_address,
                                        uint32_t row_advance)
{
    const uint8_t *source = sprite_skip_rle_rows((uint8_t *)(uintptr_t)stream,
                                                  sprite_clip_top);
    uint32_t row, row_address = first_address;
    for (row = 0; row < height; row++) {
        unsigned runs = *source++;
        uint32_t column = 0;
        while (runs--) {
            int8_t run = (int8_t)*source++;
            uint32_t count = run < 0 ? (uint32_t)(-(int)run) : (uint32_t)run;
            if (run > 0) {
                uint32_t x;
                for (x = 0; x < count; x++)
                    sprite_vga_write_linear(row_address + column + x, source[x]);
                source += count;
            }
            column += count;
        }
        row_address += column + row_advance;
    }
    (void)width;
}

static void draw_left_clipped_plane_stream(const uint8_t *stream, uint32_t height,
                                           uint32_t first_address, uint32_t clipped_columns,
                                           uint32_t plane_stride)
{
    const uint8_t *source = sprite_skip_rle_rows((uint8_t *)(uintptr_t)stream,
                                                  sprite_clip_top);
    uint32_t clip_base = first_address;
    uint32_t row;
    for (row = 0; row < height; row++) {
        unsigned runs = *source++;
        uint32_t dest = clip_base - clipped_columns;
        while (runs--) {
            int8_t run = (int8_t)*source++;
            uint32_t count = run < 0 ? (uint32_t)(-(int)run) : (uint32_t)run;
            if (run < 0) {
                dest += count;
            } else {
                int32_t relative = (int32_t)(dest - clip_base);
                if (relative < 0) {
                    int32_t end = relative + (int32_t)count;
                    if (end <= 0) {
                        source += count;
                        dest += count;
                        continue;
                    }
                    count = (uint32_t)end;
                    source -= relative;
                    dest -= relative;
                }
                {
                    uint32_t x;
                    for (x = 0; x < count; x++)
                        sprite_vga_write_linear(dest + x, source[x]);
                }
                source += count;
                dest += count;
            }
        }
        clip_base += plane_stride;
    }
}

void render_sprite_record_kind_5_draw(const uint8_t *esi, uint32_t ebx,
                                      uint32_t edx, uint32_t edi)
{
    const uint8_t *table = esi - 10;
    uint32_t stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    unsigned first_plane = edi & 3u;
    unsigned first_stream = sprite_clip_left & 3u;
    unsigned pass;

    current_vga_plane_mask = (uint8_t)((0x11u << first_plane) |
                                       (0x11u >> (8u - first_plane)));
    first_vga_plane_mask = current_vga_plane_mask;
    sprite_row_width_remaining = ebx;
    vga_row_advance = stride >> 2;
    if (!sprite_clip_left && !sprite_clip_right) {
        uint32_t base = pixel_to_planar_address(edi);
        uint32_t row_advance = (stride - ebx) >> 2;
        for (pass = 0; pass < 4; pass++) {
            unsigned plane = (first_plane + pass) & 3u;
            unsigned wraps = first_plane + pass >= 4u;
            uint8_t *stream = (uint8_t *)(uintptr_t)(table + 10u +
                                      get16(table + pass * 2u));
            select_plane(plane);
            draw_unclipped_plane_stream(stream, ebx, edx,
                                        base + wraps, row_advance);
            sprite_source_column = (uint16_t)(8u - 2u * pass);
        }
        current_vga_plane_mask = first_vga_plane_mask;
        return;
    }
    if (sprite_clip_left) {
        uint32_t clipped_columns = sprite_clip_left >> 2;
        unsigned first_stream = sprite_clip_left & 3u;
        vga_plane_index = first_stream;
        vga_row_advance = stride >> 2;
        for (pass = 0; pass < 4; pass++) {
            unsigned plane = (first_plane + pass) & 3u;
            unsigned stream_index = (first_stream + pass) & 3u;
            uint32_t wraps = first_plane + pass >= 4u;
            uint8_t *stream = (uint8_t *)(uintptr_t)(table + 10u +
                                      get16(table + stream_index * 2u));
            select_plane(plane);
            draw_left_clipped_plane_stream(stream, edx,
                                           pixel_to_planar_address(edi) + wraps,
                                           clipped_columns, stride >> 2);
            vga_plane_index++;
        }
        current_vga_plane_mask = first_vga_plane_mask;
        return;
    }
    for (pass = 0; pass < 4; pass++) {
        unsigned stream_index = (first_stream + pass) & 3u;
        unsigned plane = (first_plane + pass) & 3u;
        uint8_t *stream = (uint8_t *)(uintptr_t)(table + 10u +
                                                 get16(table + stream_index * 2u));
        select_plane(plane);
        draw_plane_stream(stream, ebx, edx, edi, stream_index, first_plane,
                          sprite_clip_left, stride);
        sprite_source_column = (uint16_t)(10u - 2u * (pass + 1u));
    }
    current_vga_plane_mask = first_vga_plane_mask;
}

void render_sprite_record_kind_5_entry(const uint8_t *esi, uint32_t ebx,
                                       uint32_t edx, uint32_t edi)
{
    const uint8_t *table = esi - 10;
    SpriteUpdateRecord *record = sprite_update_record_reserve();
    record->kind = 5;
    record->height_rows = (uint16_t)edx;
    record->width_pixels = (uint16_t)ebx;
    record->source_stream = (uint32_t)(uintptr_t)(table + 10u + get16(table + 8));
    record->page_offset = edi - (uint32_t)render_page_base;
    record->clip_left = (uint16_t)sprite_clip_left;
    record->clip_width = (uint16_t)visible_sprite_width;
    record->clip_top_rows = (uint16_t)sprite_clip_top;
    render_sprite_record_kind_5_draw(esi, ebx, edx, edi);
}

void restore_sprite_background_record(SpriteUpdateRecord *record)
{
    uint32_t source_pixel = (uint32_t)screen_page_base + record->page_offset;
    uint32_t dest_pixel = (uint32_t)render_page_base + record->page_offset;
    uint32_t stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    uint32_t bytes = ((source_pixel & 3u) + record->width_pixels + 3u) >> 2;
    uint32_t y, x;
    background_plane_delta = (uint32_t)screen_page_base - (uint32_t)render_page_base;
    if (record->clip_left || record->clip_width) {
        for (y = 0; y < record->height_rows; y++) {
            uint32_t src = pixel_to_planar_address(source_pixel + y * stride) & ~0u;
            uint32_t dst = pixel_to_planar_address(dest_pixel + y * stride) & ~0u;
            for (x = 0; x < bytes; x++) {
                uint8_t value = sprite_vga_read_linear(src + x);
                sprite_vga_write_linear(dst + x, value);
            }
        }
        return;
    }
    /* Unclipped records retain the planar run stream for sparse background restoration. */
    {
        const uint8_t *source = (const uint8_t *)(uintptr_t)record->source_stream;
        uint32_t row;
        for (row = 0; row < record->height_rows; row++) {
            unsigned runs = *source++;
            uint32_t pos = 0;
            while (runs--) {
                int8_t run = (int8_t)*source++;
                uint32_t count = run < 0 ? (uint32_t)(-(int)run) : (uint32_t)run;
                if (run > 0) {
                    uint32_t n;
                    for (n = 0; n < count; n++) {
                        uint32_t src = pixel_to_planar_address(source_pixel + row * stride + (pos + n) * 4u);
                        uint32_t dst = pixel_to_planar_address(dest_pixel + row * stride + (pos + n) * 4u);
                        uint8_t value = sprite_vga_read_linear(src);
                        sprite_vga_write_linear(dst, value);
                    }
                    source += count;
                }
                pos += count;
            }
        }
    }
}
