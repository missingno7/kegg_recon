/* m_12f9c_13324.c - literal C translation of asm/m_12f9c_13324.asm.
 * The original's page bases, starts and display offsets are summed in 32-bit arithmetic;
 * planar transfers divide the resulting linear address by four and keep the original row
 * padding and clipping behavior.
 */
#include <stdint.h>
#include "../vhw/vhw.h"

#define VGA_GC_INDEX_DATA_PORT 0x3ceu
#define VGA_SEQ_INDEX_DATA_PORT 0x3c4u
#define VGA_GC_PLANAR_WRITE_MODE_0 0x4005u
#define VGA_GC_PLANAR_WRITE_MODE_1 0x4105u
#define VGA_SEQ_MAP_MASK_ALL_PLANES 0x0f02u
#define VGA_STATE_MODE       0x00u
#define VGA_STATE_PAGE_BASE  0x02u
#define VGA_STATE_PAGE_START 0x12u
#define VGA_STATE_PAGE_SHOW  0x22u
#define VGA_STATE_STRIDE     0x3au
#define VGA_STATE_VIEW_LEFT  0x4au
#define VGA_STATE_VIEW_TOP   0x4eu
#define VGA_STATE_VIEW_RIGHT 0x52u
#define VGA_STATE_VIEW_BOT   0x56u
#define VGA_STATE_GC_MODE    0x60u
#define VGA_STATE_SEQ_MASK   0x61u

extern unsigned char vga_state[];
extern int outpw(int port, int value);

typedef void (*m_12f9c_io_observer)(uint16_t port, uint32_t value, int size);
static m_12f9c_io_observer io_observer;
void m_12f9c_set_io_observer(m_12f9c_io_observer observer) { io_observer = observer; }

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

static int in_vga(uint32_t address)
{
    return address >= 0xa0000u && address < 0xc0000u;
}

static uint8_t read_byte(uint32_t address)
{
    return in_vga(address) ? vga_mem_read8(address) :
           *(volatile uint8_t *)(uintptr_t)address;
}

static void write_byte(uint32_t address, uint8_t value)
{
    if (in_vga(address))
        vga_mem_write8(address, value);
    else
        *(volatile uint8_t *)(uintptr_t)address = value;
}

static void move_forward(uint32_t *source, uint32_t *destination, uint32_t count, unsigned size)
{
    uint32_t n;
    for (n = 0; n < count; n++) {
        uint8_t bytes[4];
        unsigned i;
        for (i = 0; i < size; i++)
            bytes[i] = read_byte(*source + i);
        for (i = 0; i < size; i++)
            write_byte(*destination + i, bytes[i]);
        *source += size;
        *destination += size;
    }
}

void copy_screen_span(uint32_t source_page, uint32_t source_offset,
                      uint32_t destination_page, uint32_t destination_offset,
                      uint32_t byte_count)
{
    uint32_t source, destination, count;
    uint16_t mode = *(uint16_t *)(void *)(vga_state + VGA_STATE_MODE);
    if ((int32_t)byte_count <= 0)
        return;
    if (mode != 0) {
        if (vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
            vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
            out_word(VGA_SEQ_INDEX_DATA_PORT, VGA_SEQ_MAP_MASK_ALL_PLANES);
        }
        if (mode == 1 && vga_state[VGA_STATE_GC_MODE] != 0x41u) {
            vga_state[VGA_STATE_GC_MODE] = 0x41u;
            out_word(VGA_GC_INDEX_DATA_PORT, VGA_GC_PLANAR_WRITE_MODE_1);
        }
        source = (page_origin(source_page) + source_offset) >> 2;
        destination = (page_origin(destination_page) + destination_offset) >> 2;
        move_forward(&source, &destination, byte_count >> 2, 1);
        return;
    }
    if (vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
        vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
        out_word(VGA_SEQ_INDEX_DATA_PORT, VGA_SEQ_MAP_MASK_ALL_PLANES);
    }
    if (vga_state[VGA_STATE_GC_MODE] != 0x40u) {
        vga_state[VGA_STATE_GC_MODE] = 0x40u;
        out_word(VGA_GC_INDEX_DATA_PORT, VGA_GC_PLANAR_WRITE_MODE_0);
    }
    source = page_origin(source_page) + source_offset;
    destination = page_origin(destination_page) + destination_offset;
    count = byte_count >> 2;
    move_forward(&source, &destination, count, 4);
    move_forward(&source, &destination, byte_count & 3u, 1);
}

void copy_screen_span_entry(uint32_t source_page, uint32_t source_offset,
                            uint32_t destination_page, uint32_t destination_offset,
                            uint32_t byte_count)
{
    copy_screen_span(source_page, source_offset, destination_page, destination_offset,
                     byte_count);
}

