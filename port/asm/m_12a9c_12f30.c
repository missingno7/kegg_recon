/* m_12a9c_12f30.c - C translation of asm/m_12a9c_12f30.asm.
 *
 * The three internal render callbacks use the common ESI/EBX/EDX/EDI interface in
 * sprite_records.h. VGA addresses remain virtual linear addresses and every access to the
 * A0000h aperture goes through vga_mem_*.
 */
#include <stdint.h>
#include <string.h>
#include "sprite_records.h"
#include "../vhw/vhw.h"

#define VGA_GC_INDEX_PORT 0x3ce
#define VGA_SEQ_INDEX_PORT 0x3c4
#define VGA_GC_MODE0_COMMAND 0x4005
#define VGA_GC_MODE1_COMMAND 0x4105
#define VGA_SEQ_ALL_PLANES_COMMAND 0x0f02
#define VGA_MODE0_CACHE 0x40
#define VGA_MODE1_CACHE 0x41
#define VGA_ALL_PLANES 0x0f
#define SPRITE_ERROR_DIMENSIONS 0x401
#define SPRITE_ERROR_ORIGIN 0x402
#define SPRITE_ERROR_SIZE 0x405
#define SPRITE_ERROR_MODE 0x406

extern uint8_t vga_state[];
extern short image_buffer_error_code;
extern short image_color_depth;
extern short drawpage, page2;
extern int render_page_base, screen_page_base;
extern unsigned char *active_video_page_buffer;
extern unsigned char *image_buffer_cursor;

/* Public _DATA labels in their original declaration order. */
uint32_t sprite_record_cursor[2];
uint32_t vga_draw_origin;
uint32_t vga_row_advance;
uint32_t background_plane_delta;
uint16_t sprite_record_flags;
uint16_t sprite_source_column;
uint32_t sprite_clip_top;
uint32_t sprite_clip_bottom;
uint32_t sprite_clip_left;
uint32_t sprite_clip_right;
uint32_t visible_sprite_width;
uint32_t vga_plane_index;
uint32_t sprite_row_width_remaining;
uint8_t sprite_row_width_scratch[14];
uint8_t current_vga_plane_mask;
uint8_t first_vga_plane_mask;
uint8_t sprite_operation_dispatch_table[8];
uint32_t sprite_render_mode_table[20];
uint32_t sprite_origin_x;
uint32_t sprite_origin_y;
uint8_t sprite_command_cursor[10];

static uint8_t *command_cursor;
static uint8_t *update_cursor;

void sprite_vga_write_linear(uint32_t address, uint8_t value)
{
    if (address >= 0xa0000u && address < 0xc0000u)
        vga_mem_write8(address, value);
    else
        *(volatile uint8_t *)(uintptr_t)address = value;
}

uint8_t sprite_vga_read_linear(uint32_t address)
{
    if (address >= 0xa0000u && address < 0xc0000u)
        return vga_mem_read8(address);
    return *(volatile uint8_t *)(uintptr_t)address;
}

SpriteUpdateRecord *sprite_update_record_reserve(void)
{
    SpriteUpdateRecord *record = (SpriteUpdateRecord *)(uintptr_t)sprite_record_cursor[0];
    sprite_record_cursor[0] += sizeof(*record);
    return record;
}

static uint32_t page_address(unsigned page)
{
    const uint32_t *bases = (const uint32_t *)(const void *)(vga_state + 2);
    const uint32_t *starts = (const uint32_t *)(const void *)(vga_state + 0x12);
    const uint32_t *displays = (const uint32_t *)(const void *)(vga_state + 0x22);
    return bases[page] + starts[page] + displays[page];
}

static void set_gc_cache(uint8_t mode)
{
    if (vga_state[0x60] != mode) {
        vga_state[0x60] = mode;
        sprite_outpw(VGA_GC_INDEX_PORT, mode == VGA_MODE1_CACHE ? VGA_GC_MODE1_COMMAND
                                                                 : VGA_GC_MODE0_COMMAND);
    }
}

static void set_all_planes(void)
{
    vga_state[0x61] = VGA_ALL_PLANES;
    sprite_outpw(VGA_SEQ_INDEX_PORT, VGA_SEQ_ALL_PLANES_COMMAND);
}

