/* mouse.c - INT 33h mouse driver (Microsoft-compatible subset the game calls).
 *
 * Host relative motion is converted to mickeys (1 host pixel = 1 mickey at the default
 * scale) and to driver coordinates with the driver's sensitivity (mickeys per 8 pixels,
 * default 8/16 as MS MOUSE.COM), clamped to the ranges set by functions 7/8.
 * Functions: 00 reset, 03 position+buttons, 04 set position, 07/08 ranges, 0B motion
 * counters, 0F mickey ratio, 1A/1B sensitivity, 24 version. Others are logged once.
 *
 * WORK PACKAGE "mouse-joystick": match MS driver acceleration (threshold), relative-mode
 * policy, capture/grab UX.
 */
#include <stdio.h>
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

static CRITICAL_SECTION mouse_lock;
static double pos_x, pos_y;
static int min_x, max_x = 639, min_y, max_y = 199;
static int buttons;
static int mickey_x, mickey_y;           /* motion counters for function 0Bh */
static int ratio_x = 8, ratio_y = 16;    /* mickeys per 8 pixels */
static int sens_x = 50, sens_y = 50, sens_threshold = 50;

static void clamp(void)
{
    if (pos_x < min_x) pos_x = min_x;
    if (pos_x > max_x) pos_x = max_x;
    if (pos_y < min_y) pos_y = min_y;
    if (pos_y > max_y) pos_y = max_y;
}

void vmouse_motion(float dx, float dy)
{
    EnterCriticalSection(&mouse_lock);
    mickey_x += (int)dx;
    mickey_y += (int)dy;
    pos_x += dx * 8.0 / ratio_x * (sens_x / 50.0);
    pos_y += dy * 8.0 / ratio_y * (sens_y / 50.0);
    clamp();
    LeaveCriticalSection(&mouse_lock);
}

void vmouse_buttons(int mask)
{
    EnterCriticalSection(&mouse_lock);
    buttons = mask;
    LeaveCriticalSection(&mouse_lock);
}

void vmouse_int33(union REGS *r, struct SREGS *s)
{
    char key[32];
    (void)s;
    EnterCriticalSection(&mouse_lock);
    switch (r->w.ax) {
    case 0x00:
        r->w.ax = 0xffff;
        r->w.bx = 2;
        min_x = 0; max_x = 639; min_y = 0; max_y = 199;
        pos_x = 320; pos_y = 100;
        ratio_x = 8; ratio_y = 16;
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
        sens_x = r->w.bx; sens_y = r->w.cx; sens_threshold = r->w.dx;
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
    LeaveCriticalSection(&mouse_lock);
}

void vmouse_init(void) { InitializeCriticalSection(&mouse_lock); }
