/* m_12974_12a9c.c - C translation of the transparent sprite record module. */
#include <stdint.h>
#include "sprite_records.h"

extern uint32_t sprite_clip_top, sprite_clip_left, sprite_clip_right;
extern uint32_t vga_row_advance;
extern int render_page_base, screen_page_base;
extern uint8_t vga_state[];

static uint8_t *skip_pixels(const uint8_t *pixels, uint32_t width, uint32_t rows)
{
    return (uint8_t *)(uintptr_t)(pixels + (uintptr_t)width * rows);
}

void draw_transparent_sprite_rows(const uint8_t *esi, uint32_t ebx, uint32_t edx, uint32_t edi)
{
    uint32_t clip_left = sprite_clip_left;
    uint32_t clip_right = sprite_clip_right;
    uint32_t full_width = ebx + clip_left + clip_right;
    uint32_t row, x;
    const uint8_t *source = skip_pixels(esi, full_width, sprite_clip_top) + clip_left;
    uint32_t stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    vga_row_advance = stride - ebx;
    for (row = 0; row < edx; row++) {
        for (x = 0; x < ebx; x++) {
            uint8_t pixel = source[x];
            if (pixel)
                sprite_vga_write_linear(edi + x, pixel);
        }
        source += full_width;
        edi += stride;
    }
}

void render_transparent_sprite_record_entry(const uint8_t *esi, uint32_t ebx,
                                            uint32_t edx, uint32_t edi)
{
    SpriteUpdateRecord *record = sprite_update_record_reserve();
    record->kind = 1;
    record->height_rows = (uint16_t)edx;
    record->width_pixels = (uint16_t)ebx;
    record->page_offset = edi - (uint32_t)render_page_base;
    /* The original transparent entry leaves the trailing record words untouched. */
    draw_transparent_sprite_rows(esi, ebx, edx, edi);
}

void restore_sprite_rectangle(SpriteUpdateRecord *record)
{
    uint32_t src = (uint32_t)screen_page_base + record->page_offset;
    uint32_t dst = (uint32_t)render_page_base + record->page_offset;
    uint32_t width = record->width_pixels;
    uint32_t height = record->height_rows;
    uint32_t stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    uint32_t row, x;
    int planar = *(uint16_t *)(void *)vga_state == 1;

    if (!planar) {
        for (row = 0; row < height; row++) {
            uint8_t temp[65536];
            if (width > sizeof temp) return;
            for (x = 0; x < width; x++) temp[x] = sprite_vga_read_linear(src + x);
            for (x = 0; x < width; x++) sprite_vga_write_linear(dst + x, temp[x]);
            src += stride;
            dst += stride;
        }
        return;
    }

    /* VGA write mode 1 copies all four latches at once; one read primes them. */
    for (row = 0; row < height; row++) {
        uint32_t aligned_src = src & ~3u, aligned_dst = dst & ~3u;
        uint32_t prefix = src & 3u;
        uint32_t bytes = (width + prefix + 3u) & ~3u;
        uint32_t offset;
        for (offset = 0; offset < bytes; offset++) {
            uint8_t value = sprite_vga_read_linear(aligned_src + offset);
            sprite_vga_write_linear(aligned_dst + offset, value);
        }
        src += stride;
        dst += stride;
    }
}
