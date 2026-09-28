/* test_joystick.c - SDL virtual gamepad through the production input and gameport paths. */
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

typedef struct TestJoystickState {
    unsigned int button_bits;
    short axis_timing[12];
} TestJoystickState;

extern int detect_joystick(void);
extern void poll_joystick_ports(void);
extern short joystick_available;
extern int joystick_max_axis_time;
extern TestJoystickState primary_joystick;

void ke_input_event(const SDL_Event *event);

static int feed_gamepad_events(void)
{
    SDL_Event event;
    int count = 0;
    SDL_PumpEvents();
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
        case SDL_EVENT_GAMEPAD_REMAPPED:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            ke_input_event(&event);
            ++count;
            break;
        default:
            break;
        }
    }
    return count;
}

static int test_sdl_virtual_gamepad_path(void)
{
    SDL_VirtualJoystickDesc desc;
    SDL_JoystickID id = 0;
    SDL_Joystick *joystick = NULL;
    int failures = 0;
    int previous_joystick_setting = ke_config.joystick;
    int sdl_started = 0;

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL gamepad init failed: %s\n", SDL_GetError());
        return 1;
    }
    sdl_started = 1;
    ke_config.joystick = 1;
    /* Earlier differential fixtures may have left their per-port override active. */
    vhw_set_port_override(NULL, NULL, NULL);
    vjoy_init();

    memset(&desc, 0, sizeof desc);
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_LEFTX) | (1u << SDL_GAMEPAD_AXIS_LEFTY) |
                     (1u << SDL_GAMEPAD_AXIS_RIGHTX) | (1u << SDL_GAMEPAD_AXIS_RIGHTY);
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_SOUTH) | (1u << SDL_GAMEPAD_BUTTON_EAST) |
                       (1u << SDL_GAMEPAD_BUTTON_WEST) | (1u << SDL_GAMEPAD_BUTTON_NORTH);
    desc.name = "Krypton Egg test gamepad";
    id = SDL_AttachVirtualJoystick(&desc);
    if (!id) {
        fprintf(stderr, "SDL virtual gamepad attach failed: %s\n", SDL_GetError());
        failures++;
        goto done;
    }
    joystick = SDL_OpenJoystick(id);
    if (!joystick) {
        fprintf(stderr, "SDL virtual gamepad open failed: %s\n", SDL_GetError());
        failures++;
        goto done;
    }
    if (feed_gamepad_events() == 0) {
        fprintf(stderr, "SDL emitted no virtual gamepad attach event\n");
        failures++;
        goto done;
    }

    if (detect_joystick() != -1 || joystick_available != -1) {
        fprintf(stderr, "game did not detect the SDL-backed 201h gameport\n");
        failures++;
        goto done;
    }
    if (primary_joystick.axis_timing[7] < 32 || primary_joystick.axis_timing[10] < 32 ||
        joystick_max_axis_time <= 0) {
        fprintf(stderr, "T15 neutral calibration was not populated (center %d,%d max %d)\n",
                primary_joystick.axis_timing[7], primary_joystick.axis_timing[10],
                joystick_max_axis_time);
        failures++;
        goto done;
    }

    if (!SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, -32768) ||
        !SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true)) {
        fprintf(stderr, "SDL virtual gamepad input failed: %s\n", SDL_GetError());
        failures++;
        goto done;
    }
    feed_gamepad_events();
    poll_joystick_ports();
    if ((primary_joystick.button_bits & 0x11u) != 0x11u) {
        fprintf(stderr, "T15 left/fire state missing (button_bits=%#x)\n",
                primary_joystick.button_bits);
        failures++;
    }

    if (!SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 32767) ||
        !SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false)) {
        fprintf(stderr, "SDL virtual gamepad input failed: %s\n", SDL_GetError());
        failures++;
        goto done;
    }
    feed_gamepad_events();
    poll_joystick_ports();
    if ((primary_joystick.button_bits & 0x12u) != 0x02u) {
        fprintf(stderr, "T15 right/released-fire state incorrect (button_bits=%#x)\n",
                primary_joystick.button_bits);
        failures++;
    }

    if (!SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 6000)) {
        fprintf(stderr, "SDL virtual dead-zone input failed: %s\n", SDL_GetError());
        failures++;
        goto done;
    }
    feed_gamepad_events();
    poll_joystick_ports();
    if ((primary_joystick.button_bits & 0x03u) != 0) {
        fprintf(stderr, "T15 dead-zone state moved (button_bits=%#x)\n",
                primary_joystick.button_bits);
        failures++;
    }

done:
    if (joystick)
        SDL_CloseJoystick(joystick);
    if (id)
        SDL_DetachVirtualJoystick(id);
    if (sdl_started) {
        feed_gamepad_events();
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
    ke_config.joystick = previous_joystick_setting;
    if (!failures)
        printf("  SDL virtual pad -> ke_input_event -> 201h -> T15 detection/calibration/directions\n");
    return failures;
}

void register_joystick_tests(void)
{
    oracle_register("SDL virtual joystick / T15 gameport", test_sdl_virtual_gamepad_path);
}
