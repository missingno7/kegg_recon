/* kbd.c - 8042 keyboard controller (ports 60h/64h, IRQ1) and BIOS keyboard services.
 *
 * The SDL main thread queues XT set-1 bytes (including E0/E1 sequences). One byte at a
 * time sits in the controller output buffer and raises IRQ1; reading port 60h empties it
 * and loads the next byte (the new IRQ is taken after the handler's EOI, as on a PC).
 * Keyboard commands written to port 60h are acknowledged with FAh; EDh's following byte
 * updates the virtual lock LEDs. When IRQ1 is not hooked, the BIOS handler translates make
 * codes into (scan << 8 | ASCII) words in the BIOS ring used by kbhit()/getch().
 */
#include "../platform/ke_platform.h"
#include "vhw.h"
#include "../include/ke_port.h"

#define QUEUE 256
#define BIOS_RING_SIZE 32
#define BDA_KEYBOARD_FLAGS 0x417

extern uint8_t ke_lowmem_shadow[0x10000];

static uint8_t queue[QUEUE];
static unsigned q_head, q_tail;
static uint8_t out_buffer, out_full, led_pending, led_state;
static KeMutex kbd_lock;
static uint16_t bios_ring[BIOS_RING_SIZE];
static unsigned bios_head, bios_tail;
static uint8_t bios_down[2][128];
static int e0_prefix, e1_bytes_left;

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
    ke_mutex_lock(&kbd_lock);
    enqueue_locked(code);
    refill_locked();
    ke_mutex_unlock(&kbd_lock);
}

static uint32_t kbd_in(void *ctx, uint16_t port, int size)
{
    uint8_t v;
    (void)ctx; (void)size;
    ke_mutex_lock(&kbd_lock);
    if (port == 0x60) {
        v = out_buffer;         /* reading an empty buffer returns the last byte again */
        if (out_full) {
            out_full = 0;
            refill_locked();
        }
    } else {                    /* 64h status: bit0 output full, bit2 system */
        v = (uint8_t)(0x14 | (out_full ? 1 : 0));
    }
    ke_mutex_unlock(&kbd_lock);
    return v;
}

static void kbd_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    uint8_t byte = (uint8_t)value;
    (void)ctx; (void)size;
    if (port != 0x60)
        return;
    ke_mutex_lock(&kbd_lock);
    if (led_pending) {
        led_pending = 0;
        led_state = byte & 7;
        ke_log(KE_LOG_DEBUG, "kbd", "LEDs <- %02X", led_state);
    } else if (byte == 0xed) {
        led_pending = 1;
    }
    enqueue_locked(0xfa);       /* ACK */
    refill_locked();
    ke_mutex_unlock(&kbd_lock);
}

static void bios_update_shift_flags_locked(void)
{
    uint8_t flags = (uint8_t)(ke_lowmem_shadow[BDA_KEYBOARD_FLAGS] & 0xf0);
    if (bios_down[0][0x36]) flags |= 0x01; /* right shift */
    if (bios_down[0][0x2a]) flags |= 0x02; /* left shift */
    if (bios_down[0][0x1d] || bios_down[1][0x1d]) flags |= 0x04; /* Ctrl */
    if (bios_down[0][0x38] || bios_down[1][0x38]) flags |= 0x08; /* Alt */
    ke_lowmem_shadow[BDA_KEYBOARD_FLAGS] = flags;
}

static int bios_key_ascii(uint8_t scan, int extended, uint8_t flags)
{
    static const char keypad_digits[13] = {
        '7', '8', '9', 0, '4', '5', '6', 0, '1', '2', '3', '0', '.'};
    static const uint8_t keypad_scans[13] = {
        0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53};
    int i, shifted = (flags & 0x03) != 0;
    char c = 0;
    if (extended)
        return 0;
    if (scan >= 0x47 && scan <= 0x53) {
        for (i = 0; i < 13; i++)
            if (keypad_scans[i] == scan)
                break;
        if (i < 13) {
            if (scan == 0x4a) return '-';
            if (scan == 0x4e) return '+';
            /* Shift temporarily reverses NumLock's keypad navigation mode. */
            return (!!(flags & 0x20) ^ shifted) ? keypad_digits[i] : 0;
        }
    }
    if (scan >= sizeof ascii_lower)
        return 0;
    c = ((shifted ^ !!(flags & 0x40)) ? ascii_upper : ascii_lower)[scan];
    if ((flags & 0x04) && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 1);
    else if ((flags & 0x04) && c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 1);
    return (uint8_t)c;
}

