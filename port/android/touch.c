/* touch.c - Android touch controls, overlay buttons, Back key and soft keyboard.
 *
 * Specification: docs/android/architecture.md, "Touch UX". Everything enters the game
 * through the same virtual devices the desktop port uses:
 *   position  -> ke_native_mouse_set_game_position() (vmouse_set_absolute_position; the
 *                game's update_mouse() smoothing stays the consumer),
 *   buttons   -> ke_input_set_mouse_buttons() (INT 33h function 03 BX),
 *   keys      -> ke_input_push_scancode() (8042 controller, IRQ1).
 * Gameplay is recognised from port-side state only: the scan-out is 320x200 and the
 * virtual mouse driver's ranges are the play ranges (vertical maximum 376 = racket row 188,
 * or the horizontal range locked = boss fight); nothing in the game code is consulted.
 * The overlay is drawn by the presentation layer after the frame, never into VGA memory.
 */
#include <string.h>
#include <SDL3/SDL.h>
#include "../include/ke_port.h"
#include "../include/viewport.h"
#include "../vhw/vhw.h"
#include "ke_android.h"

void ke_input_set_mouse_buttons(int buttons);
void ke_input_push_scancode(uint8_t code);
void ke_input_event(const SDL_Event *e);
int ke_present_last_viewport(KeViewport *out);
extern unsigned char scan_code_to_ascii[128];   /* the game's own table (t16_keyboard.c) */

#define MAX_FINGERS 10
#define XT_ESC 0x01
#define XT_P 0x19
#define XT_ENTER 0x1c
#define XT_BACKSPACE 0x0e
#define TAP_HOLD_MAX_NS 150000000ull   /* a press is held until the game read it, or this */
#define KEY_HOLD_NS 60000000ull        /* paced key: make .. break .. next make           */

enum { ROLE_NONE, ROLE_POINTER, ROLE_TAP, ROLE_LEFT, ROLE_RIGHT, ROLE_PAUSE, ROLE_KEYBOARD };
enum { BTN_LEFT, BTN_RIGHT, BTN_PAUSE, BTN_KEYBOARD, BTN_COUNT };

typedef struct Finger { SDL_FingerID id; int role; int active; } Finger;
typedef struct Rect { float x, y, w, h; } Rect;
/* A mouse button that is released only after the game has read it pressed. */
typedef struct HeldButton {
    int down, release_pending;
    uint32_t reads_at_press;
    uint64_t released_at;
} HeldButton;
typedef struct PacedKey { uint8_t code; } PacedKey;

static SDL_Window *touch_window;
static Finger fingers[MAX_FINGERS];
static HeldButton held[2];                       /* [0] left, [1] right */
static Rect buttons[BTN_COUNT];
static int button_visible[BTN_COUNT];
static int gameplay_mode;
static int text_input_on;
static PacedKey key_queue[64];
static int key_head, key_tail;
static int key_phase;                            /* 0 idle, 1 make sent, 2 break sent   */
static uint64_t key_phase_at;
static uint8_t key_current;

/* ---- gameplay detection ---------------------------------------------------------------- */
int ke_touch_gameplay(void)
{
    KeViewport vp;
    int x_min, x_max, y_min, y_max;
    if (!ke_present_last_viewport(&vp) || vp.game_w != 320 || vp.game_h != 200)
        return 0;
    vmouse_get_ranges(&x_min, &x_max, &y_min, &y_max);
    return y_max == 376 || x_min == x_max;
}

/* ---- mouse buttons with a minimum hold ---------------------------------------------- */
static int button_mask(void)
{
    return (held[0].down ? 1 : 0) | (held[1].down ? 2 : 0);
}

static void press_button(int which)
{
    held[which].down = 1;
    held[which].release_pending = 0;
    held[which].reads_at_press = vmouse_position_reads();
    ke_input_set_mouse_buttons(button_mask());
}

static void release_button_now(int which)
{
    held[which].down = 0;
    held[which].release_pending = 0;
    ke_input_set_mouse_buttons(button_mask());
}

static void release_button(int which)
{
    if (!held[which].down)
        return;
    if (vmouse_position_reads() - held[which].reads_at_press >= 2) {
        release_button_now(which);
    } else {
        held[which].release_pending = 1;       /* the game has not seen the press yet */
        held[which].released_at = ke_now_ns();
    }
}

