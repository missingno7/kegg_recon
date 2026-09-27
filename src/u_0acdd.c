/* Scratch used by the recursive midpoint-displacement heightfield generator. */
unsigned char g_e1f0[8];
int midpoint_height;
float g_height_noise_scale;

#include <stdlib.h>

extern signed short draw_idx;
extern int random_in_range(int, int);
extern void fill_clipped_vga_rectangle();
extern void write_vga_pixel_entry();
extern unsigned char read_vga_pixel_entry(short, short);
extern int g_e200;
extern int g_e204;
extern unsigned char g_text_render_state;
extern int g_e209;
extern int g_e20d;
extern int g_e219;
extern int g_e21d;
extern int g_e221;
extern int g_e225;
extern void subdivide_heightfield();
void perturb_height_midpoint(short, short, short, short, short, short);

/* These globals are the permitted height range and the signed random offset limits. */
int g_height_min = 0;
int g_height_max = 0xff;
int g_height_range = 0x100;
int g_noise_min = 0;
int g_noise_max = 0;

typedef struct {
    int value;
    int skip_count;
} PackedValueRecord;

void configure_height_generation(float noise_scale, int minimum, int maximum)
{
    int half_range;

    g_height_min = minimum;
    g_height_max = maximum;
    g_height_range = g_height_max - g_height_min + 1;
    if (noise_scale < 0.0f)
        noise_scale = 0.0f;
    if (noise_scale > 1.0f)
        noise_scale = 1.0f;
    half_range = g_height_range / 2;
    g_noise_max = (int)(half_range * noise_scale);
    g_noise_min = -g_noise_max;
}

void generate_heightfield(float noise_scale, int left, int top, int right, int bottom)
{
    g_height_noise_scale = noise_scale;
    fill_clipped_vga_rectangle(draw_idx, left, top, right, bottom, g_height_min);
    write_vga_pixel_entry(left, top, random_in_range(g_height_min + 1, g_height_max - 1));
    write_vga_pixel_entry(right, top, random_in_range(g_height_min + 1, g_height_max - 1));
    write_vga_pixel_entry(right, bottom, random_in_range(g_height_min + 1, g_height_max - 1));
    write_vga_pixel_entry(left, bottom, random_in_range(g_height_min + 1, g_height_max - 1));
    subdivide_heightfield(left, top, right, bottom);
}

void subdivide_heightfield(short left, short top, short right, short bottom)
{
    short midpoint_x;
    short midpoint_y;

    if (right - left < 2) {
        if (bottom - top < 2)
            return;
    }

    midpoint_x = (left + right) >> 1;
    midpoint_y = (top + bottom) >> 1;
    perturb_height_midpoint(left, top, right, top, midpoint_x, top);
    perturb_height_midpoint(right, top, right, bottom, right, midpoint_y);
    perturb_height_midpoint(left, bottom, right, bottom, midpoint_x, bottom);
    perturb_height_midpoint(left, top, left, bottom, left, midpoint_y);
    if (read_vga_pixel_entry(midpoint_x, midpoint_y) == g_height_min) {
        write_vga_pixel_entry(midpoint_x, midpoint_y,
                (read_vga_pixel_entry(left, top) + read_vga_pixel_entry(right, top) +
                 read_vga_pixel_entry(right, bottom) + read_vga_pixel_entry(left, bottom)) >> 2);
    }
    subdivide_heightfield(left, top, midpoint_x, midpoint_y);
    subdivide_heightfield(midpoint_x, top, right, midpoint_y);
    subdivide_heightfield(midpoint_x, midpoint_y, right, bottom);
    subdivide_heightfield(left, midpoint_y, midpoint_x, bottom);
}

void perturb_height_midpoint(short x1, short y1, short x2, short y2, short midpoint_x, short midpoint_y)
{
    if (read_vga_pixel_entry(midpoint_x, midpoint_y) != g_height_min)
        return;

    midpoint_height = random_in_range(g_noise_min, g_noise_max) +
                         ((read_vga_pixel_entry(x1, y1) + read_vga_pixel_entry(x2, y2)) >> 1) +
                         (int)(random_in_range(-g_height_range >> 1, g_height_range >> 1) * g_height_noise_scale *
                               (float)(abs(x1 - x2) + abs(y1 - y2))) /
                             (g_height_range >> 1);
    if (midpoint_height < g_height_min + 1)
        midpoint_height = g_height_min + 1;
    if (midpoint_height > g_height_max - 1)
        midpoint_height = g_height_max - 1;
    write_vga_pixel_entry(midpoint_x, midpoint_y, midpoint_height);
}

int next_packed_table_value(int *remaining, PackedValueRecord **cursor)
{
    --*remaining;
    if (*remaining <= 0) {
        *cursor += 1;
        if ((*remaining = (*cursor)->skip_count) < 0) {
            *cursor += *remaining;
            *remaining = (*cursor)->skip_count;
        }
    }
    return (*cursor)->value;
}