static void bios_ring_push_locked(uint16_t word)
{
    unsigned next = (bios_tail + 1) % BIOS_RING_SIZE;
    if (next != bios_head) {
        bios_ring[bios_tail] = word;
        bios_tail = next;
    }
}

/* BIOS INT 09h equivalent: runs when IRQ1 is not hooked by the game. */
void vkbd_bios_irq1(void)
{
    uint8_t code = (uint8_t)vhw_port_in(0x60, 1);
    uint8_t scan;
    uint8_t flags;
    int release, extended, was_down, ascii;
    ke_mutex_lock(&kbd_lock);
    if (e1_bytes_left) {
        e1_bytes_left--;
        e0_prefix = 0;
        ke_mutex_unlock(&kbd_lock);
        return;
    }
    if (code == 0xe1) {
        e0_prefix = 0;
        e1_bytes_left = 5;       /* remaining bytes of E1 1D 45 E1 9D C5 */
        ke_mutex_unlock(&kbd_lock);
        return;
    }
    if (code == 0xe0) {
        e0_prefix = 1;
        ke_mutex_unlock(&kbd_lock);
        return;
    }
    if (code == 0xfa || code == 0xfe) { /* keyboard ACK / RESEND, not a key */
        e0_prefix = 0;
        ke_mutex_unlock(&kbd_lock);
        return;
    }
    extended = e0_prefix;
    e0_prefix = 0;
    scan = code & 0x7f;
    release = (code & 0x80) != 0;
    if (extended && scan == 0x2a) { /* PrintScreen's fake left-shift bytes */
        ke_mutex_unlock(&kbd_lock);
        return;
    }
    was_down = bios_down[extended][scan] != 0;
    bios_down[extended][scan] = (uint8_t)!release;
    if (!extended && !release && !was_down) {
        if (scan == 0x3a) ke_lowmem_shadow[BDA_KEYBOARD_FLAGS] ^= 0x40; /* CapsLock */
        if (scan == 0x45) ke_lowmem_shadow[BDA_KEYBOARD_FLAGS] ^= 0x20; /* NumLock */
        if (scan == 0x46) ke_lowmem_shadow[BDA_KEYBOARD_FLAGS] ^= 0x10; /* ScrollLock */
    }
    bios_update_shift_flags_locked();
    if (release || (!extended && (scan == 0x2a || scan == 0x36 || scan == 0x1d ||
                                  scan == 0x38 || scan == 0x3a || scan == 0x45 ||
                                  scan == 0x46))) {
        ke_mutex_unlock(&kbd_lock);
        return;
    }
    flags = ke_lowmem_shadow[BDA_KEYBOARD_FLAGS];
    ascii = bios_key_ascii(scan, extended, flags);
    bios_ring_push_locked((uint16_t)(((uint16_t)scan << 8) | (uint8_t)ascii));
    ke_mutex_unlock(&kbd_lock);
}

int vkbd_bios_kbhit(void)
{
    int ready;
    ke_mutex_lock(&kbd_lock);
    ready = bios_head != bios_tail;
    ke_mutex_unlock(&kbd_lock);
    return ready;
}

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
    for (;;) {
        ke_mutex_lock(&kbd_lock);
        if (bios_head != bios_tail) {
            w = bios_ring[bios_head];
            bios_head = (bios_head + 1) % BIOS_RING_SIZE;
            ke_mutex_unlock(&kbd_lock);
            break;
        }
        ke_mutex_unlock(&kbd_lock);
        vhw_idle(5000000);
    }
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
    ke_mutex_init(&kbd_lock);
    vhw_register_ports(0x60, 0x60, kbd_in, kbd_out, NULL, "kbd-data");
    vhw_register_ports(0x64, 0x64, kbd_in, kbd_out, NULL, "kbd-status");
}
