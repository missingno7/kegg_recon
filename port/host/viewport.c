/* viewport.c - canonical renderer destination and window-point to game mapping. */
#include "../include/viewport.h"

int ke_viewport_calculate(KeViewport *viewport, int game_w, int game_h,
                          int output_w, int output_h, int aspect_4_3,
                          int integer_scale)
{
    float scale_x, scale_y, scale;
    if (!viewport || game_w <= 0 || game_h <= 0 || output_w <= 0 || output_h <= 0)
        return 0;
    scale_x = (float)output_w / (float)game_w;
    scale_y = aspect_4_3 ? (float)output_h / ((float)game_w * 3.0f / 4.0f) :
                           (float)output_h / (float)game_h;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if (integer_scale)
        scale = (float)(int)scale;
    if (scale < 1.0f)
        scale = 1.0f;
    viewport->w = (float)game_w * scale;
    viewport->h = aspect_4_3 ? viewport->w * 3.0f / 4.0f : (float)game_h * scale;
    viewport->x = ((float)output_w - viewport->w) / 2.0f;
    viewport->y = ((float)output_h - viewport->h) / 2.0f;
    viewport->output_w = output_w;
    viewport->output_h = output_h;
    viewport->game_w = game_w;
    viewport->game_h = game_h;
    return 1;
}

static int clamp_pixel(float coordinate, int count)
{
    int pixel;
    if (coordinate <= 0.0f)
        return 0;
    if (coordinate >= 1.0f)
        return count - 1;
    pixel = (int)(coordinate * count);
    return pixel < count ? pixel : count - 1;
}

int ke_viewport_map_window_point(const KeViewport *viewport, int window_w, int window_h,
                                 float window_x, float window_y, int *game_x, int *game_y)
{
    float output_x, output_y, normalized_x, normalized_y;
    if (!viewport || !game_x || !game_y || window_w <= 0 || window_h <= 0 ||
        viewport->output_w <= 0 || viewport->output_h <= 0 ||
        viewport->game_w <= 0 || viewport->game_h <= 0 || viewport->w <= 0.0f ||
        viewport->h <= 0.0f)
        return 0;
    output_x = window_x * (float)viewport->output_w / (float)window_w;
    output_y = window_y * (float)viewport->output_h / (float)window_h;
    normalized_x = (output_x - viewport->x) / viewport->w;
    normalized_y = (output_y - viewport->y) / viewport->h;
    /* Letterbox/pillarbox and outside-window positions stick to the nearest game edge. */
    *game_x = clamp_pixel(normalized_x, viewport->game_w);
    *game_y = clamp_pixel(normalized_y, viewport->game_h);
    return 1;
}
