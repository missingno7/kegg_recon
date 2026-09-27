/* joy.c - PC gameport (port 201h), attached with KE_JOY=1.
 *
 * Writing 201h fires the four one-shots; each axis bit then reads 1 for a duration
 * proportional to the stick position. The game measures that duration by counting its own
 * polling iterations with interrupts disabled, so the virtual one-shot is clocked by the
 * number of port reads (deterministic, host-speed independent) rather than by host time.
 * Buttons are bits 4-7, active low. Without KE_JOY the port is unclaimed (reads FFh: no
 * gameport, every axis times out), which is what the game sees on a PC without a joystick.
 *
 * WORK PACKAGE "mouse-joystick": SDL gamepad mapping, dead zone, calibration flow.
 */
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

#define READS_MIN 24
#define READS_RANGE 1000

static volatile LONG axis_reads[4] = {READS_MIN + READS_RANGE / 2, READS_MIN + READS_RANGE / 2,
                                      READS_MIN + READS_RANGE / 2, READS_MIN + READS_RANGE / 2};
static volatile LONG button_bits;        /* 1 = pressed, bits 0..3 = buttons 1..4 */
static int remaining[4];

void vjoy_set(int axis, float value, int buttons)
{
    if (axis >= 0 && axis < 4) {
        if (value < -1.0f) value = -1.0f;
        if (value > 1.0f) value = 1.0f;
        InterlockedExchange(&axis_reads[axis], READS_MIN + (LONG)((value + 1.0f) * 0.5f * READS_RANGE));
    }
    if (buttons >= 0)
        InterlockedExchange(&button_bits, buttons & 0x0f);
}

static uint32_t joy_in(void *ctx, uint16_t port, int size)
{
    uint8_t v = (uint8_t)(0xf0 & ~(button_bits << 4));
    int i;
    (void)ctx; (void)port; (void)size;
    for (i = 0; i < 4; i++)
        if (remaining[i] > 0) {
            v |= (uint8_t)(1u << i);
            remaining[i]--;
        }
    return v;
}

static void joy_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    int i;
    (void)ctx; (void)port; (void)value; (void)size;
    for (i = 0; i < 4; i++)
        remaining[i] = (int)axis_reads[i];
}

void vjoy_init(void)
{
    if (!ke_config.joystick)
        return;
    vhw_register_ports(0x201, 0x201, joy_in, joy_out, NULL, "gameport");
    ke_log(KE_LOG_INFO, "joy", "gameport attached at 201h");
}
