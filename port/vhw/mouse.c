/* mouse.c - INT 33h mouse driver (Microsoft-compatible subset the game calls).
 *
 * Host relative motion is converted to mickeys (1 host pixel = 1 mickey at the default
 * scale) and to driver coordinates with the driver's sensitivity (mickeys per 8 pixels,
 * default 8/16 as MS MOUSE.COM), clamped to the ranges set by functions 7/8.
 * Functions: 00 reset, 03 position+buttons, 04 set position, 07/08 ranges, 0B motion
 * counters, 0F mickey ratio, 1A/1B sensitivity, 24 version. Others are logged once.
 *
 * INT 33h function 1Ah's double-speed threshold is measured in mickeys per second. The host
 * side supplies relative motion while its window is captured (port/host/input.c).
 */
#include <stdio.h>
#include "../platform/ke_platform.h"
#include "vhw.h"
#include "../include/ke_port.h"

static KeMutex mouse_lock;
static double pos_x, pos_y;
static int min_x, max_x = 639, min_y, max_y = 199;
static int buttons;
static int mickey_x, mickey_y;           /* motion counters for function 0Bh */
static double mickey_fraction_x, mickey_fraction_y;
static int ratio_x = 8, ratio_y = 16;    /* mickeys per 8 pixels */
static int sens_x = 50, sens_y = 50, sens_threshold = 50;
static uint64_t last_motion_timestamp_ns;
static int motion_clock_valid;

static int mouse_accelerate(float dx, float dy, uint64_t timestamp_ns)
{
    double elapsed, threshold;
    int speed_threshold = sens_threshold ? sens_threshold : 64;

    if (!motion_clock_valid || timestamp_ns <= last_motion_timestamp_ns) {
        last_motion_timestamp_ns = timestamp_ns;
        motion_clock_valid = 1;
        return 0;
    }
    elapsed = (double)(timestamp_ns - last_motion_timestamp_ns) / 1000000000.0;
    last_motion_timestamp_ns = timestamp_ns;
    if (elapsed <= 0.0)
        return 0;

    threshold = (double)speed_threshold * elapsed;
    return (double)dx * dx + (double)dy * dy > threshold * threshold;
}

static void clamp(void)
{
    if (pos_x < min_x) pos_x = min_x;
    if (pos_x > max_x) pos_x = max_x;
    if (pos_y < min_y) pos_y = min_y;
    if (pos_y > max_y) pos_y = max_y;
}

void vmouse_motion(float dx, float dy)
{
    vmouse_motion_at(dx, dy, ke_monotonic_raw_ns());
}

void vmouse_motion_at(float dx, float dy, uint64_t timestamp_ns)
{
    int accelerated;
    double whole_x, whole_y;

    ke_mutex_lock(&mouse_lock);
    accelerated = mouse_accelerate(dx, dy, timestamp_ns);
    whole_x = dx + mickey_fraction_x;
    whole_y = dy + mickey_fraction_y;
    mickey_x += (int)whole_x;
    mickey_y += (int)whole_y;
    mickey_fraction_x = whole_x - (int)whole_x;
    mickey_fraction_y = whole_y - (int)whole_y;
    if (accelerated) {
        dx *= 2.0f;
        dy *= 2.0f;
    }
    pos_x += dx * 8.0 / ratio_x * (sens_x / 50.0);
    pos_y += dy * 8.0 / ratio_y * (sens_y / 50.0);
    clamp();
    ke_mutex_unlock(&mouse_lock);
}

void vmouse_set_absolute_position(int x, int y)
{
    ke_mutex_lock(&mouse_lock);
    pos_x = x;
    pos_y = y;
    clamp();
    ke_mutex_unlock(&mouse_lock);
}

void vmouse_buttons(int mask)
{
    ke_mutex_lock(&mouse_lock);
    buttons = mask;
    ke_mutex_unlock(&mouse_lock);
}

void vmouse_int33(union REGS *r, struct SREGS *s)
{
    char key[32];
    (void)s;
    ke_mutex_lock(&mouse_lock);
    switch (r->w.ax) {
    case 0x00:
        r->w.ax = 0xffff;
        r->w.bx = 2;
        min_x = 0; max_x = 639; min_y = 0; max_y = 199;
        pos_x = 320; pos_y = 100;
        ratio_x = 8; ratio_y = 16;
        buttons = 0;
        mickey_x = mickey_y = 0;
        mickey_fraction_x = mickey_fraction_y = 0.0;
        sens_x = sens_y = sens_threshold = 50;
        motion_clock_valid = 0;
        break;
    case 0x03:
        r->w.bx = (unsigned short)buttons;
        r->w.cx = (unsigned short)(int)pos_x;
        r->w.dx = (unsigned short)(int)pos_y;
        break;
    case 0x04:
        pos_x = (short)r->w.cx;
        pos_y = (short)r->w.dx;
        clamp();
        break;
    case 0x07:
        min_x = (short)r->w.cx; max_x = (short)r->w.dx;
        if (min_x > max_x) { int t = min_x; min_x = max_x; max_x = t; }
        clamp();
        break;
    case 0x08:
        min_y = (short)r->w.cx; max_y = (short)r->w.dx;
        if (min_y > max_y) { int t = min_y; min_y = max_y; max_y = t; }
        clamp();
        break;
    case 0x0b:
        r->w.cx = (unsigned short)mickey_x;
        r->w.dx = (unsigned short)mickey_y;
        mickey_x = mickey_y = 0;
        break;
    case 0x0f:
        ratio_x = r->w.cx ? r->w.cx : 8;
        ratio_y = r->w.dx ? r->w.dx : 16;
        break;
    case 0x1a:
        sens_x = r->w.bx < 1 ? 1 : (r->w.bx > 100 ? 100 : r->w.bx);
        sens_y = r->w.cx < 1 ? 1 : (r->w.cx > 100 ? 100 : r->w.cx);
        sens_threshold = r->w.dx > 100 ? 100 : r->w.dx;
        motion_clock_valid = 0;
        break;
    case 0x1b:
        r->w.bx = (unsigned short)sens_x;
        r->w.cx = (unsigned short)sens_y;
        r->w.dx = (unsigned short)sens_threshold;
        break;
    case 0x24:
        r->w.bx = 0x0626;   /* driver 6.26 */
        r->h.ch = 4;        /* PS/2 */
        r->h.cl = 0;
        break;
    default:
        snprintf(key, sizeof key, "int33.%04x", r->w.ax);
        ke_log_once(key, KE_LOG_WARN, "mouse", "INT 33h AX=%04Xh not implemented", r->w.ax);
        break;
    }
    ke_mutex_unlock(&mouse_lock);
}

void vmouse_init(void)
{
    ke_mutex_init(&mouse_lock);
}
