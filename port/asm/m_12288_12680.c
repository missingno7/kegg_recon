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
    unsigned pass;

    current_vga_plane_mask = (uint8_t)((0x11u << first_plane) |
                                       (0x11u >> (8u - first_plane)));
    first_vga_plane_mask = current_vga_plane_mask;
    if (!sprite_clip_left && !sprite_clip_right) {
        uint32_t base = pixel_to_planar_address(edi);
        for (pass = 0; pass < 4; pass++) {
            unsigned plane = (first_plane + pass) & 3u;
            unsigned wraps = first_plane + pass >= 4u;
            uint8_t *stream = (uint8_t *)(uintptr_t)(table + 10u +
                                      get16(table + pass * 2u));
            /* kind5_rows_after_top_clip: EDX = (row stride - EBX) >> 2, and kind5_plane_done
             * decrements EBX after every plane, so plane N advances by
             * (stride - (width - N)) >> 2 per row (docs/port/lockstep.md, L2). */
            uint32_t row_advance = (stride - (ebx - pass)) >> 2;
            vga_row_advance = row_advance;
            select_plane(plane);
            draw_unclipped_plane_stream(stream, ebx, edx,
                                        base + wraps, row_advance);
            sprite_source_column = (uint16_t)(8u - 2u * pass);
        }
        current_vga_plane_mask = first_vga_plane_mask;
        return;
    }
    if (sprite_clip_left) {
        /* draw_kind5_left_clipped, literally (docs/port/lockstep.md, L6): the global clip is
         * turned into planar columns (`sar [sprite_clip_left],2`), and whenever the stream
         * index wraps from plane 3 to 0 the remaining planes start one column later
         * (`inc dword ptr [sprite_clip_left]`). */
        vga_plane_index = sprite_clip_left & 3u;
        sprite_clip_left = (uint32_t)((int32_t)sprite_clip_left >> 2);
        vga_row_advance = stride >> 2;
        for (pass = 0; pass < 4; pass++) {
            unsigned plane = (first_plane + pass) & 3u;
            uint32_t wraps = first_plane + pass >= 4u;
            uint8_t *stream = (uint8_t *)(uintptr_t)(table + 10u +
                                      get16(table + vga_plane_index * 2u));
            select_plane(plane);
            draw_left_clipped_plane_stream(stream, edx,
                                           pixel_to_planar_address(edi) + wraps,
                                           sprite_clip_left, stride >> 2);
            if (++vga_plane_index == 4u) {
                vga_plane_index = 0;
                sprite_clip_left++;
            }
        }
        current_vga_plane_mask = first_vga_plane_mask;
        return;
    }
    /* draw_kind5_right_clipped, literally (docs/port/lockstep.md, L7): streams in plane pass
     * order; plane N shows (visible_width - N + 3) >> 2 planar columns, kept in the globals
     * visible_sprite_width / sprite_row_width_remaining as the original does. */
    sprite_source_column = 0x0a;
    sprite_row_width_remaining = visible_sprite_width;
    vga_row_advance = stride >> 2;
    for (pass = 0; pass < 4; pass++) {
        unsigned plane = (first_plane + pass) & 3u;
        uint32_t wraps = first_plane + pass >= 4u;
        const uint8_t *esi = (const uint8_t *)(uintptr_t)(table + 10u +
                                 get16(table + (10u - sprite_source_column)));
        uint32_t row_address = pixel_to_planar_address(edi) + wraps, row;
        select_plane(plane);
        sprite_source_column = (uint16_t)(sprite_source_column - 2u);
        esi = sprite_skip_rle_rows((uint8_t *)(uintptr_t)esi, sprite_clip_top);
        visible_sprite_width = (uint32_t)((int32_t)(sprite_row_width_remaining + 3u) >> 2);
        sprite_row_width_remaining--;
        for (row = 0; row < edx; row++) {
            uint32_t dest = row_address;
            unsigned runs = *esi++;
            while (runs--) {
                int8_t run = (int8_t)*esi++;
                if (run < 0) {
                    dest += (uint32_t)(-(int)run);
                } else {
                    int32_t beyond = (int32_t)(dest - row_address - visible_sprite_width);
                    uint32_t count = (uint32_t)run, rest = 0, x;
                    if (beyond >= 0) {
                        esi += count;          /* kind5_right_run_starts_beyond_clip */
                        continue;
                    }
                    beyond += (int32_t)count;
                    if (beyond >= 0) {
                        count -= (uint32_t)beyond;
                        rest = (uint32_t)beyond;
                    }
                    for (x = 0; x < count; x++)
                        sprite_vga_write_linear(dest + x, esi[x]);
                    esi += count + rest;
                    dest += count;
                }
            }
            row_address += vga_row_advance;
        }
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
    uint32_t stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    if (record->clip_left || record->clip_width) {
        /* Clipped record: copy the saved rectangle from the screen page (latch copies). */
        uint32_t source_pixel = (uint32_t)screen_page_base + record->page_offset;
        uint32_t dest_pixel = (uint32_t)render_page_base + record->page_offset;
        uint32_t bytes = ((dest_pixel & 3u) + record->width_pixels + 3u) >> 2;
        uint32_t y, x;
        for (y = 0; y < record->height_rows; y++) {
            uint32_t src = pixel_to_planar_address(source_pixel + y * stride);
            uint32_t dst = pixel_to_planar_address(dest_pixel + y * stride);
            for (x = 0; x < bytes; x++) {
                uint8_t value = sprite_vga_read_linear(src + x);
                sprite_vga_write_linear(dst + x, value);
            }
        }
        return;
    }
    /* kind5_restore_encoded_background, literally (docs/port/lockstep.md, L4): the record's
     * stream is the sprite's pixel-run stream (run lengths only, no pixel bytes). Skip the
     * clipped top rows (`lodsb; add esi,eax`), then per row copy every opaque run's planar
     * bytes ((x & 3) + run + 3) >> 2 from the screen page to the render page. */
    {
        uint32_t delta = (uint32_t)screen_page_base - (uint32_t)render_page_base;
        uint32_t ebp = record->page_offset + (uint32_t)render_page_base;
        uint8_t dl = (uint8_t)-(uint8_t)record->height_rows;
        const uint8_t *esi = (const uint8_t *)(uintptr_t)record->source_stream;
        const uint8_t *ebx;
        uint32_t eax = 0, ecx = record->clip_top_rows;
        background_plane_delta = delta;
        while (ecx) {                              /* lodsb; add esi,eax; loop */
            eax = *esi++;
            esi += eax;
            ecx--;
        }
        ebx = esi;
        do {                                       /* kind5_restore_row_start */
            uint8_t dh = (uint8_t)-*ebx++;
            ecx = ebp;
            do {                                   /* kind5_restore_decode_run */
                uint8_t al = *ebx++;
                eax = (eax & ~0xffu) | al;
                if ((int8_t)al < 0) {
                    eax = (eax & ~0xffu) | (uint8_t)-al;
                    ecx += eax;
                } else {
                    uint32_t edi = ecx, src, n;
                    ecx = ((ecx & 3u) + eax + 3u) >> 2;
                    eax += edi;
                    src = pixel_to_planar_address(delta + edi);
                    edi = pixel_to_planar_address(edi);
                    for (n = 0; n < ecx; n++)       /* rep movsb (write mode 1: latches) */
                        sprite_vga_write_linear(edi + n, sprite_vga_read_linear(src + n));
                    ecx = eax;                     /* xchg eax,ecx after rep: ECX = 0 */
                    eax = 0;
                }
            } while (++dh);
            ebp += stride;
        } while (++dl);
    }
}
