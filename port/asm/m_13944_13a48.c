/* m_13944_13a48.c - literal translation of asm/m_13944_13a48.asm.
 *
 * The indexed register helpers preserve the original port order and byte accesses. The
 * attribute-controller read from 3C0h is intentional: the original reads the index port
 * after selecting a register, then writes the read-modify-write result through that port.
 */
#include <stdint.h>
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

#ifdef KE_ORACLE
#include "../oracle/oracle.h"
#endif

/* EQU values from m_13944_13a48.asm. */
#define VGA_ATTR_INDEX_PORT 0x3c0
#define VGA_CRTC_INDEX_PORT 0x3d4
#define VGA_SEQ_INDEX_PORT  0x3c4
#define VGA_GC_INDEX_PORT   0x3ce
#define VGA_SEQ_PLANE_MASK_INDEX 0x02
#define VGA_GC_READ_PLANE_INDEX  0x04
#define VGA_GC_MODE_INDEX        0x05
#define VGA_ALL_PLANES_MASK      0x0f
#define VGA_READ_PLANE_MASK      0x03
#define VGA_PLANE_ROTATION_SEED  0x11
#define VGA_PLANE_ROTATION_COMMAND 0x1102

/* VGA_STATE STRUC offsets in the byte-packed DisplayModeInfo. */
#define VGA_STATE_GC_MODE       0x60
#define VGA_STATE_SEQ_PLANE_MASK 0x61
#define VGA_STATE_GC_READ_PLANE 0x62

extern unsigned char vga_state[];
int inp(int port);
int outp(int port, int value);
unsigned outpw(int port, int value);

/* Test-only instrumentation: the original oracle records OUT DX,AX as one 16-bit event.
 * Port I/O still goes through the same virtual hardware in both executables. */
static int in8(int port)
{
    int value = inp(port) & 0xff;
#ifdef KE_ORACLE
    oracle_trace_add('I', (uint16_t)port, (uint32_t)value, 1);
#endif
    return value;
}

static void out8(int port, int value)
{
    value &= 0xff;
    outp(port, value);
#ifdef KE_ORACLE
    oracle_trace_add('O', (uint16_t)port, (uint32_t)value, 1);
#endif
}

static void out16(int port, int value)
{
    value &= 0xffff;
    outpw(port, value);
#ifdef KE_ORACLE
    oracle_trace_add('O', (uint16_t)port, (uint32_t)value, 2);
#endif
}

static int update_register(int index_port, uint8_t index, uint8_t retain, uint8_t set,
                           int attribute)
{
    int data_port = index_port;
    int value;
    out8(index_port, attribute ? (index | 0x20) : index);
    if (!attribute)
        data_port++;
    value = in8(data_port);
    value = (value & retain) | set;
    out8(data_port, value);
    return 0; /* The cdecl callers declare int; the historical EAX value is unspecified. */
}

int update_attr_register(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_register(VGA_ATTR_INDEX_PORT, index, retain, set, 1);
}
int update_attr_register_entry(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_attr_register(index, retain, set);
}

int update_crtc_register(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_register(VGA_CRTC_INDEX_PORT, index, retain, set, 0);
}
int update_crtc_register_entry(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_crtc_register(index, retain, set);
}

int update_seq_register(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_register(VGA_SEQ_INDEX_PORT, index, retain, set, 0);
}
int update_seq_register_entry(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_seq_register(index, retain, set);
}

int update_gc_register(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_register(VGA_GC_INDEX_PORT, index, retain, set, 0);
}
int update_gc_register_entry(uint8_t index, uint8_t retain, uint8_t set)
{
    return update_gc_register(index, retain, set);
}

void set_seq_plane_mask(uint32_t mask)
{
    uint8_t value = (uint8_t)mask & VGA_ALL_PLANES_MASK;
    vga_state[VGA_STATE_SEQ_PLANE_MASK] = value;
    out16(VGA_SEQ_INDEX_PORT, VGA_SEQ_PLANE_MASK_INDEX | ((int)value << 8));
}
void set_seq_plane_mask_entry(uint32_t mask) { set_seq_plane_mask(mask); }

static uint8_t rol8(uint8_t value, uint8_t count)
{
    unsigned n = (count & 0x1f) & 7;
    if (n == 0)
        return value;
    return (uint8_t)((value << n) | (value >> (8 - n)));
}

void rotate_seq_plane_mask(uint32_t count)
{
    uint8_t value = rol8(VGA_PLANE_ROTATION_SEED, (uint8_t)count) & VGA_ALL_PLANES_MASK;
    vga_state[VGA_STATE_SEQ_PLANE_MASK] = value;
    out16(VGA_SEQ_INDEX_PORT, VGA_SEQ_PLANE_MASK_INDEX | ((int)value << 8));
}
void rotate_seq_plane_mask_entry(uint32_t count) { rotate_seq_plane_mask(count); }

void set_gc_read_map(uint32_t plane)
{
    uint8_t value = (uint8_t)plane & VGA_READ_PLANE_MASK;
    vga_state[VGA_STATE_GC_READ_PLANE] = value;
    out16(VGA_GC_INDEX_PORT, VGA_GC_READ_PLANE_INDEX | ((int)value << 8));
}
void set_gc_read_map_entry(uint32_t plane) { set_gc_read_map(plane); }

void set_gc_mode(uint32_t mode)
{
    uint8_t value = (uint8_t)mode;
    vga_state[VGA_STATE_GC_MODE] = value;
    out16(VGA_GC_INDEX_PORT, VGA_GC_MODE_INDEX | ((int)value << 8));
}
void set_gc_mode_entry(uint32_t mode) { set_gc_mode(mode); }
