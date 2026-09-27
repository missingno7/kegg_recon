/* kbd.c - 8042 keyboard controller (ports 60h/64h, IRQ1) and the BIOS keyboard buffer.
 *
 * The SDL main thread queues XT set-1 bytes (make, make|80h break, E0h prefixes). One byte
 * at a time sits in the controller output buffer and raises IRQ1; reading port 60h empties
 * it and loads the next byte (the new IRQ is taken after the handler's EOI, as on a PC).
 * Commands written to port 60h (EDh set LEDs + data byte) are acknowledged with FAh.
 * When the game has not hooked IRQ1, the BIOS default handler (vkbd_bios_irq1) translates
 * make codes to (scan << 8 | ascii) words in the BIOS ring used by kbhit()/getch().
 */
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

#define QUEUE 256
static uint8_t queue[QUEUE];
static unsigned q_head, q_tail;
static uint8_t out_buffer, out_full, led_pending;
static CRITICAL_SECTION kbd_lock;
static uint16_t bios_ring[32];
static unsigned bios_head, bios_tail;
static int shift_down, e0_prefix;

static const char ascii_lower[0x3a] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8, 9,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13, 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};
static const char ascii_upper[0x3a] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8, 9,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 13, 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '};

/* Lock held. Move the next queued byte into the output buffer and raise IRQ1. */
static void refill_locked(void)
{
    if (out_full || q_head == q_tail)
        return;
    out_buffer = queue[q_head];
    q_head = (q_head + 1) % QUEUE;
    out_full = 1;
    vpic_raise_irq(1);
}

static void enqueue_locked(uint8_t code)
{
    unsigned next = (q_tail + 1) % QUEUE;
    if (next != q_head) {
        queue[q_tail] = code;
        q_tail = next;
    }
}

void vkbd_push_scancode(uint8_t code)
{
    EnterCriticalSection(&kbd_lock);
    enqueue_locked(code);
    refill_locked();
    LeaveCriticalSection(&kbd_lock);
}

static uint32_t kbd_in(void *ctx, uint16_t port, int size)
{
    uint8_t v;
    (void)ctx; (void)size;
    EnterCriticalSection(&kbd_lock);
    if (port == 0x60) {
        v = out_buffer;         /* reading an empty buffer returns the last byte again */
        if (out_full) {
            out_full = 0;
            refill_locked();
        }
    } else {                    /* 64h status: bit0 output full, bit1 input full, bit2 sys */
        v = (uint8_t)(0x14 | (out_full ? 1 : 0));
    }
    LeaveCriticalSection(&kbd_lock);
    return v;
}

static void kbd_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    (void)ctx; (void)size;
    EnterCriticalSection(&kbd_lock);
    if (port == 0x60) {
        if (led_pending) {
            led_pending = 0;
            ke_log(KE_LOG_DEBUG, "kbd", "LEDs <- %02X", value & 7);
        } else if ((value & 0xff) == 0xed) {
            led_pending = 1;
        }
        enqueue_locked(0xfa);   /* ACK */
        refill_locked();
    }
    LeaveCriticalSection(&kbd_lock);
}

/* BIOS INT 09h equivalent: runs when IRQ1 is not hooked by the game. */
void vkbd_bios_irq1(void)
{
    uint8_t code = (uint8_t)vhw_port_in(0x60, 1);
    uint8_t make = code & 0x7f;
    int release = code & 0x80;
    char ascii = 0;
    if (code == 0xe0) {
        e0_prefix = 1;
        return;
    }
    if (code == 0xfa)
        return;
    if (make == 0x2a || make == 0x36) {
        shift_down = !release;
        e0_prefix = 0;
        return;
    }
    if (!release && make < 0x3a && !e0_prefix)
        ascii = shift_down ? ascii_upper[make] : ascii_lower[make];
    if (!release) {
        unsigned next = (bios_tail + 1) % 32;
        if (next != bios_head) {
            bios_ring[bios_tail] = (uint16_t)((make << 8) | (uint8_t)ascii);
            bios_tail = next;
        }
    }
    e0_prefix = 0;
}

int vkbd_bios_kbhit(void) { return bios_head != bios_tail; }

int vkbd_bios_getch(void)
{
    static int pending_scan = -1;
    uint16_t w;
    int r;
    if (pending_scan >= 0) {
        r = pending_scan;
        pending_scan = -1;
        return r;
    }
    while (bios_head == bios_tail)
        vhw_idle(5000000);
    w = bios_ring[bios_head];
    bios_head = (bios_head + 1) % 32;
    if ((w & 0xff) == 0) {      /* extended key: 0 then scan code, as Watcom getch() */
        pending_scan = w >> 8;
        return 0;
    }
    return w & 0xff;
}

/* ---- Watcom clib conio kbhit()/getch() -------------------------------------------------- */
int kbhit(void)
{
    int r;
    vhw_enter();
    r = vkbd_bios_kbhit();
    vhw_leave();
    return r;
}

int getch(void)
{
    int r;
    vhw_enter();
    r = vkbd_bios_getch();
    vhw_leave();
    return r;
}

int getche(void) { return getch(); }

void vkbd_init(void)
{
    InitializeCriticalSection(&kbd_lock);
    vhw_register_ports(0x60, 0x60, kbd_in, kbd_out, NULL, "kbd-data");
    vhw_register_ports(0x64, 0x64, kbd_in, kbd_out, NULL, "kbd-status");
}
