/* Shared sprite record layouts and register-entry interface for the A6 translations. */
#ifndef KE_SPRITE_RECORDS_H
#define KE_SPRITE_RECORDS_H

#include <stdint.h>
#include "../vhw/vhw.h"

#if defined(__GNUC__)
#define KE_SPRITE_PACKED __attribute__((packed))
#else
#define KE_SPRITE_PACKED
#endif

unsigned int outpw(int port, int value);
#if defined(KE_ORACLE_TRACE)
void oracle_trace_add(uint8_t kind, uint16_t port, uint32_t value, int size);
#endif

static inline void sprite_outpw(uint16_t port, uint16_t value)
{
    (void)outpw(port, value);
#if defined(KE_ORACLE_TRACE)
    oracle_trace_add('O', port, value, 2);
#endif
}

typedef struct KE_SPRITE_PACKED SpriteUpdateRecord {
    uint16_t kind;
    uint16_t height_rows;
    uint16_t width_pixels;
    uint32_t source_stream;
    uint32_t page_offset;
    uint16_t clip_left;
    uint16_t clip_width;
    uint16_t clip_top_rows;
} SpriteUpdateRecord;

typedef struct KE_SPRITE_PACKED SpriteBobHeader {
    uint16_t prefix;
    uint16_t width;
    uint16_t height;
    uint16_t payload_offset;
    uint16_t flags;
    int16_t x_offset;
    int16_t y_offset;
    int16_t alternate_x_offset;
    int16_t alternate_y_offset;
} SpriteBobHeader;

/* TASM's internal entries receive ESI, EBX, EDX and EDI in that order. */
typedef void (*SpriteRegisterRenderer)(const uint8_t *esi_source, uint32_t ebx_width,
                                       uint32_t edx_height, uint32_t edi_destination);

_Static_assert(sizeof(SpriteUpdateRecord) == 20, "historical sprite update record size");
_Static_assert(sizeof(SpriteBobHeader) == 18, "historical BOB header size");

void render_sprite_record_kind_5_entry(const uint8_t *, uint32_t, uint32_t, uint32_t);
void render_sprite_record_kind_5_draw(const uint8_t *, uint32_t, uint32_t, uint32_t);
void restore_sprite_background_record(SpriteUpdateRecord *);
void render_sprite_record_kind_3_entry(const uint8_t *, uint32_t, uint32_t, uint32_t);
void render_sprite_record_kind_3_draw(const uint8_t *, uint32_t, uint32_t, uint32_t);
void restore_sprite_background_from_record(SpriteUpdateRecord *);
void render_transparent_sprite_record_entry(const uint8_t *, uint32_t, uint32_t, uint32_t);
void draw_transparent_sprite_rows(const uint8_t *, uint32_t, uint32_t, uint32_t);
void restore_sprite_rectangle(SpriteUpdateRecord *);

SpriteUpdateRecord *sprite_update_record_reserve(void);
uint8_t sprite_vga_read_linear(uint32_t address);
void sprite_vga_write_linear(uint32_t address, uint8_t value);
uint8_t *sprite_skip_rle_rows(uint8_t *source, unsigned rows);

#endif
