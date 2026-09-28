/* mouse_native.c - SDL window points -> shared presentation viewport -> INT 33h position. */
#include "ke_port.h"
#include "../vhw/vhw.h"

void ke_native_mouse_set_game_position(int game_x, int game_y)
{
    /* t17_mouse.c reads function 03 and halves the DOS driver's 2:1 coordinate range. */
    vmouse_set_absolute_position(game_x * 2, game_y * 2);
}

void ke_native_mouse_position(float window_x, float window_y)
{
    int game_x, game_y;
    if (ke_present_map_mouse(window_x, window_y, &game_x, &game_y))
        ke_native_mouse_set_game_position(game_x, game_y);
}