static void update_held(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        if (held[i].release_pending &&
            (vmouse_position_reads() - held[i].reads_at_press >= 2 ||
             ke_now_ns() - held[i].released_at > TAP_HOLD_MAX_NS))
            release_button_now(i);
    }
}

/* ---- paced keys: the game samples its keyboard once per tick ------------------------- */
static void queue_key(uint8_t code)
{
    int next = (key_tail + 1) % (int)(sizeof key_queue / sizeof key_queue[0]);
    if (next == key_head)
        return;
    key_queue[key_tail].code = code;
    key_tail = next;
}

static void update_keys(void)
{
    uint64_t now = ke_now_ns();
    if (key_phase == 1 && now - key_phase_at >= KEY_HOLD_NS) {
        ke_input_push_scancode((uint8_t)(key_current | 0x80));
        key_phase = 2;
        key_phase_at = now;
    } else if (key_phase == 2 && now - key_phase_at >= KEY_HOLD_NS) {
        key_phase = 0;
    }
    if (key_phase == 0 && key_head != key_tail) {
        key_current = key_queue[key_head].code;
        key_head = (key_head + 1) % (int)(sizeof key_queue / sizeof key_queue[0]);
        ke_input_push_scancode(key_current);
        key_phase = 1;
        key_phase_at = now;
    }
}

/* Character typed on the soft keyboard -> the scan code the game's table maps to it. */
static int scan_for_char(unsigned char c)
{
    int s;
    if (c >= 'a' && c <= 'z')
        c = (unsigned char)(c - 'a' + 'A');
    if (c == ' ')
        return 0x39;
    if (c == '?')
        return -1;
    for (s = 1; s < 128; s++)
        if (scan_code_to_ascii[s] == c)
            return s;
    return -1;
}

/* ---- layout ----------------------------------------------------------------------------- */
static void layout(const KeViewport *vp, int w, int h)
{
    float left = vp ? vp->x : 0.0f;
    float right = vp ? (float)w - (vp->x + vp->w) : 0.0f;
    float pillar = left < right ? left : right;
    float s;
    memset(button_visible, 0, sizeof button_visible);
    if (!vp)
        return;
    if (pillar >= 0.12f * (float)h) {
        s = pillar * 0.82f;
        if (s > 0.36f * (float)h)
            s = 0.36f * (float)h;
        buttons[BTN_LEFT] = (Rect){left / 2 - s / 2, (float)h * 0.66f - s / 2, s, s};
        buttons[BTN_RIGHT] = (Rect){(float)w - right / 2 - s / 2, (float)h * 0.66f - s / 2, s, s};
        buttons[BTN_PAUSE] = (Rect){(float)w - right / 2 - s * 0.22f, (float)h * 0.16f - s * 0.22f,
                                    s * 0.44f, s * 0.44f};
    } else {
        s = 0.2f * (float)h;
        buttons[BTN_LEFT] = (Rect){s * 0.15f, (float)h - s * 1.15f, s, s};
        buttons[BTN_RIGHT] = (Rect){(float)w - s * 1.15f, (float)h - s * 1.15f, s, s};
        buttons[BTN_PAUSE] = (Rect){(float)w - s * 0.6f, s * 0.15f, s * 0.45f, s * 0.45f};
    }
    buttons[BTN_KEYBOARD] = buttons[BTN_PAUSE];
    if (gameplay_mode) {
        button_visible[BTN_LEFT] = button_visible[BTN_RIGHT] = button_visible[BTN_PAUSE] = 1;
    } else {
        button_visible[BTN_KEYBOARD] = 1;
    }
}

static int hit_button(float x, float y)
{
    int i;
    for (i = 0; i < BTN_COUNT; i++) {
        Rect r = buttons[i];
        float grow = r.w * 0.12f;
        if (button_visible[i] && x >= r.x - grow && x < r.x + r.w + grow &&
            y >= r.y - grow && y < r.y + r.h + grow)
            return i;
    }
    return -1;
}

/* ---- events ----------------------------------------------------------------------------- */
static Finger *finger_for(SDL_FingerID id, int create)
{
    int i;
    for (i = 0; i < MAX_FINGERS; i++)
        if (fingers[i].active && fingers[i].id == id)
            return &fingers[i];
    if (!create)
        return NULL;
    for (i = 0; i < MAX_FINGERS; i++)
        if (!fingers[i].active) {
            fingers[i].active = 1;
            fingers[i].id = id;
            fingers[i].role = ROLE_NONE;
            return &fingers[i];
        }
    return NULL;
}

