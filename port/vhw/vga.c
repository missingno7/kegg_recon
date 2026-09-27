/* vga.c - VGA adapter: 4 x 64 KiB planes, sequencer, graphics controller, CRTC, attribute
 * controller, DAC, input status (retrace) and scan-out to 8-bit indexed pixels.
 *
 * Memory model (as the hardware): a CPU byte address inside the A0000h window maps to plane
 * offset (addr & 0xFFFF) in unchained modes, where the sequencer map mask selects the planes
 * written and the GC read map selects the plane read; with chain-4 enabled the low two
 * address bits select the plane and the plane offset is (addr & 0xFFFC). Write modes 0-3,
 * read modes 0-1, latches, set/reset, data rotate / logical op and bit mask are implemented.
 *
 * Timing: retrace is derived from the host clock using the programmed CRTC vertical total
 * and a 31.469 kHz line rate (70 Hz for 400-line timing, 60 Hz for 480-line timing).
 * Polling loops on 3DAh therefore see the historical cadence; a poll far from the next
 * retrace yields the CPU (see vga_status_poll).
 *
 * WORK PACKAGE "vga": verify against the historical renderers (port/oracle), latch start
 * address at retrace, panning/line compare, text mode (currently not rendered).
 */
#include <string.h>
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

static uint8_t planes[4][0x10000];
static uint8_t latch[4];
static uint8_t seq_index, seq[8];
static uint8_t gc_index, gc[16];
static uint8_t crtc_index, crtc[32];
static uint8_t attr_index, attr[32], attr_flipflop;
static uint8_t misc_output = 0x63;
static uint8_t dac[256][3];
static uint8_t dac_write_index, dac_read_index, dac_component, dac_read_component;
static uint8_t dac_pel_mask = 0xff;
static int bios_mode = 3;
static uint64_t timing_origin;
static CRITICAL_SECTION dac_lock;

/* ---- register helpers ---------------------------------------------------------------- */
static int chain4(void) { return (seq[4] & 0x08) != 0; }

static int vertical_total_lines(void)
{
    int vt = crtc[6] | ((crtc[7] & 0x01) << 8) | ((crtc[7] & 0x20) << 4);
    return vt ? vt + 2 : 449;
}
static int vretrace_start_line(void)
{
    return crtc[0x10] | ((crtc[7] & 0x04) << 6) | ((crtc[7] & 0x80) << 2);
}
static int vdisplay_end_line(void)
{
    return crtc[0x12] | ((crtc[7] & 0x02) << 7) | ((crtc[7] & 0x40) << 3);
}

#define LINE_NS 31778ull   /* 1 / 31.469 kHz */

/* Returns the current scan line within the frame and the frame number. */
static int current_line(uint64_t *frame)
{
    uint64_t t = ke_now_ns() - timing_origin;
    uint64_t lines = t / LINE_NS;
    int total = vertical_total_lines();
    if (frame)
        *frame = lines / (uint64_t)total;
    return (int)(lines % (uint64_t)total);
}

uint32_t vga_frame_counter(void)
{
    uint64_t f;
    current_line(&f);
    return (uint32_t)f;
}

/* Input status 1 (3DAh): bit 3 vertical retrace, bit 0 display disabled (blanking). */
static uint8_t vga_status_poll(void)
{
    int line = current_line(NULL);
    int vrs = vretrace_start_line(), vde = vdisplay_end_line();
    int vre = vrs + 2;                     /* 2-line retrace pulse as programmed by BIOS */
    uint8_t v = 0;
    if (vrs == 0)
        vrs = 412, vre = 414, vde = 399;
    if (line >= vrs && line < vre)
        v |= 0x08;
    if (line > vde)
        v |= 0x01;
    if (!(v & 0x08)) {
        /* Busy polling far from the retrace edge: give the host CPU back. */
        int total = vertical_total_lines();
        int distance = (vrs - line + total) % total;
        if (distance > 64 && !vhw_on_irq_thread())
            ke_sleep_ns((uint64_t)(distance - 48) * LINE_NS / 4);
    }
    return v;
}

/* ---- memory -------------------------------------------------------------------------- */
static uint8_t rotate(uint8_t v, int n) { n &= 7; return (uint8_t)((v >> n) | (v << (8 - n))); }

uint8_t vga_mem_read8(uint32_t linear)
{
    uint32_t off = linear - 0xA0000u;
    int plane, p;
    if (linear < 0xA0000u || linear > 0xBFFFFu)
        return 0xff;
    if (chain4()) {
        plane = off & 3;
        off &= 0xfffc;
    } else {
        plane = gc[4] & 3;
        off &= 0xffff;
    }
    for (p = 0; p < 4; p++)
        latch[p] = planes[p][off];
    if (gc[5] & 0x08) {                    /* read mode 1: color compare */
        uint8_t result = 0;
        int bit;
        for (bit = 0; bit < 8; bit++) {
            int match = 1;
            for (p = 0; p < 4; p++) {
                if (!(gc[7] & (1 << p)))
                    continue;
                if (((latch[p] >> bit) & 1) != ((gc[2] >> p) & 1))
                    match = 0;
            }
            result |= (uint8_t)(match << bit);
        }
        return result;
    }
    return latch[plane];
}

