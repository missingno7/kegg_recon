#define SMOOTH_PASS_HORIZONTAL 0x01
#define SMOOTH_PASS_VERTICAL 0x02
#define SMOOTH_PASS_CROSS 0x04
#define SMOOTH_PASS_3_BY_3 0x08

extern unsigned char read_vga_pixel_entry(int, int);
extern void write_vga_pixel_entry(int, int, int);

/* Each bit enables a sequential horizontal, vertical, cross, or 3-by-3 averaging pass. */
void smooth_surface_region(int smoothing_flags, int left, int top, int right, int bottom)
{
    int x;
    int y;

    if ((smoothing_flags & SMOOTH_PASS_HORIZONTAL) == SMOOTH_PASS_HORIZONTAL) {
        for (x = left; x <= right; x++) {
            for (y = top; y <= bottom; y++) {
                write_vga_pixel_entry(x, y,
                        (read_vga_pixel_entry(x, y) +
                         (read_vga_pixel_entry(x - 1, y) + read_vga_pixel_entry(x + 1, y) + read_vga_pixel_entry(x, y))) >> 2);
            }
        }
    }

    if (smoothing_flags & SMOOTH_PASS_VERTICAL) {
        for (y = top; y <= bottom; y++) {
            for (x = left; x <= right; x++) {
                write_vga_pixel_entry(x, y,
                        (read_vga_pixel_entry(x, y) +
                         (read_vga_pixel_entry(x, y - 1) + read_vga_pixel_entry(x, y + 1) + read_vga_pixel_entry(x, y))) >> 2);
            }
        }
    }

    if (smoothing_flags & SMOOTH_PASS_CROSS) {
        for (y = top; y <= bottom; y++) {
            for (x = left; x <= right; x++) {
                write_vga_pixel_entry(x, y,
                        (read_vga_pixel_entry(x, y) +
                         (((read_vga_pixel_entry(x - 1, y) + read_vga_pixel_entry(x + 1, y)) + read_vga_pixel_entry(x, y - 1)) +
                          read_vga_pixel_entry(x, y + 1))) / 5);
            }
        }
    }

    if (smoothing_flags & SMOOTH_PASS_3_BY_3) {
        for (y = top; y <= bottom; y++) {
            for (x = left; x <= right; x++) {
                write_vga_pixel_entry(x, y,
                        (read_vga_pixel_entry(x - 1, y) + read_vga_pixel_entry(x + 1, y) +
                         read_vga_pixel_entry(x, y - 1) + read_vga_pixel_entry(x, y + 1) + read_vga_pixel_entry(x, y) +
                         read_vga_pixel_entry(x - 1, y + 1) + read_vga_pixel_entry(x + 1, y - 1) +
                         read_vga_pixel_entry(x - 1, y - 1) + read_vga_pixel_entry(x + 1, y + 1)) / 9);
            }
        }
    }
}
