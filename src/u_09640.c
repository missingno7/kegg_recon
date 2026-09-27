struct PaletteGradient { short first_index, last_index; unsigned char start_red, start_green, start_blue, end_red, end_green, end_blue; };
#define PALETTE_GRADIENT_END (-1)
#define VGA_PALETTE_LAST_INDEX 255
extern void set_vga_palette_rgb(unsigned char, unsigned char, unsigned char, unsigned char);

/* Interpolate each RGB ramp into the VGA palette or a packed color buffer. */
void apply_palette_gradients(struct PaletteGradient *gradients, unsigned char *rgb_output)
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    int red_delta;
    int green_delta;
    int blue_delta;
    int palette_index;
    int index_step;
    int interpolation_step;
    int interval_count;
    while (gradients->first_index != PALETTE_GRADIENT_END) {
        if (gradients->first_index < 0) break;
        if (gradients->first_index > VGA_PALETTE_LAST_INDEX) break;
        if (gradients->last_index < 0) break;
        if (gradients->last_index > VGA_PALETTE_LAST_INDEX) break;
        if (rgb_output == 0)
            set_vga_palette_rgb(gradients->first_index, gradients->start_red, gradients->start_green, gradients->start_blue);
        else {
            *rgb_output++ = gradients->start_red;
            *rgb_output++ = gradients->start_green;
            *rgb_output++ = gradients->start_blue;
        }
        red_delta = gradients->end_red - gradients->start_red;
        green_delta = gradients->end_green - gradients->start_green;
        blue_delta = gradients->end_blue - gradients->start_blue;
        interval_count = gradients->last_index - gradients->first_index;
        index_step = 1;
        if (interval_count < 0) {
            interval_count = -interval_count;
            index_step = -1;
        }
        interval_count++;
        palette_index = gradients->first_index;
        for (interpolation_step = 1; interpolation_step < interval_count; interpolation_step++) {
            red = red_delta * interpolation_step / interval_count + gradients->start_red;
            green = green_delta * interpolation_step / interval_count + gradients->start_green;
            blue = blue_delta * interpolation_step / interval_count + gradients->start_blue;
            if (rgb_output == 0) {
                palette_index += index_step;
                set_vga_palette_rgb(palette_index, red, green, blue);
            } else {
                *rgb_output++ = red;
                *rgb_output++ = green;
                *rgb_output++ = blue;
            }
        }
        gradients++;
    }
}