static int output_point(float nx, float ny, float *x, float *y, int *w, int *h)
{
    SDL_Renderer *renderer = SDL_GetRenderer(touch_window);
    if (!renderer || !SDL_GetCurrentRenderOutputSize(renderer, w, h))
        return 0;
    *x = nx * (float)*w;
    *y = ny * (float)*h;
    return 1;
}

static void move_pointer(float x, float y, int w, int h)
{
    KeViewport vp;
    int gx, gy;
    if (!ke_present_last_viewport(&vp))
        return;
    if (ke_viewport_map_window_point(&vp, w, h, x, y, &gx, &gy))
        ke_native_mouse_set_game_position(gx, gy);
}

static int inside_image(float x, float y)
{
    KeViewport vp;
    return ke_present_last_viewport(&vp) && x >= vp.x && x < vp.x + vp.w && y >= vp.y &&
           y < vp.y + vp.h;
}

static void release_all(void)
{
    int i;
    for (i = 0; i < MAX_FINGERS; i++)
        fingers[i].active = 0;
    release_button(0);
    release_button(1);
}

static void set_text_input(int on)
{
    if (on == text_input_on)
        return;
    text_input_on = on;
    if (on)
        SDL_StartTextInput(touch_window);
    else
        SDL_StopTextInput(touch_window);
}

static void finger_down(const SDL_TouchFingerEvent *t)
{
    float x, y;
    int w, h, button;
    Finger *f = finger_for(t->fingerID, 1);
    if (!f || !output_point(t->x, t->y, &x, &y, &w, &h))
        return;
    button = hit_button(x, y);
    switch (button) {
    case BTN_LEFT: f->role = ROLE_LEFT; press_button(0); return;
    case BTN_RIGHT: f->role = ROLE_RIGHT; press_button(1); return;
    case BTN_PAUSE: f->role = ROLE_PAUSE; queue_key(XT_P); return;
    case BTN_KEYBOARD: f->role = ROLE_KEYBOARD; set_text_input(!text_input_on); return;
    default: break;
    }
    if (gameplay_mode) {
        int i;
        for (i = 0; i < MAX_FINGERS; i++)
            if (fingers[i].active && fingers[i].role == ROLE_POINTER && &fingers[i] != f)
                return;                         /* one finger steers */
        f->role = ROLE_POINTER;
        move_pointer(x, y, w, h);
    } else if (inside_image(x, y)) {
        f->role = ROLE_TAP;                     /* tap = move + left click */
        move_pointer(x, y, w, h);
        press_button(0);
    }
}

static void finger_motion(const SDL_TouchFingerEvent *t)
{
    float x, y;
    int w, h;
    Finger *f = finger_for(t->fingerID, 0);
    if (!f || !output_point(t->x, t->y, &x, &y, &w, &h))
        return;
    if (f->role == ROLE_POINTER || (f->role == ROLE_TAP && inside_image(x, y)))
        move_pointer(x, y, w, h);
}

static void finger_up(const SDL_TouchFingerEvent *t)
{
    Finger *f = finger_for(t->fingerID, 0);
    if (!f)
        return;
    if (f->role == ROLE_LEFT || f->role == ROLE_TAP)
        release_button(0);
    else if (f->role == ROLE_RIGHT)
        release_button(1);
    f->active = 0;
}

int ke_touch_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_FINGER_DOWN:
        finger_down(&e->tfinger);
        return 1;
    case SDL_EVENT_FINGER_MOTION:
        finger_motion(&e->tfinger);
        return 1;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        finger_up(&e->tfinger);
        return 1;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (e->key.scancode == SDL_SCANCODE_AC_BACK) {
            /* Back: pause/resume during play, Esc (skip, leave, quit on the menu) elsewhere. */
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) {
                if (text_input_on)
                    set_text_input(0);
                queue_key(gameplay_mode ? XT_P : XT_ESC);
            }
            return 1;
        }
        if (text_input_on) {
            /* The soft keyboard also reports its characters as key events at QWERTY
             * positions; letters, digits and space come through SDL_EVENT_TEXT_INPUT. */
            SDL_Scancode sc = e->key.scancode;
            if ((sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_0) || sc == SDL_SCANCODE_SPACE ||
                (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_0))
                return 1;
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) {
                if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
                    queue_key(XT_ENTER);
                    return 1;
                }
                if (sc == SDL_SCANCODE_BACKSPACE) {
                    queue_key(XT_BACKSPACE);
                    return 1;
                }
            }
            if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER ||
                sc == SDL_SCANCODE_BACKSPACE)
                return 1;
        }
        return 0;                               /* physical keyboard: desktop mapping */
    case SDL_EVENT_TEXT_INPUT: {
        const unsigned char *s = (const unsigned char *)e->text.text;
        for (; *s; s++) {
            int code = *s < 0x80 ? scan_for_char(*s) : -1;
            if (code > 0)
                queue_key((uint8_t)code);
        }
        return 1;
    }
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return 0;                               /* a real mouse: native absolute mode  */
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        release_all();
        set_text_input(0);
        return 1;
    default:
        return 0;
    }
}