static void set_replay_registers(void)
{
    if (vga_state[0] == 1) {
        if (vga_state[0x61] != VGA_ALL_PLANES)
            set_all_planes();
        set_gc_cache(VGA_MODE1_CACHE);
    } else {
        if (vga_state[0x61] != VGA_ALL_PLANES)
            set_all_planes();
        set_gc_cache(VGA_MODE0_CACHE);
    }
}

static void finish_replay_registers(void)
{
    if (vga_state[0] != 0) {
        set_all_planes();
        set_gc_cache(VGA_MODE0_CACHE);
    }
}

static void reject_command(uint16_t error)
{
    image_buffer_error_code = (short)error;
    if (command_cursor)
        *(uint32_t *)(void *)command_cursor = 0;
}

uint8_t *sprite_skip_rle_rows(uint8_t *source, unsigned rows)
{
    while (rows--) {
        unsigned runs = *source++;
        while (runs--) {
            int8_t run = (int8_t)*source++;
            if (run > 0)
                source += (uint8_t)run;
        }
    }
    return source;
}

static void invoke_sprite(const uint8_t *record, int32_t x, int32_t y)
{
    const SpriteBobHeader *bob = (const SpriteBobHeader *)(const void *)record;
    uint32_t width = bob->width, height = bob->height;
    uint16_t kind = bob->flags & 7u;
    uint32_t combined;
    uint8_t *source;
    int32_t left, top, right, bottom;
    uint32_t visible_w, visible_h, stride = *(uint32_t *)(void *)(vga_state + 0x3a);
    uint32_t dest;
    SpriteRegisterRenderer renderer = NULL;

    if (width > stride * 2u || height > *(uint32_t *)(void *)(vga_state + 0x3e) * 2u) {
        reject_command(SPRITE_ERROR_DIMENSIONS);
        return;
    }
    if (sprite_record_flags & 1u) {
        if (kind & 2u) {
            x += bob->alternate_x_offset;
            y += bob->alternate_y_offset;
        }
    } else if (kind & 1u) {
        x += bob->x_offset;
        y += bob->y_offset;
    }
    combined = (uint32_t)(x | y);
    if ((int32_t)combined > 0x7d00 || (int32_t)combined < -0x7d00) {
        reject_command(SPRITE_ERROR_ORIGIN);
        return;
    }
    if (bob->payload_offset > 0x1000u) {
        reject_command(SPRITE_ERROR_SIZE);
        return;
    }
    source = (uint8_t *)(uintptr_t)record + bob->payload_offset;
    if (kind == 5 && vga_state[0] == 0) {
        reject_command(SPRITE_ERROR_MODE);
        return;
    }

    left = *(int32_t *)(void *)(vga_state + 0x4a);
    top = *(int32_t *)(void *)(vga_state + 0x4e);
    right = *(int32_t *)(void *)(vga_state + 0x52);
    bottom = *(int32_t *)(void *)(vga_state + 0x56);
    sprite_clip_top = sprite_clip_bottom = sprite_clip_left = sprite_clip_right = 0;
    visible_sprite_width = 0;
    if (y < top) {
        sprite_clip_top = (uint32_t)(top - y);
        height -= sprite_clip_top;
        y = top;
        if ((int32_t)height <= 0) return;
    }
    if ((int64_t)y + height - 1 > bottom) {
        sprite_clip_bottom = (uint32_t)((int64_t)y + height - 1 - bottom);
        height -= sprite_clip_bottom;
        if ((int32_t)height <= 0) return;
    }
    if (x < left) {
        sprite_clip_left = (uint32_t)(left - x);
        width -= sprite_clip_left;
        x = left;
        if ((int32_t)width <= 0) return;
    }
    if ((int64_t)x + width - 1 > right) {
        sprite_clip_right = (uint32_t)((int64_t)x + width - 1 - right);
        width -= sprite_clip_right;
        if ((int32_t)width <= 0) return;
        visible_sprite_width = width;
    }
    visible_w = width;
    visible_h = height;
    dest = vga_draw_origin + (uint32_t)y * stride + (uint32_t)x;

    if (kind == 1) {
        renderer = image_color_depth == 4 ? render_transparent_sprite_record_entry
                                          : draw_transparent_sprite_rows;
    } else if (kind == 3) {
        renderer = image_color_depth == 4 ? render_sprite_record_kind_3_entry
                                          : render_sprite_record_kind_3_draw;
    } else if (kind == 5) {
        renderer = image_color_depth == 4 ? render_sprite_record_kind_5_entry
                                          : render_sprite_record_kind_5_draw;
    }
    if (renderer) {
        /* EBX/EBP carry the clipped extent; the clip globals retain the hidden edges. */
        renderer(source, visible_w, visible_h, dest);
    }
}

