/* m_12684_1296e.c - C translation of the run-length sprite renderer. */
#include <stdint.h>
#include "sprite_records.h"

extern uint32_t sprite_clip_top, sprite_clip_left, sprite_clip_right;
extern uint32_t visible_sprite_width;
extern uint32_t vga_row_advance, background_plane_delta;
extern int render_page_base, screen_page_base;
extern uint8_t vga_state[];

static void draw_rows(const uint8_t *source, uint32_t width, uint32_t height, uint32_t dest)
{
    uint32_t stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    uint32_t left = sprite_clip_left;
    uint32_t row, run_x;
    vga_row_advance = stride - width;
    for (row = 0; row < height; row++) {
        unsigned runs = *source++;
        run_x = 0;
        while (runs--) {
            int8_t run = (int8_t)*source++;
            uint32_t count = run < 0 ? (uint32_t)(-(int)run) : (uint32_t)run;
            if (run > 0) {
                uint32_t first = run_x > left ? run_x : left;
                uint32_t end = run_x + count;
                uint32_t visible_end = left + width;
                uint32_t last = end < visible_end ? end : visible_end;
                if (first < last) {
                    uint32_t i;
                    for (i = first; i < last; i++)
                        sprite_vga_write_linear(dest + (i - left), source[i - run_x]);
                }
                source += count;
            }
            run_x += count;
        }
        dest += stride;
    }
}

void render_sprite_record_kind_3_draw(const uint8_t *esi, uint32_t ebx,
                                      uint32_t edx, uint32_t edi)
{
    uint8_t *source = sprite_skip_rle_rows((uint8_t *)(uintptr_t)esi, sprite_clip_top);
    draw_rows(source, ebx, edx, edi);
}

void render_sprite_record_kind_3_entry(const uint8_t *esi, uint32_t ebx,
                                       uint32_t edx, uint32_t edi)
{
    uint8_t *source = sprite_skip_rle_rows((uint8_t *)(uintptr_t)esi, sprite_clip_top);
    SpriteUpdateRecord *record = sprite_update_record_reserve();
    record->kind = 3;
    record->height_rows = (uint16_t)edx;
    record->width_pixels = (uint16_t)ebx;
    record->source_stream = (uint32_t)(uintptr_t)source;
    record->page_offset = edi - (uint32_t)render_page_base;
    record->clip_left = (uint16_t)sprite_clip_left;
    record->clip_width = (uint16_t)visible_sprite_width;
    draw_rows(source, ebx, edx, edi);
}

void restore_sprite_background_from_record(SpriteUpdateRecord *record)
{
    const uint8_t *source = (const uint8_t *)(uintptr_t)record->source_stream;
    uint32_t stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    uint32_t left = record->clip_left;
    uint32_t width = record->clip_width;
    uint32_t src_page = (uint32_t)screen_page_base + record->page_offset;
    uint32_t dst_page = (uint32_t)render_page_base + record->page_offset;
    uint32_t row;
    background_plane_delta = (uint32_t)screen_page_base - (uint32_t)render_page_base;

    for (row = 0; row < record->height_rows; row++) {
        unsigned runs = *source++;
        uint32_t run_x = 0;
        while (runs--) {
            int8_t run = (int8_t)*source++;
            uint32_t count = run < 0 ? (uint32_t)(-(int)run) : (uint32_t)run;
            if (run > 0) {
                uint32_t first = run_x > left ? run_x : left;
                uint32_t end = run_x + count;
                uint32_t visible_end = left + width;
                uint32_t last = end < visible_end ? end : visible_end;
                if (first < last) {
                    uint32_t x;
                    for (x = first; x < last; x++) {
                        uint32_t offset = (x - left) + row * stride;
                        uint8_t value = sprite_vga_read_linear(src_page + offset);
                        sprite_vga_write_linear(dst_page + offset, value);
                    }
                }
                source += count;
            }
            run_x += count;
        }
    }
}