void ke_touch_update(void)
{
    int now_gameplay = ke_touch_gameplay();
    if (now_gameplay != gameplay_mode) {
        release_all();
        gameplay_mode = now_gameplay;
        if (gameplay_mode)
            set_text_input(0);
        ke_log(KE_LOG_INFO, "touch", "%s controls", gameplay_mode ? "gameplay" : "menu");
    }
    update_held();
    update_keys();
}

void ke_touch_init(SDL_Window *window)
{
    touch_window = window;
    memset(fingers, 0, sizeof fingers);
    memset(held, 0, sizeof held);
    SDL_StopTextInput(window);
}

/* ---- overlay drawing --------------------------------------------------------------------- */
static const char *const glyph_l[7] = {"1000", "1000", "1000", "1000", "1000", "1000", "1111"};
static const char *const glyph_r[7] = {"1110", "1001", "1001", "1110", "1010", "1001", "1001"};

static void draw_glyph(SDL_Renderer *r, const char *const rows[7], Rect box)
{
    float cell = box.h * 0.5f / 7.0f;
    float ox = box.x + (box.w - cell * 4) / 2, oy = box.y + (box.h - cell * 7) / 2;
    int x, y;
    for (y = 0; y < 7; y++)
        for (x = 0; x < 4; x++)
            if (rows[y][x] == '1') {
                SDL_FRect c = {ox + x * cell, oy + y * cell, cell, cell};
                SDL_RenderFillRect(r, &c);
            }
}

static void draw_frame(SDL_Renderer *r, Rect b, int pressed)
{
    SDL_FRect fill = {b.x, b.y, b.w, b.h};
    float t = b.w * 0.03f + 1.0f;
    SDL_FRect edges[4] = {{b.x, b.y, b.w, t}, {b.x, b.y + b.h - t, b.w, t},
                          {b.x, b.y, t, b.h}, {b.x + b.w - t, b.y, t, b.h}};
    SDL_SetRenderDrawColor(r, 255, 255, 255, pressed ? 110 : 36);
    SDL_RenderFillRect(r, &fill);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 170);
    SDL_RenderFillRects(r, edges, 4);
}

void ke_touch_draw(SDL_Renderer *r, const KeViewport *vp, int w, int h)
{
    layout(vp, w, h);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (button_visible[BTN_LEFT]) {
        draw_frame(r, buttons[BTN_LEFT], held[0].down);
        draw_glyph(r, glyph_l, buttons[BTN_LEFT]);
    }
    if (button_visible[BTN_RIGHT]) {
        draw_frame(r, buttons[BTN_RIGHT], held[1].down);
        draw_glyph(r, glyph_r, buttons[BTN_RIGHT]);
    }
    if (button_visible[BTN_PAUSE]) {
        Rect b = buttons[BTN_PAUSE];
        SDL_FRect bars[2] = {{b.x + b.w * 0.3f, b.y + b.h * 0.25f, b.w * 0.14f, b.h * 0.5f},
                             {b.x + b.w * 0.56f, b.y + b.h * 0.25f, b.w * 0.14f, b.h * 0.5f}};
        draw_frame(r, b, 0);
        SDL_RenderFillRects(r, bars, 2);
    }
    if (button_visible[BTN_KEYBOARD]) {
        Rect b = buttons[BTN_KEYBOARD];
        SDL_FRect keys[9];
        int i;
        draw_frame(r, b, text_input_on);
        for (i = 0; i < 9; i++) {
            float k = b.w * 0.14f;
            keys[i] = (SDL_FRect){b.x + b.w * 0.2f + (i % 3) * k * 1.5f,
                                  b.y + b.h * 0.25f + (i / 3) * k * 1.3f, k, k};
        }
        SDL_RenderFillRects(r, keys, 9);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
