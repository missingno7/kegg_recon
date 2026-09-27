/* input.c - SDL3 events -> XT keyboard controller / mouse / gameport. */
#include <SDL3/SDL.h>
#include "ke_port.h"
#include "replay.h"
#include "../vhw/vhw.h"

static SDL_Window *input_window;
static SDL_Gamepad *active_gamepad;
static SDL_JoystickID active_gamepad_id;
static float gamepad_axes[4];
static int gamepad_button_bits;
static int mouse_button_bits;
static int mouse_captured;

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

static void update_gameport(void)
{
    int i;
    for (i = 0; i < 4; ++i)
        vjoy_set(i, gamepad_axes[i], gamepad_button_bits);
}

/* Deterministic smoke/replay injection enters through the same virtual devices as SDL. */
void ke_input_set_mouse_position(int x, int y)
{
    union REGS r;
    struct SREGS s;
    memset(&r, 0, sizeof r);
    memset(&s, 0, sizeof s);
    r.w.ax = 0x04;
    r.w.cx = (unsigned short)x;
    r.w.dx = (unsigned short)y;
    vmouse_int33(&r, &s);
}

void ke_input_set_mouse_buttons(int buttons)
{
    mouse_button_bits = buttons & 7;
    vmouse_buttons(mouse_button_bits);
}

void ke_input_push_scancode(uint8_t code)
{
    vkbd_push_scancode(code);
}

static float normalize_gamepad_axis(Sint16 value)
{
    const int dead_zone = 8000;
    if (value > dead_zone)
        return (float)(value - dead_zone) / (32767 - dead_zone);
    if (value < -dead_zone)
        return (float)(value + dead_zone) / (32768 - dead_zone);
    return 0.0f;
}

static int gamepad_axis_slot(Uint8 axis)
{
    switch ((SDL_GamepadAxis)axis) {
    case SDL_GAMEPAD_AXIS_LEFTX: return 0;
    case SDL_GAMEPAD_AXIS_LEFTY: return 1;
    case SDL_GAMEPAD_AXIS_RIGHTX: return 2;
    case SDL_GAMEPAD_AXIS_RIGHTY: return 3;
    default: return -1;
    }
}

static int gamepad_button_mask(Uint8 button)
{
    switch ((SDL_GamepadButton)button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return 0x01;
    case SDL_GAMEPAD_BUTTON_EAST: return 0x02;
    case SDL_GAMEPAD_BUTTON_WEST: return 0x04;
    case SDL_GAMEPAD_BUTTON_NORTH: return 0x08;
    default: return 0;
    }
}

static void sample_gamepad(void)
{
    static const SDL_GamepadAxis axes[4] = {
        SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY
    };
    static const SDL_GamepadButton buttons[4] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH
    };
    int i;

    if (!active_gamepad)
        return;
    for (i = 0; i < 4; ++i) {
        gamepad_axes[i] = normalize_gamepad_axis(SDL_GetGamepadAxis(active_gamepad, axes[i]));
        if (SDL_GetGamepadButton(active_gamepad, buttons[i]))
            gamepad_button_bits |= 1 << i;
        else
            gamepad_button_bits &= ~(1 << i);
    }
    update_gameport();
}

static void open_gamepad(SDL_JoystickID id)
{
    const char *name;
    if (!ke_config.joystick || active_gamepad)
        return;
    active_gamepad = SDL_OpenGamepad(id);
    if (!active_gamepad) {
        ke_log(KE_LOG_WARN, "input", "could not open SDL gamepad %d: %s", (int)id, SDL_GetError());
        return;
    }
    active_gamepad_id = id;
    name = SDL_GetGamepadName(active_gamepad);
    ke_log(KE_LOG_INFO, "input", "gamepad attached to gameport: %s", name ? name : "(unnamed)");
    sample_gamepad();
}

static void open_next_gamepad(void)
{
    SDL_JoystickID *ids;
    int count = 0, i;
    ids = SDL_GetGamepads(&count);
    for (i = 0; ids && i < count && !active_gamepad; ++i)
        open_gamepad(ids[i]);
    SDL_free(ids);
}

static void set_mouse_capture(int capture)
{
    if (!input_window || mouse_captured == capture)
        return;
    if (SDL_SetWindowRelativeMouseMode(input_window, capture)) {
        mouse_captured = capture;
        SDL_SetWindowTitle(input_window, capture ?
            "Krypton Egg - mouse captured (Esc releases)" :
            "Krypton Egg - click to capture mouse");
    } else {
        if (!capture)
            mouse_captured = 0;
        ke_log(KE_LOG_WARN, "input", "mouse %s failed: %s",
               capture ? "capture" : "release", SDL_GetError());
    }
}

static void set_window_from_id(SDL_WindowID id)
{
    SDL_Window *window = SDL_GetWindowFromID(id);
    if (window)
        input_window = window;
}

void ke_input_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        SDL_Scancode sc = e->key.scancode;
        uint16_t code;
        int down = e->type == SDL_EVENT_KEY_DOWN;
        if (down && sc == SDL_SCANCODE_ESCAPE) {
            set_window_from_id(e->key.windowID);
            set_mouse_capture(0);
        }
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
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        set_window_from_id(e->window.windowID);
        if (!mouse_captured && input_window)
            SDL_SetWindowTitle(input_window, "Krypton Egg - click to capture mouse");
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        release_held_keys();
        set_window_from_id(e->window.windowID);
        set_mouse_capture(0);
        mouse_button_bits = 0;
        vmouse_buttons(0);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        vmouse_motion_at(e->motion.xrel, e->motion.yrel, e->motion.timestamp);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        int button_bit = e->button.button == SDL_BUTTON_LEFT ? 1 :
                         e->button.button == SDL_BUTTON_RIGHT ? 2 :
                         e->button.button == SDL_BUTTON_MIDDLE ? 4 : 0;
        set_window_from_id(e->button.windowID);
        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT)
            set_mouse_capture(1);
        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            mouse_button_bits |= button_bit;
        else
            mouse_button_bits &= ~button_bit;
        vmouse_buttons(mouse_button_bits);
        break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        open_gamepad(e->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (active_gamepad && e->gdevice.which == active_gamepad_id) {
            SDL_CloseGamepad(active_gamepad);
            active_gamepad = NULL;
            active_gamepad_id = 0;
            gamepad_axes[0] = gamepad_axes[1] = gamepad_axes[2] = gamepad_axes[3] = 0.0f;
            gamepad_button_bits = 0;
            update_gameport();
            open_next_gamepad();
        }
        break;
    case SDL_EVENT_GAMEPAD_REMAPPED:
        if (active_gamepad && e->gdevice.which == active_gamepad_id)
            sample_gamepad();
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (active_gamepad && e->gaxis.which == active_gamepad_id) {
            int axis = gamepad_axis_slot(e->gaxis.axis);
            if (axis >= 0) {
                gamepad_axes[axis] = normalize_gamepad_axis(e->gaxis.value);
                vjoy_set(axis, gamepad_axes[axis], gamepad_button_bits);
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (active_gamepad && e->gbutton.which == active_gamepad_id) {
            int bit = gamepad_button_mask(e->gbutton.button);
            if (e->gbutton.down)
                gamepad_button_bits |= bit;
            else
                gamepad_button_bits &= ~bit;
            update_gameport();
        }
        break;
    default:
        break;
    }
}
