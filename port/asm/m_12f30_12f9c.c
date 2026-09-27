/* m_12f30_12f9c.c - literal C translation of asm/m_12f30_12f9c.asm.
 * The scanline is split by selecting map masks 1,2,4,8 and advancing the chunky source by
 * four bytes per destination byte.  The original LOOP underflows when byte_count/4 is zero;
 * the tested game path uses nonzero groups, while counts 0..3 remain outside the test domain.
 */
#include <stdint.h>
#include "../vhw/vhw.h"

#define VGA_GC_INDEX_DATA_PORT 0x3ceu
#define VGA_SEQ_INDEX_DATA_PORT 0x3c4u
#define VGA_GC_PLANAR_WRITE_MODE_0 0x4005u
#define VGA_SEQ_MAP_MASK_ALL_PLANES 0x0f02u
#define VGA_STATE_GC_MODE 0x60u
#define VGA_STATE_SEQ_MASK 0x61u

extern unsigned char vga_state[];
extern int outpw(int port, int value);

typedef void (*m_12f30_io_observer)(uint16_t port, uint32_t value, int size);
static m_12f30_io_observer io_observer;
void m_12f30_set_io_observer(m_12f30_io_observer observer) { io_observer = observer; }

static void out_word(uint16_t port, uint16_t value)
{
    outpw((int)port, (int)value);
    if (io_observer)
        io_observer(port, value, 2);
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

void copy_chunky_scanline_to_vga(uint32_t source, uint32_t destination, uint32_t byte_count)
{
    uint32_t source_start = source;
    uint8_t mask;
    if (vga_state[VGA_STATE_GC_MODE] != 0x40u) {
        vga_state[VGA_STATE_GC_MODE] = 0x40u;
        out_word(VGA_GC_INDEX_DATA_PORT, VGA_GC_PLANAR_WRITE_MODE_0);
    }
    for (mask = 1; mask != 0x10u; mask = (uint8_t)(mask << 1)) {
        uint32_t esi = source_start, edi = destination, count = byte_count >> 2;
        vga_state[VGA_STATE_SEQ_MASK] = mask;
        out_word(VGA_SEQ_INDEX_DATA_PORT, (uint16_t)(((uint16_t)mask << 8) | 2u));
        /* MOVSB; ADD ESI,3; LOOP -- one source pixel in each four-byte group. */
        while (count--) {
            write_byte(edi++, read_byte(esi));
            esi += 4u;
        }
        source_start++;
    }
    if (vga_state[VGA_STATE_SEQ_MASK] != 0x0fu) {
        vga_state[VGA_STATE_SEQ_MASK] = 0x0fu;
        out_word(VGA_SEQ_INDEX_DATA_PORT, VGA_SEQ_MAP_MASK_ALL_PLANES);
    }
}

void copy_chunky_scanline_to_vga_entry(uint32_t source, uint32_t destination,
                                       uint32_t byte_count)
{
    copy_chunky_scanline_to_vga(source, destination, byte_count);
}
