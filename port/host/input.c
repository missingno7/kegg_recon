/* input.c - SDL3 events -> virtual keyboard controller / mouse / gameport. */
#include <SDL3/SDL.h>
#include "ke_port.h"
#include "../vhw/vhw.h"

/* SDL scancode -> XT set-1 make code; 0x100 flag = E0-prefixed extended key. */
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
    case SDL_SCANCODE_RETURN: return 0x1c;
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_BACKSPACE: return 0x0e;
    case SDL_SCANCODE_TAB: return 0x0f;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_MINUS: return 0x0c;
    case SDL_SCANCODE_EQUALS: return 0x0d;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1a;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
    case SDL_SCANCODE_BACKSLASH: return 0x2b;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_CAPSLOCK: return 0x3a;
    case SDL_SCANCODE_F11: return 0x57;
    case SDL_SCANCODE_F12: return 0x58;
    case SDL_SCANCODE_SCROLLLOCK: return 0x46;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
    case SDL_SCANCODE_LCTRL: return 0x1d;
    case SDL_SCANCODE_LSHIFT: return 0x2a;
    case SDL_SCANCODE_LALT: return 0x38;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_RCTRL: return 0x11d;
    case SDL_SCANCODE_RALT: return 0x138;
    case SDL_SCANCODE_KP_DIVIDE: return 0x135;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_KP_MINUS: return 0x4a;
    case SDL_SCANCODE_KP_PLUS: return 0x4e;
    case SDL_SCANCODE_KP_ENTER: return 0x11c;
    case SDL_SCANCODE_KP_7: return 0x47;
    case SDL_SCANCODE_KP_8: return 0x48;
    case SDL_SCANCODE_KP_9: return 0x49;
    case SDL_SCANCODE_KP_4: return 0x4b;
    case SDL_SCANCODE_KP_5: return 0x4c;
    case SDL_SCANCODE_KP_6: return 0x4d;
    case SDL_SCANCODE_KP_1: return 0x4f;
    case SDL_SCANCODE_KP_2: return 0x50;
    case SDL_SCANCODE_KP_3: return 0x51;
    case SDL_SCANCODE_KP_0: return 0x52;
    case SDL_SCANCODE_KP_PERIOD: return 0x53;
    case SDL_SCANCODE_HOME: return 0x147;
    case SDL_SCANCODE_UP: return 0x148;
    case SDL_SCANCODE_PAGEUP: return 0x149;
    case SDL_SCANCODE_LEFT: return 0x14b;
    case SDL_SCANCODE_RIGHT: return 0x14d;
    case SDL_SCANCODE_END: return 0x14f;
    case SDL_SCANCODE_DOWN: return 0x150;
    case SDL_SCANCODE_PAGEDOWN: return 0x151;
    case SDL_SCANCODE_INSERT: return 0x152;
    case SDL_SCANCODE_DELETE: return 0x153;
    case SDL_SCANCODE_PAUSE: return 0x45;  /* simplified: no E1 sequence */
    default: return 0;
    }
}

void ke_input_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        uint16_t code = xt_code(e->key.scancode);
        if (!code || (e->type == SDL_EVENT_KEY_DOWN && e->key.repeat && code == 0x45))
            break;
        /* Typematic repeat is delivered as repeated make codes, like a PC keyboard. */
        if (code & 0x100)
            vkbd_push_scancode(0xe0);
        vkbd_push_scancode((uint8_t)((code & 0x7f) | (e->type == SDL_EVENT_KEY_UP ? 0x80 : 0)));
        break;
    }
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