void vga_mem_write8(uint32_t linear, uint8_t value)
{
    uint32_t off = linear - 0xA0000u;
    uint8_t mask = seq[2] & 0x0f, bitmask = gc[8];
    int mode = gc[5] & 3, op = (gc[3] >> 3) & 3, p;
    if (linear < 0xA0000u || linear > 0xBFFFFu)
        return;
    if (chain4()) {
        mask &= (uint8_t)(1u << (off & 3));
        off &= 0xfffc;
    } else {
        off &= 0xffff;
    }
    for (p = 0; p < 4; p++) {
        uint8_t data;
        if (!(mask & (1 << p)))
            continue;
        switch (mode) {
        case 0:
            data = rotate(value, gc[3] & 7);
            if (gc[1] & (1 << p))
                data = (gc[0] & (1 << p)) ? 0xff : 0x00;
            break;
        case 1:
            planes[p][off] = latch[p];
            continue;
        case 2:
            data = (value & (1 << p)) ? 0xff : 0x00;
            break;
        default: /* 3 */
            data = (gc[0] & (1 << p)) ? 0xff : 0x00;
            bitmask &= rotate(value, gc[3] & 7);
            break;
        }
        switch (op) {
        case 1: data &= latch[p]; break;
        case 2: data |= latch[p]; break;
        case 3: data ^= latch[p]; break;
        default: break;
        }
        planes[p][off] = (uint8_t)((data & bitmask) | (latch[p] & (uint8_t)~bitmask));
    }
}

/* ---- ports --------------------------------------------------------------------------- */
static uint32_t vga_in(void *ctx, uint16_t port, int size)
{
    uint8_t v = 0xff;
    (void)ctx; (void)size;
    switch (port) {
    case 0x3c0: v = attr_index; break;
    case 0x3c1: v = attr[attr_index & 0x1f]; break;
    case 0x3c2: v = 0x10; break;                          /* input status 0 */
    case 0x3c4: v = seq_index; break;
    case 0x3c5: v = seq[seq_index & 7]; break;
    case 0x3c6: v = dac_pel_mask; break;
    case 0x3c7: v = 0x03; break;                          /* DAC state: read mode */
    case 0x3c8: v = dac_write_index; break;
    case 0x3c9:
        EnterCriticalSection(&dac_lock);
        v = dac[dac_read_index][dac_read_component];
        if (++dac_read_component == 3) {
            dac_read_component = 0;
            dac_read_index++;
        }
        LeaveCriticalSection(&dac_lock);
        break;
    case 0x3cc: v = misc_output; break;
    case 0x3ce: v = gc_index; break;
    case 0x3cf: v = gc[gc_index & 15]; break;
    case 0x3d4: case 0x3b4: v = crtc_index; break;
    case 0x3d5: case 0x3b5: v = crtc[crtc_index & 31]; break;
    case 0x3da: case 0x3ba:
        attr_flipflop = 0;
        v = vga_status_poll();
        break;
    default: break;
    }
    return v;
}

static void vga_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    uint8_t v = (uint8_t)value;
    (void)ctx; (void)size;
    switch (port) {
    case 0x3c0:
        if (!attr_flipflop)
            attr_index = v;
        else
            attr[attr_index & 0x1f] = v;
        attr_flipflop ^= 1;
        break;
    case 0x3c2: misc_output = v; break;
    case 0x3c4: seq_index = v; break;
    case 0x3c5: seq[seq_index & 7] = v; break;
    case 0x3c6: dac_pel_mask = v; break;
    case 0x3c7: dac_read_index = v; dac_read_component = 0; break;
    case 0x3c8: dac_write_index = v; dac_component = 0; break;
    case 0x3c9:
        EnterCriticalSection(&dac_lock);
        dac[dac_write_index][dac_component] = v & 0x3f;
        if (++dac_component == 3) {
            dac_component = 0;
            dac_write_index++;
        }
        LeaveCriticalSection(&dac_lock);
        break;
    case 0x3ce: gc_index = v; break;
    case 0x3cf: gc[gc_index & 15] = v; break;
    case 0x3d4: case 0x3b4: crtc_index = v; break;
    case 0x3d5: case 0x3b5:
        if ((crtc_index & 31) < 8 && (crtc[0x11] & 0x80) && crtc_index != 7)
            break;                                        /* write protected */
        if (crtc_index == 7 && (crtc[0x11] & 0x80)) {     /* only line compare bit 4 */
            crtc[7] = (uint8_t)((crtc[7] & ~0x10) | (v & 0x10));
            break;
        }
        crtc[crtc_index & 31] = v;
        break;
    case 0x3da: case 0x3ba: break;                        /* feature control */
    default: break;
    }
}

/* ---- BIOS mode set (INT 10h AH=00h) --------------------------------------------------- */
static const uint8_t crtc_mode13[25] = {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f, 0x00,
                                        0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e,
                                        0x8f, 0x28, 0x40, 0x96, 0xb9, 0xa3, 0xff};
