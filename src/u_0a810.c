/* This is the transform view of vga_state; its arrays overlay the page-address fields. */
struct VgaTransformView {
    short render_mode;
    int transform_term_a[4];
    int transform_term_b[4];
    int transform_term_c[4];
    unsigned char reserved_0x32[4];
    int draw_parameter;
    int reserved_0x36;
    int reserved_0x3a;
    int reserved_0x3e;
    int reserved_0x42;
    int viewport_left;
    int viewport_top;
    int viewport_width;
    int viewport_height;
    unsigned char reserved_0x5a[9];
    unsigned char tail;
};
extern struct VgaTransformView vga_state;

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
/* Assembly pixel-render entry used when the transform mode is not chunky. */
extern void f_13889(int, int, int);
extern short page_idx;
extern short drawpage;
extern void write_vga_pixel_entry(int, int, int);
extern void write_vga_palette(void *);

void plot_transformed_pixel(int pixel_value, int transform_index)
{
    if (vga_state.render_mode == 1) {
        copy_chunky_scanline_to_vga(pixel_value,
                (vga_state.transform_term_a[transform_index] +
                 vga_state.transform_term_b[(unsigned)transform_index] +
                 vga_state.transform_term_c[transform_index]) >> 2,
                vga_state.draw_parameter);
    } else {
        f_13889(pixel_value,
                vga_state.transform_term_a[transform_index] +
                vga_state.transform_term_b[(unsigned)transform_index] +
                vga_state.transform_term_c[transform_index],
                vga_state.draw_parameter);
    }
}

void blit_indexed_image(struct DecodedImage *image)
{
    int x;
    int y;
    int saved_draw_page;
    unsigned char *pixel_cursor;

    saved_draw_page = drawpage;
    drawpage = page_idx;
    write_vga_palette(image->palette);
    pixel_cursor = image->pixels;
    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
            write_vga_pixel_entry(x, y, *pixel_cursor++);
        }
    }
    drawpage = (unsigned short)saved_draw_page;
}
