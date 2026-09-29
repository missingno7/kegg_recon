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
#include "../include/viewport.h"
#include "../vhw/vhw.h"

#define MAX_W 400
#define MAX_H 480

static SDL_Renderer *renderer;
static SDL_Window *present_window;
static SDL_Texture *texture;
static uint8_t indexed[MAX_W * MAX_H];
static uint32_t argb[MAX_W * MAX_H];
static int game_w, game_h;
static KeViewport last_viewport;
static int last_viewport_valid;
/* Host overlay (Android touch controls): drawn over the frame, never into the VGA image. */
void (*ke_present_overlay)(SDL_Renderer *renderer, const KeViewport *viewport, int output_w,
                           int output_h);

int ke_present_init(SDL_Window *window, SDL_Renderer *r)
{
    present_window = window;
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
    KeViewport viewport;
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
        game_w = w;
        game_h = h;
        src.x = 0; src.y = 0; src.w = (float)w; src.h = (float)h;
        if (ke_viewport_calculate(&viewport, w, h, out_w, out_h, ke_config.aspect,
                                  ke_config_integer_scale())) {
            dst.x = viewport.x; dst.y = viewport.y;
            dst.w = viewport.w; dst.h = viewport.h;
            SDL_RenderTexture(renderer, texture, &src, &dst);
            last_viewport = viewport;
            last_viewport_valid = 1;
        }
    }
    if (ke_present_overlay) {
        int ow = 0, oh = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &ow, &oh);
        ke_present_overlay(renderer, last_viewport_valid ? &last_viewport : NULL, ow, oh);
    }
    SDL_RenderPresent(renderer);
}

/* The destination of the last presented frame and its scan-out size (0 before the first). */
int ke_present_last_viewport(KeViewport *out)
{
    if (!last_viewport_valid)
        return 0;
    *out = last_viewport;
    return 1;
}

int ke_present_map_mouse(float window_x, float window_y, int *game_x, int *game_y)
{
    KeViewport viewport;
    int window_w, window_h, output_w, output_h;
    if (!present_window || !renderer || game_w <= 0 || game_h <= 0)
        return 0;
    if (!SDL_GetWindowSize(present_window, &window_w, &window_h) ||
        !SDL_GetCurrentRenderOutputSize(renderer, &output_w, &output_h))
        return 0;
    if (!ke_viewport_calculate(&viewport, game_w, game_h, output_w, output_h,
                               ke_config.aspect, ke_config_integer_scale()))
        return 0;
    return ke_viewport_map_window_point(&viewport, window_w, window_h,
                                        window_x, window_y, game_x, game_y);
}

void ke_present_shutdown(void)
{
    if (texture)
        SDL_DestroyTexture(texture);
    texture = NULL;
    present_window = NULL;
    renderer = NULL;
    game_w = game_h = 0;
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