void copy_clipped_screen_rectangle(uint32_t source_page,
                                   uint32_t left, uint32_t top,
                                   uint32_t right, uint32_t bottom,
                                   uint32_t destination_page, uint32_t destination_x,
                                   uint32_t destination_y)
{
    int32_t l = (int32_t)left, r = (int32_t)right;
    int32_t t = (int32_t)top, b = (int32_t)bottom;
    int32_t dx = (int32_t)destination_x, dy = (int32_t)destination_y;
    uint32_t width, height, stride, source, destination;
    uint16_t mode;
    if (l > r) { int32_t q = l; l = r; r = q; }
    if (t > b) { int32_t q = t; t = b; b = q; }
    if (l < (int32_t)state_u32(VGA_STATE_VIEW_LEFT)) l = (int32_t)state_u32(VGA_STATE_VIEW_LEFT);
    if (r > (int32_t)state_u32(VGA_STATE_VIEW_RIGHT)) r = (int32_t)state_u32(VGA_STATE_VIEW_RIGHT);
    if (t < (int32_t)state_u32(VGA_STATE_VIEW_TOP)) t = (int32_t)state_u32(VGA_STATE_VIEW_TOP);
    if (b > (int32_t)state_u32(VGA_STATE_VIEW_BOT)) b = (int32_t)state_u32(VGA_STATE_VIEW_BOT);
    width = (uint32_t)r - (uint32_t)l + 1u;
    height = (uint32_t)b - (uint32_t)t + 1u;

    if (dx < (int32_t)state_u32(VGA_STATE_VIEW_LEFT))
        dx = (int32_t)state_u32(VGA_STATE_VIEW_LEFT);
    if ((int32_t)(width + (uint32_t)dx - state_u32(VGA_STATE_VIEW_RIGHT)) > 0) {
        int32_t available = (int32_t)(state_u32(VGA_STATE_VIEW_RIGHT) - (uint32_t)dx);
        uint32_t new_width;
        if (available < 0)
            return;
        new_width = (uint32_t)available + 1u;
        r = (int32_t)((uint32_t)r - width + new_width);
        width = new_width;
    }
    if (dy < (int32_t)state_u32(VGA_STATE_VIEW_TOP))
        dy = (int32_t)state_u32(VGA_STATE_VIEW_TOP);
    if ((int32_t)(height + (uint32_t)dy - state_u32(VGA_STATE_VIEW_BOT)) > 0) {
        int32_t available = (int32_t)(state_u32(VGA_STATE_VIEW_BOT) - (uint32_t)dy);
        uint32_t new_height;
        if (available < 0)
            return;
        new_height = (uint32_t)available + 1u;
        b = (int32_t)((uint32_t)b - height + new_height);
        height = new_height;
    }

    mode = *(uint16_t *)(void *)(vga_state + VGA_STATE_MODE);
    if (mode != 0 && vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
        vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
        out_word(VGA_SEQ_INDEX_DATA_PORT, VGA_SEQ_MAP_MASK_ALL_PLANES);
    }
    if (mode == 1 && vga_state[VGA_STATE_GC_MODE] != 0x41u) {
        vga_state[VGA_STATE_GC_MODE] = 0x41u;
        out_word(VGA_GC_INDEX_DATA_PORT, VGA_GC_PLANAR_WRITE_MODE_1);
    }
    if (mode == 0 && vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
        vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
        out_word(VGA_SEQ_INDEX_DATA_PORT, VGA_SEQ_MAP_MASK_ALL_PLANES);
    }
    if (mode == 0 && vga_state[VGA_STATE_GC_MODE] != 0x40u) {
        vga_state[VGA_STATE_GC_MODE] = 0x40u;
        out_word(VGA_GC_INDEX_DATA_PORT, VGA_GC_PLANAR_WRITE_MODE_0);
    }

    stride = state_u32(VGA_STATE_STRIDE);
    source = page_origin(source_page) + (uint32_t)t * stride;
    source += (uint32_t)l;
    destination = page_origin(destination_page) + (uint32_t)dy * stride + (uint32_t)dx;
    if (mode != 0) {
        uint32_t carry = (page_origin(source_page) + (uint32_t)t * stride) & 3u;
        uint32_t source_x = (uint32_t)l;
        uint32_t right_x = (uint32_t)r;
        uint32_t byte_count, row_skip, row;
        source >>= 2;
        destination >>= 2;
        byte_count = (right_x + carry) / 4u - (source_x + carry) / 4u + 1u;
        row_skip = (stride >> 2) - byte_count;
        for (row = 0; row < height; row++) {
            move_forward(&source, &destination, byte_count, 1);
            source += row_skip;
            destination += row_skip;
        }
    } else {
        uint32_t row_skip = stride - width, row;
        for (row = 0; row < height; row++) {
            move_forward(&source, &destination, width >> 2, 4);
            move_forward(&source, &destination, width & 3u, 1);
            source += row_skip;
            destination += row_skip;
        }
    }
}
