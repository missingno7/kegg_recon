/* present.c - emulated VGA scan-out -> SDL3 texture.
 *
 * Each host frame: scan out the CRTC-visible frame as 8-bit indices (vga_scanout), expand
 * through the DAC palette, upload to a streaming texture and draw it into the largest
 * integer multiple of the frame that fits; with KE_ASPECT=1 (default) the frame is shown
 * with the 4:3 aspect of a VGA monitor (320x200 -> 4:3 non-square pixels, nearest filter
 * vertically non-integer), with KE_ASPECT=0 pixels are square.
 * Text mode is not rendered (the game prints its startup report to the console).
 *
 * WORK PACKAGE "present": frame pacing vs virtual retrace, latched start address, text mode.
 */
#include <string.h>
#include <SDL3/SDL.h>
#include "ke_port.h"
#include "../vhw/vhw.h"

#define MAX_W 400
#define MAX_H 480

static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint8_t indexed[MAX_W * MAX_H];
static uint32_t argb[MAX_W * MAX_H];

int ke_present_init(SDL_Window *window, SDL_Renderer *r)
{
    (void)window;
    renderer = r;
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                MAX_W, MAX_H);
    if (!texture)
        return -1;
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return 0;
}

void ke_present_frame(void)
{
    int w = 0, h = 0, out_w, out_h, x, y;
    float scale, scale_x, scale_y;
    uint8_t pal[256][3];
    uint32_t lut[256];
    SDL_FRect src, dst;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (vga_scanout(indexed, MAX_W, MAX_W, MAX_H, &w, &h)) {
        vga_palette_rgb888(pal);
        /* Match DOSBox-X's 6-bit DAC screenshot conversion: shift without bit replication. */
        for (x = 0; x < 256; x++) {
            pal[x][0] &= 0xfcu;
            pal[x][1] &= 0xfcu;
            pal[x][2] &= 0xfcu;
        }
        for (x = 0; x < 256; x++)
            lut[x] = 0xff000000u | ((uint32_t)pal[x][0] << 16) | ((uint32_t)pal[x][1] << 8) | pal[x][2];
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                argb[y * MAX_W + x] = lut[indexed[y * MAX_W + x]];
        SDL_UpdateTexture(texture, NULL, argb, MAX_W * 4);
        SDL_GetCurrentRenderOutputSize(renderer, &out_w, &out_h);
        src.x = 0; src.y = 0; src.w = (float)w; src.h = (float)h;
        if (ke_config.aspect) {
            /* Display at 4:3; optional whole-number scale preserves source pixels. */
            scale_x = (float)out_w / (float)w;
            scale_y = (float)out_h / ((float)w * 3.0f / 4.0f);
        } else {
            scale_x = (float)out_w / (float)w;
            scale_y = (float)out_h / (float)h;
        }
        scale = scale_x < scale_y ? scale_x : scale_y;
        if (ke_config_integer_scale())
            scale = (float)(int)scale;
        if (scale < 1.0f)
            scale = 1.0f;
        dst.w = (float)w * scale;
        dst.h = ke_config.aspect ? dst.w * 3.0f / 4.0f : (float)h * scale;
        dst.x = ((float)out_w - dst.w) / 2;
        dst.y = ((float)out_h - dst.h) / 2;
        SDL_RenderTexture(renderer, texture, &src, &dst);
    }
    SDL_RenderPresent(renderer);
}

void ke_present_shutdown(void)
{
    if (texture)
        SDL_DestroyTexture(texture);
    texture = NULL;
}

/* Save the current scan-out (indexed + DAC palette) as an 8-bit BMP. */
int ke_present_save_bmp(const char *path)
{
    int w = 0, h = 0, i, ok;
    uint8_t pal[256][3];
    SDL_Color colors[256];
    SDL_Surface *surface;
    SDL_Palette *palette;
    if (!vga_scanout(indexed, MAX_W, MAX_W, MAX_H, &w, &h))
        return -1;
    vga_palette_rgb888(pal);
    /* The indexed BMP represents DOSBox-X's DAC truncation, like the displayed texture. */
    for (i = 0; i < 256; i++) {
        pal[i][0] &= 0xfcu;
        pal[i][1] &= 0xfcu;
        pal[i][2] &= 0xfcu;
    }
    surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_INDEX8, indexed, MAX_W);
    if (!surface)
        return -1;
    palette = SDL_CreateSurfacePalette(surface);
    for (i = 0; i < 256; i++) {
        colors[i].r = pal[i][0]; colors[i].g = pal[i][1]; colors[i].b = pal[i][2]; colors[i].a = 255;
    }
    SDL_SetPaletteColors(palette, colors, 0, 256);
    ok = SDL_SaveBMP(surface, path) ? 0 : -1;
    SDL_DestroySurface(surface);
    return ok;
}
