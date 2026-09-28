/* Shared presentation/input transform. Coordinates in dst are renderer output pixels. */
#ifndef KE_VIEWPORT_H
#define KE_VIEWPORT_H

typedef struct KeViewport {
    float x, y, w, h;
    int output_w, output_h;
    int game_w, game_h;
} KeViewport;

int ke_viewport_calculate(KeViewport *viewport, int game_w, int game_h,
                          int output_w, int output_h, int aspect_4_3,
                          int integer_scale);
int ke_viewport_map_window_point(const KeViewport *viewport, int window_w, int window_h,
                                 float window_x, float window_y, int *game_x, int *game_y);

#endif
