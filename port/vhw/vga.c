/* vga.c - VGA adapter: 4 x 64 KiB planes, sequencer, graphics controller, CRTC, attribute
 * controller, DAC, input status (retrace) and scan-out to 8-bit indexed pixels.
 *
 * Memory model (as the hardware): a CPU byte address inside the A0000h window maps to plane
 * offset (addr & 0xFFFF) in unchained modes, where the sequencer map mask selects the planes
 * written and the GC read map selects the plane read; with chain-4 enabled the low two
 * address bits select the plane and the plane offset is (addr & 0xFFFC). Write modes 0-3,
 * read modes 0-1, latches, set/reset, data rotate / logical op and bit mask are implemented.
 *
 * Timing: retrace is derived from the same monotonic nanosecond clock used by the PIT. The
 * horizontal line period comes from the selected VGA dot clock and CRTC horizontal total;
 * the programmed vertical total then gives the 70.086 Hz and 59.94 Hz game timings.
 * Polling loops on 3DAh therefore see the historical cadence; a poll far from the next
 * retrace yields the CPU (see vga_status_poll).
 *
 * The V1 virtual-VGA package covers register semantics, retrace-latched start addresses,
 * panning/split scan-out and the game's packed/planar graphics modes. Text mode is not drawn.
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
static uint64_t timing_frame_base;
static uint64_t timing_last_retrace;
static uint16_t display_start_latched;
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
static int effective_vretrace_start_line(void)
{
    int line = vretrace_start_line();
    return line ? line : 412;             /* BIOS timing fallback when start is unset */
}
static int vdisplay_end_line(void)
{
    return crtc[0x12] | ((crtc[7] & 0x02) << 7) | ((crtc[7] & 0x40) << 3);
}

static uint32_t dot_clock_hz(void)
{
    /* The game uses the standard 25.175 MHz (clock 0) and 28.322 MHz (clock 1). */
    return ((misc_output >> 2) & 3) == 0 ? 25175000u : 28322000u;
}

static uint32_t horizontal_total_dots(void)
{
    uint32_t characters = (uint32_t)crtc[0] + 5u;
    uint32_t dots_per_character = (seq[1] & 1) ? 8u : 9u;
    return characters ? characters * dots_per_character : 800u;
}

static uint64_t elapsed_dot_clocks(uint64_t elapsed_ns)
{
    uint64_t seconds = elapsed_ns / 1000000000ull;
    uint64_t remainder_ns = elapsed_ns % 1000000000ull;
    uint32_t clock_hz = dot_clock_hz();
    return seconds * clock_hz + (remainder_ns * clock_hz) / 1000000000ull;
}

/* Returns the current scan line and the number of vertical-retrace edges seen. */
static int current_line(uint64_t *retrace)
{
    uint64_t t = vhw_clock_now_ns() - timing_origin;
    uint64_t lines = elapsed_dot_clocks(t) / horizontal_total_dots();
    int total = vertical_total_lines();
    int line = (int)(lines % (uint64_t)total);
    if (retrace) {
        int vrs = effective_vretrace_start_line();
        *retrace = lines / (uint64_t)total + (line >= vrs ? 1u : 0u);
    }
    return line;
}

static uint16_t programmed_display_start(void)
{
    return (uint16_t)(((uint16_t)crtc[0x0c] << 8) | crtc[0x0d]);
}

/* The CRTC start address is copied into its display latch at vertical retrace. */
static void update_retrace_latch(void)
{
    uint64_t retrace;
    current_line(&retrace);
    if (retrace > timing_last_retrace) {
        display_start_latched = programmed_display_start();
        timing_last_retrace = retrace;
    }
}

/* Preserve the public frame count when a mode/timing set restarts the scan clock. */
static void restart_timing(void)
{
    update_retrace_latch();
    timing_frame_base += timing_last_retrace;
    timing_origin = vhw_clock_now_ns();
    timing_last_retrace = 0;
}

uint32_t vga_frame_counter(void)
{
    uint64_t retrace;
    update_retrace_latch();
    current_line(&retrace);
    return (uint32_t)(timing_frame_base + retrace);
}

/* Input status 1 (3DAh): bit 3 vertical retrace, bit 0 display disabled (blanking). */
static uint8_t vga_status_poll(void)
{
    int line = current_line(NULL);
    int vrs = effective_vretrace_start_line(), vde = vdisplay_end_line();
    int width = ((crtc[0x11] & 0x0f) - (crtc[0x10] & 0x0f)) & 0x0f;
    int vre;
    uint8_t v = 0;
    update_retrace_latch();
    if (!width)
        width = 16;
    vre = vrs + width;
    if (vretrace_start_line() == 0)
        vre = vrs + 2, vde = 399;
    if ((vre <= vertical_total_lines() && line >= vrs && line < vre) ||
        (vre > vertical_total_lines() && (line >= vrs || line < vre % vertical_total_lines())))
        v |= 0x08;
    if (line > vde)
        v |= 0x01;
    if (!(v & 0x08)) {
        /* Busy polling far from the retrace edge: give the host CPU back. */
        int total = vertical_total_lines();
        int distance = (vrs - line + total) % total;
        if (distance > 64 && !vhw_on_irq_thread()) {
            uint64_t wait_dots = (uint64_t)(distance - 48) * horizontal_total_dots();
            /* Avoid host sleep in IRQ0: the handler must poll closely enough not to miss the
             * retrace edge while servicing its scheduled PIT interrupt. */
            ke_sleep_ns(wait_dots * 1000000000ull / dot_clock_hz() / 4);
        }
    }
    return v;
}

