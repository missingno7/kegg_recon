/* ke_android.h - interfaces of the Android host (port/android). */
#ifndef KE_ANDROID_H
#define KE_ANDROID_H

#include <stddef.h>
#include <stdint.h>

/* Low address-space regions the loader (libmain.so) prepared below 2 GB. */
typedef struct KeLowRegions {
    uint8_t *image;       /* libkegame.so was loaded at the start of this reservation */
    size_t image_size;
    uint8_t *heap;        /* read/write: the game heap arena (ke32_malloc)            */
    size_t heap_size;
    uint8_t *stack;       /* read/write: the game thread's stack                      */
    size_t stack_size;
} KeLowRegions;

typedef int (*KeAndroidMain)(int argc, char *argv[], const KeLowRegions *regions);

#if !defined(KE_ANDROID_LOADER)
struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;
struct KeViewport;

/* port/android/ilp32/ke32_shims.c */
void ke32_set_heap(void *base, size_t size);
int ke32_apply_relocs(void);

/* port/android/touch.c: touch, overlay controls, Android back, soft keyboard */
void ke_touch_init(struct SDL_Window *window);
int ke_touch_event(const union SDL_Event *e);   /* 1 = consumed */
void ke_touch_update(void);                      /* main loop: paced keys, held taps  */
void ke_touch_draw(struct SDL_Renderer *renderer, const struct KeViewport *viewport,
                   int output_w, int output_h);
int ke_touch_gameplay(void);
#endif

#endif
