/* m_13712_137a8.c - literal C translation of asm/m_13712_137a8.asm.
 * fill_planar_video_rows is a tail-jump target in the original: the former assembly
 * inherited the rectangle routine's EBP frame.  This port documents a C adapter signature
 * (page,x,y,width,height,color) for its translated caller.
 */
#include <stdint.h>
#include "../vhw/vhw.h"

#define VGA_SEQ_INDEX_PORT 0x3c4u
#define VGA_GC_INDEX_PORT  0x3ceu
#define VGA_SEQ_ALL_PLANES 0x0f02u
#define VGA_GC_PLANAR_MODE 0x4005u
#define VGA_STATE_PAGE_BASE  0x02u
#define VGA_STATE_PAGE_START 0x12u
#define VGA_STATE_PAGE_SHOW  0x22u
#define VGA_STATE_STRIDE     0x3au
#define VGA_STATE_GC_MODE    0x60u
#define VGA_STATE_SEQ_MASK   0x61u

extern unsigned char vga_state[];
extern int outpw(int port, int value);

typedef void (*m_13712_io_observer)(uint16_t port, uint32_t value, int size);
static m_13712_io_observer io_observer;
void m_13712_set_io_observer(m_13712_io_observer observer) { io_observer = observer; }

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

static uint32_t page_origin(uint32_t page)
{
    uint32_t i = page << 2;
    return state_u32(VGA_STATE_PAGE_BASE + i) + state_u32(VGA_STATE_PAGE_START + i) +
           state_u32(VGA_STATE_PAGE_SHOW + i);
}

static void store_byte(uint32_t address, uint8_t value)
{
    if (address >= 0xa0000u && address < 0xc0000u)
        vga_mem_write8(address, value);
    else
        *(volatile uint8_t *)(uintptr_t)address = value;
}

/* C adapter for the original inherited-frame tail-jump target. */
void fill_planar_video_rows(uint32_t page, uint32_t x, uint32_t y,
                            uint32_t width, uint32_t height, uint32_t color)
{
    uint32_t stride, address, row, dwords, remainder, skip;
    uint32_t value = (uint8_t)color;
    if (vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
        vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
        out_word(VGA_SEQ_INDEX_PORT, VGA_SEQ_ALL_PLANES);
    }
    if (vga_state[VGA_STATE_GC_MODE] != 0x40u) {
        vga_state[VGA_STATE_GC_MODE] = 0x40u;
        out_word(VGA_GC_INDEX_PORT, VGA_GC_PLANAR_MODE);
    }
    stride = state_u32(VGA_STATE_STRIDE);
    address = page_origin(page) + y * stride + x;
    dwords = width >> 2;
    remainder = width & 3u;
    skip = stride - width;
    for (row = 0; row < height; row++) {
        uint32_t i;
        for (i = 0; i < dwords * 4u; i++)
            store_byte(address + i, (uint8_t)value);
        for (i = 0; i < remainder; i++)
            store_byte(address + dwords * 4u + i, (uint8_t)value);
        address += dwords * 4u + remainder + skip;
    }
}
