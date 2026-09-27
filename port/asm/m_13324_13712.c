/* m_13324_13712.c - literal C translation of asm/m_13324_13712.asm.
 * VGA pixels, spans and clipped fills.  Register aliases and the unusual read result in
 * read_vga_pixel are retained; addresses in A0000h..BFFFFh go through the virtual VGA.
 */
#include <stdint.h>
#include "../vhw/vhw.h"

#define VGA_SEQ_INDEX_PORT 0x3c4u
#define VGA_GC_INDEX_PORT  0x3ceu
#define VGA_SEQ_ALL_PLANES 0x0f02u
#define VGA_GC_PLANAR_MODE 0x4005u
#define VGA_STATE_MODE       0x00u
#define VGA_STATE_PAGE_BASE  0x02u
#define VGA_STATE_PAGE_START 0x12u
#define VGA_STATE_PAGE_SHOW  0x22u
#define VGA_STATE_STRIDE     0x3au
#define VGA_STATE_CLIP_LEFT  0x4au
#define VGA_STATE_CLIP_TOP   0x4eu
#define VGA_STATE_CLIP_RIGHT 0x52u
#define VGA_STATE_CLIP_BOT   0x56u
#define VGA_STATE_GC_MODE    0x60u
#define VGA_STATE_SEQ_MASK   0x61u
#define VGA_STATE_READ_PLANE 0x62u

extern unsigned char vga_state[];
extern short page_idx;
extern short drawpage;
extern int outpw(int port, int value);
extern void fill_planar_video_rows(uint32_t page, uint32_t x, uint32_t y,
                                   uint32_t width, uint32_t height, uint32_t color);

typedef void (*m_13324_io_observer)(uint16_t port, uint32_t value, int size);
static m_13324_io_observer io_observer;

/* Oracle-only tracing is opt-in; ordinary game builds pay only a null check. */
void m_13324_set_io_observer(m_13324_io_observer observer) { io_observer = observer; }

static void out_word(uint16_t port, uint16_t value)
{
    outpw((int)port, (int)value);
    if (io_observer)
        io_observer(port, value, 2);
}

static uint32_t state_u32(unsigned offset)
{
    const unsigned char *p = vga_state + offset;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t state_page_origin(uint32_t page)
{
    uint32_t i = page << 2;
    return state_u32(VGA_STATE_PAGE_BASE + i) +
           state_u32(VGA_STATE_PAGE_START + i) +
           state_u32(VGA_STATE_PAGE_SHOW + i);
}

static int in_vga(uint32_t address)
{
    return address >= 0xa0000u && address < 0xc0000u;
}

static uint8_t read_byte(uint32_t address)
{
    if (in_vga(address))
        return vga_mem_read8(address);
    return *(volatile uint8_t *)(uintptr_t)address;
}

static void write_byte(uint32_t address, uint8_t value)
{
    if (in_vga(address))
        vga_mem_write8(address, value);
    else
        *(volatile uint8_t *)(uintptr_t)address = value;
}

static uint32_t repeated_byte(uint8_t value)
{
    return (uint32_t)value * 0x01010101u;
}

/* `lodsb` only changes AL, so callers receive the address-derived upper EAX bits too. */
uint32_t read_vga_pixel(uint32_t x, uint32_t y)
{
    uint32_t eax, address, plane, unshifted_address;
    int32_t sx = (int32_t)x, sy = (int32_t)y;
    if (sx < (int32_t)state_u32(VGA_STATE_CLIP_LEFT))
        x = state_u32(VGA_STATE_CLIP_LEFT);
    if ((int32_t)x > (int32_t)state_u32(VGA_STATE_CLIP_RIGHT))
        x = state_u32(VGA_STATE_CLIP_RIGHT);
    if (sy < (int32_t)state_u32(VGA_STATE_CLIP_TOP))
        y = state_u32(VGA_STATE_CLIP_TOP);
    if ((int32_t)y > (int32_t)state_u32(VGA_STATE_CLIP_BOT))
        y = state_u32(VGA_STATE_CLIP_BOT);

    address = state_page_origin((uint16_t)page_idx) +
              y * state_u32(VGA_STATE_STRIDE) + x;
    if (*(uint16_t *)(void *)(vga_state + VGA_STATE_MODE) != 0) {
        unshifted_address = address;
        plane = address & 3u;
        vga_state[VGA_STATE_READ_PLANE] = (uint8_t)plane;
        out_word(VGA_GC_INDEX_PORT, (uint16_t)((plane << 8) | 4u));
        address >>= 2;
        eax = (unshifted_address << 8) & 0xffffff00u;
        eax = (eax & 0xffff00ffu) | (plane << 8); /* mov eax,esi; shl eax,8; and ah,3 */
        eax = (eax & 0xffffff00u) | read_byte(address);
    } else {
        eax = ((y * state_u32(VGA_STATE_STRIDE) + x) & 0xffffff00u) |
              read_byte(address);
    }
    return eax;
}

uint32_t read_vga_pixel_entry(uint32_t x, uint32_t y)
{
    return read_vga_pixel(x, y);
}

void write_vga_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t address, page, plane;
    if ((int32_t)x < (int32_t)state_u32(VGA_STATE_CLIP_LEFT) ||
        (int32_t)x > (int32_t)state_u32(VGA_STATE_CLIP_RIGHT) ||
        (int32_t)y < (int32_t)state_u32(VGA_STATE_CLIP_TOP) ||
        (int32_t)y > (int32_t)state_u32(VGA_STATE_CLIP_BOT))
        return;
    if (vga_state[VGA_STATE_GC_MODE] != 0x40u) {
        vga_state[VGA_STATE_GC_MODE] = 0x40u;
        out_word(VGA_GC_INDEX_PORT, VGA_GC_PLANAR_MODE);
    }
    page = (uint16_t)drawpage;
    address = state_page_origin(page) + y * state_u32(VGA_STATE_STRIDE) + x;
    if (*(uint16_t *)(void *)(vga_state + VGA_STATE_MODE) != 0) {
        plane = address & 3u;
        address >>= 2;
        vga_state[VGA_STATE_SEQ_MASK] = (uint8_t)(1u << plane);
        out_word(VGA_SEQ_INDEX_PORT,
                 (uint16_t)(((1u << plane) << 8) | 2u));
    }
    write_byte(address, (uint8_t)color);
}

