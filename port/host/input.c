/* input.c - SDL3 events -> XT keyboard controller / mouse / gameport. */
#include <SDL3/SDL.h>
#include "ke_port.h"
#include "../vhw/vhw.h"

#define XT_EXTENDED 0x100
#define XT_PAUSE 0x200
#define XT_PRINT_SCREEN 0x201

static uint8_t key_held[SDL_SCANCODE_COUNT];

/* SDL scancode -> XT set-1 make code; 0x100 marks an E0-prefixed key. */
static uint16_t xt_code(SDL_Scancode sc)
{
    static const uint8_t letters[26] = {0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
                                        0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
                                        0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return letters[sc - SDL_SCANCODE_A];
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return (uint16_t)(0x02 + (sc - SDL_SCANCODE_1));
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F10)
        return (uint16_t)(0x3b + (sc - SDL_SCANCODE_F1));
    switch (sc) {
    case SDL_SCANCODE_0: return 0x0b;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_RETURN2: return 0x1c;
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_BACKSPACE: return 0x0e;
    case SDL_SCANCODE_TAB: return 0x0f;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_MINUS: return 0x0c;
    case SDL_SCANCODE_EQUALS: return 0x0d;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1a;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
    case SDL_SCANCODE_BACKSLASH:
    case SDL_SCANCODE_NONUSHASH: return 0x2b;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_NONUSBACKSLASH: return 0x56;
    case SDL_SCANCODE_CAPSLOCK: return 0x3a;
    case SDL_SCANCODE_F11: return 0x57;
    case SDL_SCANCODE_F12: return 0x58;
    case SDL_SCANCODE_SCROLLLOCK: return 0x46;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
    case SDL_SCANCODE_PRINTSCREEN:
    case SDL_SCANCODE_SYSREQ: return XT_PRINT_SCREEN;
    case SDL_SCANCODE_PAUSE: return XT_PAUSE;
    case SDL_SCANCODE_LCTRL: return 0x1d;
    case SDL_SCANCODE_LSHIFT: return 0x2a;
    case SDL_SCANCODE_LALT: return 0x38;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_RCTRL: return XT_EXTENDED | 0x1d;
    case SDL_SCANCODE_RALT: return XT_EXTENDED | 0x38;
    case SDL_SCANCODE_LGUI: return XT_EXTENDED | 0x5b;
    case SDL_SCANCODE_RGUI: return XT_EXTENDED | 0x5c;
    case SDL_SCANCODE_APPLICATION:
    case SDL_SCANCODE_MENU: return XT_EXTENDED | 0x5d;
    case SDL_SCANCODE_POWER: return XT_EXTENDED | 0x5e;
    case SDL_SCANCODE_SLEEP: return XT_EXTENDED | 0x5f;
    case SDL_SCANCODE_WAKE: return XT_EXTENDED | 0x63;
    case SDL_SCANCODE_KP_DIVIDE: return XT_EXTENDED | 0x35;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_KP_MINUS: return 0x4a;
    case SDL_SCANCODE_KP_PLUS: return 0x4e;
    case SDL_SCANCODE_KP_ENTER: return XT_EXTENDED | 0x1c;
    case SDL_SCANCODE_KP_1: return 0x4f;
    case SDL_SCANCODE_KP_2: return 0x50;
    case SDL_SCANCODE_KP_3: return 0x51;
    case SDL_SCANCODE_KP_4: return 0x4b;
    case SDL_SCANCODE_KP_5: return 0x4c;
    case SDL_SCANCODE_KP_6: return 0x4d;
    case SDL_SCANCODE_KP_7: return 0x47;
    case SDL_SCANCODE_KP_8: return 0x48;
    case SDL_SCANCODE_KP_9: return 0x49;
    case SDL_SCANCODE_KP_0: return 0x52;
    case SDL_SCANCODE_KP_PERIOD: return 0x53;
    case SDL_SCANCODE_KP_EQUALS: return 0x59;
    case SDL_SCANCODE_KP_COMMA: return 0x7e;
    case SDL_SCANCODE_INTERNATIONAL1: return 0x73;
    case SDL_SCANCODE_INTERNATIONAL2: return 0x70;
    case SDL_SCANCODE_INTERNATIONAL3: return 0x7d;
    case SDL_SCANCODE_INTERNATIONAL4: return 0x79;
    case SDL_SCANCODE_INTERNATIONAL5: return 0x7b;
    case SDL_SCANCODE_INTERNATIONAL6: return 0x5c;
    case SDL_SCANCODE_INSERT: return XT_EXTENDED | 0x52;
    case SDL_SCANCODE_HOME: return XT_EXTENDED | 0x47;
    case SDL_SCANCODE_PAGEUP: return XT_EXTENDED | 0x49;
    case SDL_SCANCODE_DELETE: return XT_EXTENDED | 0x53;
    case SDL_SCANCODE_END: return XT_EXTENDED | 0x4f;
    case SDL_SCANCODE_PAGEDOWN: return XT_EXTENDED | 0x51;
    case SDL_SCANCODE_RIGHT: return XT_EXTENDED | 0x4d;
    case SDL_SCANCODE_LEFT: return XT_EXTENDED | 0x4b;
    case SDL_SCANCODE_DOWN: return XT_EXTENDED | 0x50;
    case SDL_SCANCODE_UP: return XT_EXTENDED | 0x48;
    default: return 0;
    }
}

