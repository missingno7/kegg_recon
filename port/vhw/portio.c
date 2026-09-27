/* portio.c - I/O port dispatch and the Watcom clib inp/outp/inpw/outpw entry points.
 * Unclaimed ports read as 0xFF (open bus) and ignore writes; each is logged once. */
#include <stdio.h>
#include <string.h>
#include "vhw.h"
#include "../include/ke_port.h"

typedef struct PortDevice {
    vhw_in_fn in;
    vhw_out_fn out;
    void *ctx;
    const char *name;
} PortDevice;

static PortDevice devices[32];
static int device_count;
static uint8_t port_map[0x10000];   /* 0 = unclaimed, else device index + 1 */
static uint8_t port_logged[0x10000 / 8]; /* unclaimed ports already reported */
static vhw_in_fn port_override_in;
static vhw_out_fn port_override_out;
static void *port_override_ctx;

static int first_report(uint16_t port)
{
    uint8_t bit = (uint8_t)(1u << (port & 7));
    if (port_logged[port >> 3] & bit)
        return 0;
    port_logged[port >> 3] |= bit;
    return 1;
}

void vhw_register_ports(uint16_t first, uint16_t last, vhw_in_fn in, vhw_out_fn out, void *ctx,
                        const char *device)
{
    uint32_t p;
    if (device_count >= (int)(sizeof devices / sizeof devices[0]))
        return;
    devices[device_count].in = in;
    devices[device_count].out = out;
    devices[device_count].ctx = ctx;
    devices[device_count].name = device;
    device_count++;
    for (p = first; p <= last; p++)
        port_map[p] = (uint8_t)device_count;
}

void vhw_set_port_override(vhw_in_fn in, vhw_out_fn out, void *ctx)
{
    port_override_in = in;
    port_override_out = out;
    port_override_ctx = ctx;
}

uint32_t vhw_port_in(uint16_t port, int size)
{
    int d = port_map[port];
    if (port_override_in)
        return port_override_in(port_override_ctx, port, size);
    if (d && devices[d - 1].in)
        return devices[d - 1].in(devices[d - 1].ctx, port, size);
    if (first_report(port))
        ke_log(KE_LOG_DEBUG, "port", "read from unclaimed port %04Xh", port);
    return size == 1 ? 0xff : 0xffff;
}

void vhw_port_out(uint16_t port, uint32_t value, int size)
{
    int d = port_map[port];
    if (port_override_out) {
        port_override_out(port_override_ctx, port, value, size);
        return;
    }
    if (d && devices[d - 1].out) {
        devices[d - 1].out(devices[d - 1].ctx, port, value, size);
        return;
    }
    if (first_report(port))
        ke_log(KE_LOG_DEBUG, "port", "write %02Xh to unclaimed port %04Xh", value, port);
}

/* ---- Watcom clib (conio.h) ------------------------------------------------------------- */
int inp(int port)
{
    uint32_t v;
    vhw_enter();
    v = vhw_port_in((uint16_t)port, 1) & 0xff;
    vhw_leave();
    return (int)v;
}

int inpw(int port)
{
    uint32_t v;
    vhw_enter();
    v = vhw_port_in((uint16_t)port, 1) & 0xff;
    v |= (vhw_port_in((uint16_t)(port + 1), 1) & 0xff) << 8;
    vhw_leave();
    return (int)v;
}

int outp(int port, int value)
{
    vhw_enter();
    vhw_port_out((uint16_t)port, (uint32_t)value & 0xff, 1);
    vhw_leave();
    return value & 0xff;
}

/* OUT DX,AX: low byte to port, high byte to port+1 (how every index/data pair is used). */
unsigned outpw(int port, int value)
{
    vhw_enter();
    vhw_port_out((uint16_t)port, (uint32_t)value & 0xff, 1);
    vhw_port_out((uint16_t)(port + 1), ((uint32_t)value >> 8) & 0xff, 1);
    vhw_leave();
    return (unsigned)value & 0xffff;
}
