/* The short coordinate arrays are video-mode transform terms used by the drawing helpers. */
struct DisplayModeInfo {
    short render_mode;
    int transform_term_a[4];
    int transform_term_b[4];
    int transform_term_c[4];
    unsigned char reserved_356[4];
    int draw_parameter;
    int reserved_35e;
    int reserved_362;
    int reserved_366;
    int reserved_36a;
    int viewport_left;
    int viewport_top;
    int viewport_width;
    int viewport_height;
    unsigned char reserved_37e[9];
    unsigned char tail;
};
extern struct DisplayModeInfo g_e324;

struct DecodedImage {
    unsigned char *pixels;
    unsigned char *palette;
    int width;
    int height;
    int pixel_format;
    int palette_entries;
    int allocation_size;
    int palette_size;
    int pixel_size;
};

extern void copy_chunky_scanline_to_vga(int, int, int);
extern void f_13889(int, int, int);
extern short g_7b14;
extern short g_7b16;
extern void write_vga_pixel_entry(int, int, int);
extern void f_ec76(void *);

void plot_transformed_pixel(int pixel, int selector)
{
    if (g_e324.render_mode == 1) {
        copy_chunky_scanline_to_vga(pixel,
                (g_e324.transform_term_a[selector] +
                 g_e324.transform_term_b[(unsigned)selector] +
                 g_e324.transform_term_c[selector]) >> 2,
                g_e324.draw_parameter);
    } else {
        f_13889(pixel,
                g_e324.transform_term_a[selector] +
                g_e324.transform_term_b[(unsigned)selector] +
                g_e324.transform_term_c[selector],
                g_e324.draw_parameter);
    }
}

void blit_indexed_image(unsigned char *image_bytes)
{
    int x;
    int y;
    int saved_palette;
    int pixel_address;

    saved_palette = g_7b16;
    g_7b16 = g_7b14;
    f_ec76((void *)*(int *)(image_bytes + 4));
    pixel_address = *(int *)image_bytes;
    for (y = 0; y < *(int *)(image_bytes + 0xc); y++) {
        for (x = 0; x < *(int *)(image_bytes + 8); x++) {
            write_vga_pixel_entry(x, y, *(unsigned char *)(pixel_address++));
        }
    }
    g_7b16 = (unsigned short)saved_palette;
}