static const uint8_t crtc_mode03[25] = {0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f, 0x00,
                                        0x4f, 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e,
                                        0x8f, 0x28, 0x1f, 0x96, 0xb9, 0xa3, 0xff};

void vga_bios_set_mode(int mode)
{
    int i;
    int clear = !(mode & 0x80);
    mode &= 0x7f;
    memset(gc, 0, sizeof gc);
    memset(attr, 0, sizeof attr);
    for (i = 0; i < 16; i++)
        attr[i] = (uint8_t)i;
    gc[8] = 0xff;
    gc[7] = 0x0f;
    seq[0] = 0x03;
    seq[2] = 0x0f;
    if (mode == 0x13) {
        seq[1] = 0x01; seq[3] = 0x00; seq[4] = 0x0e;
        gc[5] = 0x40; gc[6] = 0x05;
        attr[0x10] = 0x41; attr[0x11] = 0; attr[0x12] = 0x0f; attr[0x13] = 0; attr[0x14] = 0;
        memcpy(crtc, crtc_mode13, sizeof crtc_mode13);
        misc_output = 0x63;
    } else {
        seq[1] = 0x00; seq[3] = 0x00; seq[4] = 0x02;
        gc[5] = 0x10; gc[6] = 0x0e;
        attr[0x10] = 0x0c; attr[0x12] = 0x0f; attr[0x13] = 0x08;
        memcpy(crtc, crtc_mode03, sizeof crtc_mode03);
        misc_output = 0x67;
        mode = 3;
    }
    if (clear)
        memset(planes, 0, sizeof planes);
    bios_mode = mode;
    ke_lowmem_shadow[0x449] = (uint8_t)mode;
    ke_log(KE_LOG_INFO, "vga", "BIOS mode set %02Xh", mode);
}

int vga_bios_mode(void) { return bios_mode; }

void vga_palette_rgb888(uint8_t rgb[256][3])
{
    int i, c;
    EnterCriticalSection(&dac_lock);
    for (i = 0; i < 256; i++)
        for (c = 0; c < 3; c++)
            rgb[i][c] = (uint8_t)((dac[i][c] << 2) | (dac[i][c] >> 4));
    LeaveCriticalSection(&dac_lock);
}

/* ---- scan-out ------------------------------------------------------------------------ */
int vga_scanout(uint8_t *dst, int dst_pitch, int max_w, int max_h, int *out_w, int *out_h)
{
    int hde = crtc[1] + 1;                         /* character clocks */
    int vde = vdisplay_end_line() + 1;
    int max_scan = (crtc[9] & 0x1f) + 1;
    int double_scan = (crtc[9] & 0x80) ? 2 : 1;
    int width, height, y, x;
    uint32_t start = ((uint32_t)crtc[0x0c] << 8) | crtc[0x0d];
    uint32_t row_bytes;
    int dword_mode = (crtc[0x14] & 0x40) != 0;
    int byte_mode = (crtc[0x17] & 0x40) != 0;
    if (!(gc[6] & 0x01))                           /* text mode: not rendered (yet) */
        return 0;
    width = hde * 4;                               /* 256-color: 4 pixels per char clock */
    height = vde / (max_scan * double_scan);
    if (width > max_w) width = max_w;
    if (height > max_h) height = max_h;
    if (width <= 0 || height <= 0)
        return 0;
    if (dword_mode)
        row_bytes = (uint32_t)crtc[0x13] << 3;
    else if (byte_mode)
        row_bytes = (uint32_t)crtc[0x13] << 1;
    else
        row_bytes = (uint32_t)crtc[0x13] << 2;
    for (y = 0; y < height; y++) {
        uint8_t *row = dst + (size_t)y * dst_pitch;
        if (dword_mode) {
            uint32_t base = (start << 2) + (uint32_t)y * row_bytes;
            for (x = 0; x < width; x++) {
                uint32_t a = base + (uint32_t)x;
                row[x] = planes[a & 3][a & 0xfffc];
            }
        } else {
            uint32_t base = start + (uint32_t)y * (row_bytes >> (byte_mode ? 0 : 1));
            for (x = 0; x < width; x++)
                row[x] = planes[x & 3][(base + (uint32_t)(x >> 2)) & 0xffff];
        }
        for (x = 0; x < width; x++)
            row[x] &= dac_pel_mask;
    }
    *out_w = width;
    *out_h = height;
    return 1;
}

void vga_init(void)
{
    InitializeCriticalSection(&dac_lock);
    timing_origin = ke_now_ns();
    vga_bios_set_mode(3);
    vhw_register_ports(0x3b4, 0x3b5, vga_in, vga_out, NULL, "vga-crtc-mono");
    vhw_register_ports(0x3ba, 0x3ba, vga_in, vga_out, NULL, "vga-status-mono");
    vhw_register_ports(0x3c0, 0x3cf, vga_in, vga_out, NULL, "vga");
    vhw_register_ports(0x3d4, 0x3d5, vga_in, vga_out, NULL, "vga-crtc");
    vhw_register_ports(0x3da, 0x3da, vga_in, vga_out, NULL, "vga-status");
}
