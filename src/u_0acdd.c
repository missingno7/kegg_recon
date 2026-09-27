/* Unreferenced eight-byte object adjacent to the heightfield state; its original role is unknown. */
unsigned char heightfield_unreferenced_bytes[8];
int height_midpoint_value;
float height_noise_scale;

#include <stdlib.h>

extern signed short drawpage;
extern int random_in_range(int, int);
extern void fill_clipped_vga_rectangle();
extern void write_vga_pixel_entry();
extern unsigned char read_vga_pixel_entry(short, short);
/* Unused neighboring renderer-state imports retained from the original unit. */
extern int font_glyph_metric_table;
extern int font_bitmap_data;
extern unsigned char text_render_state;
extern void subdivide_heightfield(short, short, short, short); /* PORT: prototype (gcc rejects old-style decl vs promoted definition) */
void perturb_height_midpoint(short, short, short, short, short, short);

/* The generated surface stores byte-height samples; the noise range is signed. */
#define HEIGHT_SAMPLE_MAX 0xff
#define HEIGHT_SAMPLE_LEVEL_COUNT 0x100
#define HEIGHT_NOISE_SCALE_MIN 0.0f
#define HEIGHT_NOISE_SCALE_MAX 1.0f

int height_min = 0;
int height_max = HEIGHT_SAMPLE_MAX;
int height_level_count = HEIGHT_SAMPLE_LEVEL_COUNT;
int height_noise_min_offset = 0;
int height_noise_max_offset = 0;

struct PackedValueRecord {
    int value;
    /* Positive values repeat this entry; negative values step backward through the table. */
    int skip_count;
};

void configure_height_generation(float noise_scale, int minimum_height, int maximum_height)
{
    int half_range;

    height_min = minimum_height;
    height_max = maximum_height;
    height_level_count = height_max - height_min + 1;
    if (noise_scale < HEIGHT_NOISE_SCALE_MIN)
        noise_scale = HEIGHT_NOISE_SCALE_MIN;
    if (noise_scale > HEIGHT_NOISE_SCALE_MAX)
        noise_scale = HEIGHT_NOISE_SCALE_MAX;
    half_range = height_level_count / 2;
    height_noise_max_offset = (int)(half_range * noise_scale);
    height_noise_min_offset = -height_noise_max_offset;
}

void generate_heightfield(float noise_scale, int left, int top, int right, int bottom)
{
    height_noise_scale = noise_scale;
    fill_clipped_vga_rectangle(drawpage, left, top, right, bottom, height_min);
    write_vga_pixel_entry(left, top, random_in_range(height_min + 1, height_max - 1));
    write_vga_pixel_entry(right, top, random_in_range(height_min + 1, height_max - 1));
    write_vga_pixel_entry(right, bottom, random_in_range(height_min + 1, height_max - 1));
    write_vga_pixel_entry(left, bottom, random_in_range(height_min + 1, height_max - 1));
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
    /* A midpoint equal to height_min marks an unset pixel in the generated surface. */
    if (read_vga_pixel_entry(midpoint_x, midpoint_y) == height_min) {
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
    if (read_vga_pixel_entry(midpoint_x, midpoint_y) != height_min)
        return;

    height_midpoint_value = random_in_range(height_noise_min_offset, height_noise_max_offset) +
                         ((read_vga_pixel_entry(x1, y1) + read_vga_pixel_entry(x2, y2)) >> 1) +
                         (int)(random_in_range(-height_level_count >> 1, height_level_count >> 1) * height_noise_scale *
                               (float)(abs(x1 - x2) + abs(y1 - y2))) /
                             (height_level_count >> 1);
    if (height_midpoint_value < height_min + 1)
        height_midpoint_value = height_min + 1;
    if (height_midpoint_value > height_max - 1)
        height_midpoint_value = height_max - 1;
    write_vga_pixel_entry(midpoint_x, midpoint_y, height_midpoint_value);
}

int next_packed_table_value(int *remaining, struct PackedValueRecord **cursor)
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
