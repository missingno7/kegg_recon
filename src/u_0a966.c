extern unsigned char read_vga_pixel_entry(int, int);
extern void write_vga_pixel_entry(int, int, int);

/* The flags select horizontal, vertical, cross, and 3x3 neighborhood smoothing passes. */
void smooth_surface_region(int filter_flags, int left, int top, int right, int bottom)
{
    int x;
    int y;

    if ((filter_flags & 1) == 1) {
        for (x = left; x <= right; x++) {
            for (y = top; y <= bottom; y++) {
                write_vga_pixel_entry(x, y,
                        (read_vga_pixel_entry(x, y) +
                         (read_vga_pixel_entry(x - 1, y) + read_vga_pixel_entry(x + 1, y) + read_vga_pixel_entry(x, y))) >> 2);
            }
        }
    }

    if (filter_flags & 2) {
        for (y = top; y <= bottom; y++) {
            for (x = left; x <= right; x++) {
                write_vga_pixel_entry(x, y,
                        (read_vga_pixel_entry(x, y) +
                         (read_vga_pixel_entry(x, y - 1) + read_vga_pixel_entry(x, y + 1) + read_vga_pixel_entry(x, y))) >> 2);
            }
        }
    }

    if (filter_flags & 4) {
        for (y = top; y <= bottom; y++) {
            for (x = left; x <= right; x++) {
                write_vga_pixel_entry(x, y,
                        (read_vga_pixel_entry(x, y) +
                         (((read_vga_pixel_entry(x - 1, y) + read_vga_pixel_entry(x + 1, y)) + read_vga_pixel_entry(x, y - 1)) +
                          read_vga_pixel_entry(x, y + 1))) / 5);
            }
        }
    }

    if (filter_flags & 8) {
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
