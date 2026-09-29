/* main_android.c - ke_android_main(): the Android host (the SDL thread of the activity).
 *
 * Called by the loader (libmain.so, loader.c) once this library sits below 2 GB. It sets up
 * the ILP32 game world (heap arena, game stack, 32-bit data relocations), the paths under
 * the app's internal storage, the virtual PC and SDL, then runs the historical main() on the
 * game thread exactly like port/host/main_sdl.c does on Windows. Android specifics: touch
 * controls and overlay (touch.c), immersive landscape full screen, and pausing the emulated
 * machine while the app is in the background (docs/android/architecture.md, "Lifecycle").
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "../include/ke_port.h"
#include "../include/viewport.h"
#include "../platform/ke_platform.h"
#include "../vhw/vhw.h"
#include "ke_android.h"

void ke_input_event(const SDL_Event *e);
extern void (*ke_present_overlay)(SDL_Renderer *renderer, const KeViewport *viewport,
                                  int output_w, int output_h);

static SDL_AtomicInt background;

/* Called synchronously on the Java UI thread for the lifecycle events: pause the machine
 * before Android stops the activity, resume it when it is back in front. */
static bool SDLCALL lifecycle_watch(void *userdata, SDL_Event *e)
{
    (void)userdata;
    switch (e->type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        if (SDL_SetAtomicInt(&background, 1) == 0) {
            ke_time_pause();
            ke_pause_game();
            ke_log(KE_LOG_INFO, "android", "entering background: emulation paused");
        }
        break;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        if (SDL_SetAtomicInt(&background, 0) == 1) {
            ke_resume_game();
            ke_time_resume();
            ke_log(KE_LOG_INFO, "android", "foreground: emulation resumed");
        }
        break;
    case SDL_EVENT_TERMINATING:
        ke_request_quit();
        ke_resume_game();
        break;
    default:
        break;
    }
    return true;
}

static void show_error(const char *title, const char *message)
{
    ke_log(KE_LOG_ERROR, "android", "%s: %s", title, message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}

__attribute__((visibility("default"))) int ke_android_main(int argc, char *argv[],
                                                           const KeLowRegions *regions)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    char base[KE_MAX_PATH], log_path[KE_MAX_PATH], missing[1024];
    const char *internal;
    int result = 2, vhw_ready = 0, game_started = 0;

    /* ---- the low regions and the ILP32 world ---------------------------------------- */
    ke32_set_heap(regions->heap, regions->heap_size);
    ke_platform_set_game_stack(regions->stack, regions->stack_size);

    internal = SDL_GetAndroidInternalStoragePath();
    if (!internal)
        internal = ".";
    ke_platform_set_user_base(internal);
    ke_platform_set_exe_dir(internal);
    snprintf(base, sizeof base, "%s/Krypton Egg", internal);
    ke_platform_mkdir(base);
    snprintf(log_path, sizeof log_path, "%s/ke_sdl3.log", base);
    ke_config.log_level = KE_LOG_INFO;
    ke_log_init(log_path);
    ke_log(KE_LOG_INFO, "android", "Krypton Egg (historical source + virtual PC), %s, image %p",
#if defined(__aarch64__)
           "arm64-v8a",
#else
           "x86_64",
#endif
           (void *)regions->image);
    if (ke32_apply_relocs() != 0) {
        show_error("Krypton Egg", "The game image is not below 2 GB.");
        return 2;
    }

    ke_config_load(argc, argv);   /* argv[1]: the imported data directory (Java) */
    if (!ke_config_validate_assets(missing, sizeof missing)) {
        show_error("Krypton Egg - missing game data", missing);
        return 2;
    }
    if (ke_platform_chdir(ke_config.data_dir) != 0) {
        show_error("Krypton Egg", "Cannot enter the game data directory.");
        return 2;
    }
    ke_log(KE_LOG_INFO, "android", "assets: %s; audio=%d volume=%d%%", ke_config.data_dir,
           ke_config.sound_blaster, ke_config_volume());

    /* ---- virtual PC + SDL ------------------------------------------------------------- */
    if (vhw_init() != 0) {
        show_error("Krypton Egg", "Cannot map the DOS memory of the virtual PC.");
        return 2;
    }
    vhw_ready = 1;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | (ke_config.sound_blaster ? SDL_INIT_AUDIO : 0) |
                  (ke_config.joystick ? SDL_INIT_GAMEPAD : 0))) {
        show_error("SDL_Init", SDL_GetError());
        goto cleanup;
    }
    SDL_AddEventWatch(lifecycle_watch, NULL);
    if (!SDL_CreateWindowAndRenderer("Krypton Egg", 0, 0,
                                     SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window, &renderer)) {
        show_error("SDL window", SDL_GetError());
        goto cleanup;
    }
    SDL_SetRenderVSync(renderer, 1);
    if (ke_present_init(window, renderer) != 0) {
        show_error("SDL texture", SDL_GetError());
        goto cleanup;
    }
    ke_touch_init(window);
    ke_present_overlay = ke_touch_draw;
    vsb_init();

    if (ke_game_thread_start() != 0) {
        show_error("Krypton Egg", "Cannot start the game thread.");
        goto cleanup;
    }
    game_started = 1;
    while (!ke_game_thread_finished()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_TERMINATING) {
                ke_request_quit();
                ke_resume_game();
            } else if (!ke_touch_event(&e)) {
                ke_input_event(&e);
            }
        }
        ke_touch_update();
        if (SDL_GetAtomicInt(&background)) {
            SDL_Delay(20);
            continue;
        }
        ke_present_frame();       /* vsync paces the SDL thread at the display rate */
    }
    result = ke_game_exit_code();
    ke_stub_report();
    ke_log(KE_LOG_INFO, "android", "game finished (exit code %d)", result);

cleanup:
    if (game_started) {
        KeThread *thread = ke_game_thread_handle();
        ke_request_quit();
        ke_resume_game();
        if (thread && !ke_thread_join_ms(thread, 3000))
            ke_log(KE_LOG_WARN, "android", "game thread did not stop within 3 s");
    }
    if (vhw_ready)
        vhw_shutdown();
    ke_present_overlay = NULL;
    ke_present_shutdown();
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
    /* The activity finishes when SDL_main returns. Static state (the game's globals) cannot
     * be reinitialised in this process, so leave it for a fresh start. */
    ke_log(KE_LOG_INFO, "android", "exit");
    exit(result);
}