void clip_and_dispatch_sprite_record(const uint8_t *record, int32_t x, int32_t y)
{
    invoke_sprite(record, x, y);
}

void process_sprite_update_list(int32_t x, int32_t y, uint8_t *commands, uint8_t *updates)
{
    uint32_t destination, source;
    set_gc_cache(VGA_MODE0_CACHE);
    destination = page_address((uint16_t)drawpage);
    source = page_address((uint16_t)page2);
    render_page_base = (int)destination;
    screen_page_base = (int)source;
    vga_draw_origin = destination + (uint32_t)x +
                      (uint32_t)(*(uint32_t *)(void *)(vga_state + 0x3a) * y);
    sprite_origin_x = (uint32_t)x;
    sprite_origin_y = (uint32_t)y;
    command_cursor = commands;
    update_cursor = updates;
    sprite_record_cursor[0] = (uint32_t)(uintptr_t)updates;
    memcpy(sprite_command_cursor, &command_cursor, sizeof command_cursor);
    for (;;) {
        uint8_t *sprite;
        int32_t sx, sy;
        int16_t cx, cy;
        uint16_t flags;
        command_cursor += 10;
        sprite = (uint8_t *)(uintptr_t)*(uint32_t *)(void *)(command_cursor - 10);
        if (!sprite)
            break;
        cx = *(int16_t *)(void *)(command_cursor - 6);
        cy = *(int16_t *)(void *)(command_cursor - 4);
        flags = *(uint16_t *)(void *)(command_cursor - 2);
        sx = (int32_t)cx - (int32_t)sprite_origin_x;
        sy = (int32_t)cy - (int32_t)sprite_origin_y;
        sprite_record_flags = flags;
        invoke_sprite(sprite, sx, sy);
    }
    set_all_planes();
    update_cursor = (uint8_t *)(uintptr_t)sprite_record_cursor[0];
    active_video_page_buffer = update_cursor;
    image_buffer_cursor = command_cursor;
    (void)update_cursor;
}
void process_sprite_update_list_entry(int32_t x, int32_t y, uint8_t *commands, uint8_t *updates)
{
    process_sprite_update_list(x, y, commands, updates);
}

static void replay_record(SpriteUpdateRecord *record)
{
    switch (record->kind) {
    case 1: restore_sprite_rectangle(record); break;
    case 3: restore_sprite_background_from_record(record); break;
    case 5: restore_sprite_background_record(record); break;
    default: break;
    }
}

void replay_sprite_update_list(int unused)
{
    uint8_t *records;
    set_replay_registers();
    render_page_base = (int)page_address((uint16_t)drawpage);
    screen_page_base = (int)page_address((uint16_t)page2);
    if (image_color_depth == 4) {
        records = active_video_page_buffer;
        sprite_record_cursor[0] = (uint32_t)(uintptr_t)records;
        for (;;) {
            SpriteUpdateRecord *record = (SpriteUpdateRecord *)(void *)records;
            if (!record->kind) break;
            replay_record(record);
            records += 20;
        }
    }
    finish_replay_registers();
    (void)unused;
}
void replay_sprite_update_list_entry(int unused) { replay_sprite_update_list(unused); }

void draw_bob_sprite(const uint8_t *record, int32_t x, int32_t y)
{
    uint32_t render = page_address((uint16_t)drawpage);
    uint32_t screen = page_address((uint16_t)page2);
    set_gc_cache(VGA_MODE0_CACHE);
    render_page_base = (int)render;
    screen_page_base = (int)screen;
    vga_draw_origin = render;
    sprite_record_cursor[0] = (uint32_t)(uintptr_t)active_video_page_buffer;
    sprite_record_flags = 0;
    command_cursor = NULL;
    invoke_sprite(record, x, y);
    active_video_page_buffer = (uint8_t *)(uintptr_t)sprite_record_cursor[0];
    set_all_planes();
}
void draw_bob_sprite_entry(const uint8_t *record, int32_t x, int32_t y)
{
    draw_bob_sprite(record, x, y);
}