static void push_xt_code(uint16_t code, int release)
{
    if (code & XT_EXTENDED)
        vkbd_push_scancode(0xe0);
    vkbd_push_scancode((uint8_t)((code & 0x7f) | (release ? 0x80 : 0)));
}

static void push_pause(void)
{
    /* XT set-1 E1 sequence. Pause has no break sequence. */
    static const uint8_t sequence[] = {0xe1, 0x1d, 0x45, 0xe1, 0x9d, 0xc5};
    unsigned i;
    for (i = 0; i < sizeof sequence; i++)
        vkbd_push_scancode(sequence[i]);
}

static void push_print_screen(int release)
{
    /* PrintScreen's fake shift bytes are part of the XT sequence. */
    vkbd_push_scancode(0xe0);
    vkbd_push_scancode(release ? 0xb7 : 0x2a);
    vkbd_push_scancode(0xe0);
    vkbd_push_scancode(release ? 0xaa : 0x37);
}

static int xt_key_repeats(uint16_t code)
{
    unsigned scan = code & 0x7f;
    if (code == XT_PAUSE || code == XT_PRINT_SCREEN)
        return 0;
    if ((!(code & XT_EXTENDED) &&
         (scan == 0x2a || scan == 0x36 || scan == 0x1d || scan == 0x38 ||
          scan == 0x3a || scan == 0x45 || scan == 0x46)) ||
        ((code & XT_EXTENDED) && (scan == 0x1d || scan == 0x38)))
        return 0;
    return 1;
}

static void emit_key(uint16_t code, int release)
{
    if (code == XT_PAUSE) {
        if (!release)
            push_pause();
    } else if (code == XT_PRINT_SCREEN) {
        push_print_screen(release);
    } else {
        push_xt_code(code, release);
    }
}

static void release_held_keys(void)
{
    int i;
    for (i = 0; i < SDL_SCANCODE_COUNT; i++) {
        if (key_held[i]) {
            uint16_t code = xt_code((SDL_Scancode)i);
            if (code)
                emit_key(code, 1);
            key_held[i] = 0;
        }
    }
}

void ke_input_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        SDL_Scancode sc = e->key.scancode;
        uint16_t code;
        int down = e->type == SDL_EVENT_KEY_DOWN;
        if (sc <= SDL_SCANCODE_UNKNOWN || sc >= SDL_SCANCODE_COUNT)
            break;
        code = xt_code(sc);
        if (!code)
            break;
        if (down) {
            if (!key_held[sc]) {
                key_held[sc] = 1;
                emit_key(code, 0);
            } else if (e->key.repeat && xt_key_repeats(code)) {
                /* SDL repeat timing supplies the host's PC-style typematic cadence. */
                emit_key(code, 0);
            }
        } else if (key_held[sc]) {
            key_held[sc] = 0;
            emit_key(code, 1);
        }
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        release_held_keys();
        break;
    case SDL_EVENT_MOUSE_MOTION:
        vmouse_motion(e->motion.xrel, e->motion.yrel);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        SDL_MouseButtonFlags b = SDL_GetMouseState(NULL, NULL);
        vmouse_buttons(((b & SDL_BUTTON_LMASK) ? 1 : 0) | ((b & SDL_BUTTON_RMASK) ? 2 : 0) |
                       ((b & SDL_BUTTON_MMASK) ? 4 : 0));
        break;
    }
    default:
        break;
    }
}