void write_vga_pixel_entry(uint32_t x, uint32_t y, uint32_t color)
{
    write_vga_pixel(x, y, color);
}

void fill_vga_span(uint32_t page, uint32_t x, uint32_t length, uint32_t color)
{
    uint32_t address, i, count, value = repeated_byte((uint8_t)color);
    if ((int32_t)length <= 0)
        return;
    if (*(uint16_t *)(void *)(vga_state + VGA_STATE_MODE) != 0) {
        if (vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
            vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
            out_word(VGA_SEQ_INDEX_PORT, VGA_SEQ_ALL_PLANES);
        }
        if (vga_state[VGA_STATE_GC_MODE] != 0x40u) {
            vga_state[VGA_STATE_GC_MODE] = 0x40u;
            out_word(VGA_GC_INDEX_PORT, VGA_GC_PLANAR_MODE);
        }
        count = (length >> 4) * 4u + ((length >> 2) & 3u);
        address = (state_page_origin(page) + x) >> 2;
    } else {
        if (vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
            vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
            out_word(VGA_SEQ_INDEX_PORT, VGA_SEQ_ALL_PLANES);
        }
        if (vga_state[VGA_STATE_GC_MODE] != 0x40u) {
            vga_state[VGA_STATE_GC_MODE] = 0x40u;
            out_word(VGA_GC_INDEX_PORT, VGA_GC_PLANAR_MODE);
        }
        count = length;
        address = state_page_origin(page);
    }
    for (i = 0; i < count; i++)
        write_byte(address + i, (uint8_t)(value >> ((i & 3u) * 8u)));
}

void fill_vga_span_entry(uint32_t page, uint32_t x, uint32_t length, uint32_t color)
{
    fill_vga_span(page, x, length, color);
}

void fill_clipped_vga_rectangle(uint32_t page, uint32_t left, uint32_t top,
                                uint32_t right, uint32_t bottom, uint32_t color)
{
    uint32_t width, height, start, stride, row_bytes, row_skip, row, x_carry;
    uint32_t value = (uint8_t)color;
    int32_t l = (int32_t)left, r = (int32_t)right;
    int32_t t = (int32_t)top, b = (int32_t)bottom;
    if (l > r) { int32_t q = l; l = r; r = q; }
    if (t > b) { int32_t q = t; t = b; b = q; }
    if (l < (int32_t)state_u32(VGA_STATE_CLIP_LEFT)) l = (int32_t)state_u32(VGA_STATE_CLIP_LEFT);
    if (r > (int32_t)state_u32(VGA_STATE_CLIP_RIGHT)) r = (int32_t)state_u32(VGA_STATE_CLIP_RIGHT);
    if (t < (int32_t)state_u32(VGA_STATE_CLIP_TOP)) t = (int32_t)state_u32(VGA_STATE_CLIP_TOP);
    if (b > (int32_t)state_u32(VGA_STATE_CLIP_BOT)) b = (int32_t)state_u32(VGA_STATE_CLIP_BOT);
    width = (uint32_t)r - (uint32_t)l + 1u;
    height = (uint32_t)b - (uint32_t)t + 1u;
    if ((int32_t)width <= 0 || (int32_t)height <= 0)
        return;
    if (*(uint16_t *)(void *)(vga_state + VGA_STATE_MODE) == 0) {
        fill_planar_video_rows(page, (uint32_t)l, (uint32_t)t, width, height, value);
        return;
    }
    if (vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
        vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
        out_word(VGA_SEQ_INDEX_PORT, VGA_SEQ_ALL_PLANES);
    }
    if (vga_state[VGA_STATE_GC_MODE] != 0x40u) {
        vga_state[VGA_STATE_GC_MODE] = 0x40u;
        out_word(VGA_GC_INDEX_PORT, VGA_GC_PLANAR_MODE);
    }
    stride = state_u32(VGA_STATE_STRIDE);
    start = state_page_origin(page) + (uint32_t)t * stride;
    x_carry = start & 3u;
    start = (start + (uint32_t)l) >> 2;
    row_bytes = ((uint32_t)r + x_carry) / 4u - ((uint32_t)l + x_carry) / 4u + 1u;
    row_skip = (stride >> 2) - row_bytes;
    for (row = 0; row < height; row++) {
        uint32_t i;
        for (i = 0; i < row_bytes; i++)
            write_byte(start + i, (uint8_t)value);
        start += row_bytes + row_skip;
    }
}