/* ---- memory -------------------------------------------------------------------------- */
static uint8_t rotate(uint8_t v, int n) { n &= 7; return (uint8_t)((v >> n) | (v << (8 - n))); }

uint8_t vga_mem_read8(uint32_t linear)
{
    uint32_t off, address = linear - 0xA0000u;
    int map = (gc[6] >> 2) & 3;
    int plane, p;
    if (linear < 0xA0000u || linear > 0xBFFFFu)
        return 0xff;
    if (map == 1) {
        if (address >= 0x10000u) return 0xff;
    } else if (map == 2) {
        if (address < 0x10000u || address >= 0x18000u) return 0xff;
        address -= 0x10000u;
    } else if (map == 3) {
        if (address < 0x18000u) return 0xff;
        address -= 0x18000u;
    }
    address &= 0xffffu;                    /* four 64 KiB planes back the VGA aperture */
    off = address;
    if (chain4()) {
        plane = off & 3;
        off &= 0xfffc;
        off &= 0xffff;
    } else if (gc[5] & 0x10) {             /* host odd/even mode */
        plane = (gc[4] & 2) | (off & 1);
        if (gc[6] & 2)
            off &= ~1u;
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
    uint32_t off, address = linear - 0xA0000u;
    uint8_t mask = seq[2] & 0x0f, bitmask = gc[8];
    int mode = gc[5] & 3, op = (gc[3] >> 3) & 3, p;
    if (linear < 0xA0000u || linear > 0xBFFFFu)
        return;
    if (((gc[6] >> 2) & 3) == 1) {
        if (address >= 0x10000u) return;
    } else if (((gc[6] >> 2) & 3) == 2) {
        if (address < 0x10000u || address >= 0x18000u) return;
        address -= 0x10000u;
    } else if (((gc[6] >> 2) & 3) == 3) {
        if (address < 0x18000u) return;
        address -= 0x18000u;
    }
    address &= 0xffffu;
    off = address;
    if (chain4()) {
        mask &= (uint8_t)(1u << (off & 3));
        off &= 0xfffc;
        off &= 0xffff;
    } else if (gc[5] & 0x10) {             /* host odd/even mode */
        mask &= (uint8_t)(1u << ((off & 1) | (gc[4] & 2)));
        if (gc[6] & 2)
            off &= ~1u;
    } else {
        off &= 0xffff;
    }
    if (mode == 0 && !(gc[1] & 0x0f) && !(gc[3] & 0x1f) && bitmask == 0xff) {
        for (p = 0; p < 4; p++)
            if (mask & (1 << p))
                planes[p][off] = value;
        return;
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
            bitmask = (uint8_t)(gc[8] & rotate(value, gc[3] & 7));
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
    (void)ctx;
    if (size == 2) {
        vga_out(ctx, port, value & 0xff, 1);
        if (port == 0x3c4 || port == 0x3ce || port == 0x3d4 || port == 0x3b4)
            vga_out(ctx, (uint16_t)(port + 1), (value >> 8) & 0xff, 1);
        else
            vga_out(ctx, port, (value >> 8) & 0xff, 1);
        return;
    }
    update_retrace_latch();
    switch (port) {
    case 0x3c0:
        if (!attr_flipflop)
            attr_index = v;
        else
            attr[attr_index & 0x1f] = v;
        attr_flipflop ^= 1;
        break;
    case 0x3c2:
        misc_output = v;
        restart_timing();
        break;
    case 0x3c4: seq_index = v; break;
    case 0x3c5:
        seq[seq_index & 7] = v;
        if (seq_index == 1)
            restart_timing();
        break;
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
        if (crtc_index == 0 || crtc_index == 6 || crtc_index == 7 ||
            crtc_index == 0x10 || crtc_index == 0x11)
            restart_timing();
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
    restart_timing();
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
    seq_index = gc_index = crtc_index = attr_index = attr_flipflop = 0;
    bios_mode = mode;
    display_start_latched = programmed_display_start();
    timing_origin = vhw_clock_now_ns();
    timing_last_retrace = 0;
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
    int row_repeat = max_scan * double_scan;
    int width, height, y, x;
    uint32_t start;
    uint32_t plane_stride;
    uint32_t line_compare;
    int dword_mode = (crtc[0x14] & 0x40) != 0;
    int byte_mode = (crtc[0x17] & 0x40) != 0;
    int address_shift = dword_mode ? 2 : (byte_mode ? 0 : 1);
    int byte_pan = (crtc[8] >> 5) & 3;
    int preset_row_scan = crtc[8] & 0x1f;
    int panning_mode = (attr[0x10] & 0x20) != 0;
    int fine_pan = (attr[0x13] & 0x0f) >> 1;
    update_retrace_latch();
    start = display_start_latched;
    line_compare = crtc[0x18] | ((uint32_t)(crtc[7] & 0x10) << 4) |
                   ((uint32_t)(crtc[9] & 0x40) << 3);
    if (!(gc[6] & 0x01))                           /* text mode: not rendered (yet) */
        return 0;
    width = hde * 4;                               /* 256-color shift mode: 4 pixels per char */
    height = (vde + row_repeat - 1) / row_repeat;
    if (width > max_w) width = max_w;
    if (height > max_h) height = max_h;
    if (width <= 0 || height <= 0)
        return 0;
    /* The CRTC Offset register is doubled in byte and dword modes, but not word mode.
     * Unchained byte-mode scan-out indexes each plane by this per-plane byte stride. */
    plane_stride = (uint32_t)crtc[0x13] * ((byte_mode || dword_mode) ? 2u : 1u);
    for (y = 0; y < height; y++) {
        uint8_t *row = dst + (size_t)y * dst_pitch;
        uint32_t physical_line = (uint32_t)y * (uint32_t)row_repeat;
        int split = physical_line > line_compare;
        uint32_t memory_line;
        uint32_t base_start = split ? 0 : start;
        int pan = (split && panning_mode) ? 0 : fine_pan;
        int bp = (split && panning_mode) ? 0 : byte_pan;
        if (split)
            memory_line = (physical_line - (line_compare + 1)) / (uint32_t)row_repeat;
        else
            memory_line = (physical_line + (uint32_t)preset_row_scan) / (uint32_t)row_repeat;
        if (chain4()) {
            uint32_t row_bytes = (uint32_t)crtc[0x13] << (address_shift + 1);
            uint32_t base = (base_start << address_shift) + memory_line * row_bytes +
                            ((uint32_t)bp << address_shift);
            for (x = 0; x < width; x++) {
                uint32_t a = base + (uint32_t)(x + pan);
                row[x] = planes[a & 3][a & 0xfffc];
            }
        } else {
            uint32_t base = (byte_mode ? base_start :
                             (base_start << address_shift) / 4) +
                            memory_line * plane_stride + bp;
            for (x = 0; x < width; x++) {
                uint32_t px = (uint32_t)(x + pan);
                row[x] = planes[px & 3][(base + (px >> 2)) & 0xffff];
            }
        }
        for (x = 0; x < width; x++)
            row[x] &= dac_pel_mask;
    }
    *out_w = width;
    *out_h = height;
    return 1;
}

#ifdef KE_ORACLE
/* Snapshot loader used only by the external scan-out oracle. `plane_bytes` is plane-major. */
void vga_oracle_load_snapshot(const uint8_t *plane_bytes, const uint8_t *crtc_bytes,
                              const uint8_t *dac_bytes, uint16_t display_start,
                              uint8_t chained, uint8_t map_mask, uint8_t read_map,
                              uint8_t seq_clocking, uint8_t write_mode,
                              uint8_t bit_mask, uint8_t misc)
{
    memcpy(planes, plane_bytes, sizeof planes);
    memcpy(crtc, crtc_bytes, sizeof crtc);
    memcpy(dac, dac_bytes, sizeof dac);
    memset(seq, 0, sizeof seq);
    memset(gc, 0, sizeof gc);
    memset(attr, 0, sizeof attr);
    seq[1] = seq_clocking;
    seq[2] = map_mask & 0x0f;
    seq[4] = chained ? 0x0e : 0x06;
    gc[4] = read_map & 3;
    gc[5] = write_mode & 3;
    gc[6] = 0x05;                       /* A0000h graphics aperture */
    gc[8] = bit_mask;
    attr[0x10] = 0x01;                  /* graphics mode; no pel-pan reset on split */
    attr[0x13] = 0;
    misc_output = misc;
    dac_pel_mask = 0xff;
    display_start_latched = display_start;
    timing_origin = vhw_clock_now_ns();
    timing_last_retrace = 0;
}
#endif

void vga_init(void)
{
    InitializeCriticalSection(&dac_lock);
    timing_origin = vhw_clock_now_ns();
    timing_frame_base = 0;
    timing_last_retrace = 0;
    display_start_latched = 0;
    vga_bios_set_mode(3);
    vhw_register_ports(0x3b4, 0x3b5, vga_in, vga_out, NULL, "vga-crtc-mono");
    vhw_register_ports(0x3ba, 0x3ba, vga_in, vga_out, NULL, "vga-status-mono");
    vhw_register_ports(0x3c0, 0x3cf, vga_in, vga_out, NULL, "vga");
    vhw_register_ports(0x3d4, 0x3d5, vga_in, vga_out, NULL, "vga-crtc");
    vhw_register_ports(0x3da, 0x3da, vga_in, vga_out, NULL, "vga-status");
}
