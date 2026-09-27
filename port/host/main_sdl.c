/* main_sdl.c - process entry: SDL3 window + event pump on the main thread, the historical
 * game on the game thread (gamethread.c), the virtual PC underneath (port/vhw).
 *
 *   ke_sdl3.exe [DATA_DIR]        DATA_DIR (or KE_DATA) holds KE.EXE's data files; default
 *                                 is the current directory.
 * Environment: KE_IRQ, KE_WINDOWS, KE_SB, KE_JOY, KE_SCALE, KE_ASPECT, KE_LOG_LEVEL,
 * KE_LOG (log file path, default ke_sdl3.log next to the executable), KE_EXIT_AFTER_MS
 * (close automatically; for smoke tests).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <windows.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "ke_port.h"
#include "../vhw/vhw.h"

void ke_input_event(const SDL_Event *e);
int ke_present_save_bmp(const char *path);

/* Smoke-test automation: KE_AUTOKEYS="ms:scan,ms:scan" injects an XT make+break code
 * (hex) at a time after start; KE_SCREENSHOT="ms:file.bmp" saves the presented frame. */
typedef struct AutoKey { uint64_t at_ns; unsigned code; } AutoKey;
static AutoKey auto_keys[64];
static int auto_key_count, auto_key_next;

static void parse_auto_keys(void)
{
    const char *s = getenv("KE_AUTOKEYS");
    while (s && *s && auto_key_count < 64) {
        unsigned ms, code;
        if (sscanf(s, "%u:%x", &ms, &code) != 2)
            break;
        auto_keys[auto_key_count].at_ns = (uint64_t)ms * 1000000ull;
        auto_keys[auto_key_count].code = code;
        auto_key_count++;
        s = strchr(s, ',');
        if (s)
            s++;
    }
}

static void run_auto_keys(uint64_t since_start)
{
    while (auto_key_next < auto_key_count && since_start >= auto_keys[auto_key_next].at_ns) {
        unsigned code = auto_keys[auto_key_next++].code;
        ke_log(KE_LOG_INFO, "main", "KE_AUTOKEYS: scan code %02X", code);
        if (code & 0x100)
            vkbd_push_scancode(0xe0);
        vkbd_push_scancode((uint8_t)(code & 0x7f));
        if (code & 0x100)
            vkbd_push_scancode(0xe0);
        vkbd_push_scancode((uint8_t)((code & 0x7f) | 0x80));
    }
}

static void default_log_path(char *out, size_t n)
{
    char *slash;
    GetModuleFileNameA(NULL, out, (DWORD)n);
    slash = strrchr(out, '\\');
    if (slash)
        strcpy(slash + 1, "ke_sdl3.log");
    else
        snprintf(out, n, "ke_sdl3.log");
}

static void set_window_icon(SDL_Window *window)
{
    SDL_Surface *icon = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_ARGB8888);
    uint32_t clear, edge, shell, yolk, highlight;
    int x, y;
    if (!icon)
        return;
    clear = SDL_MapSurfaceRGBA(icon, 0, 0, 0, 0);
    edge = SDL_MapSurfaceRGBA(icon, 105, 62, 22, 255);
    shell = SDL_MapSurfaceRGBA(icon, 250, 226, 169, 255);
    yolk = SDL_MapSurfaceRGBA(icon, 235, 157, 42, 255);
    highlight = SDL_MapSurfaceRGBA(icon, 255, 248, 218, 255);
    if (!SDL_LockSurface(icon)) {
        SDL_DestroySurface(icon);
        return;
    }
    for (y = 0; y < 32; y++) {
        int dy = y - 16;
        int width = 11 - (dy * dy * 8) / 144 + dy / 6;
        uint32_t *row = (uint32_t *)((uint8_t *)icon->pixels + y * icon->pitch);
        for (x = 0; x < 32; x++)
            row[x] = clear;
        if (y < 3 || y > 29)
            continue;
        for (x = 16 - width; x <= 16 + width; x++) {
            int dx = x - 16;
            int yd = y - 18;
            int border = x == 16 - width || x == 16 + width || y == 3 || y == 29;
            row[x] = border ? edge : shell;
            if (!border && dx * dx + yd * yd <= 16)
                row[x] = yolk;
            if (!border && y < 12 && x < 15 && (x - 13) * (x - 13) + (y - 9) * (y - 9) <= 3)
                row[x] = highlight;
        }
    }
    SDL_UnlockSurface(icon);
    if (!SDL_SetWindowIcon(window, icon))
        ke_log(KE_LOG_WARN, "main", "window icon: %s", SDL_GetError());
    SDL_DestroySurface(icon);
}

static void request_game_quit(int *requested, uint64_t *requested_at, const char *why)
{
    if (*requested)
        return;
    *requested = 1;
    *requested_at = ke_now_ns();
    ke_request_quit();
    ke_log_game_backtrace(why);
}

static int is_fullscreen_shortcut(const SDL_Event *e)
{
    return e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat &&
           (e->key.scancode == SDL_SCANCODE_F11 ||
            (e->key.scancode == SDL_SCANCODE_RETURN && (e->key.mod & SDL_KMOD_ALT)));
}

/* The virtual PC needs linear LOWMEM_BASE..LOWMEM_END (DOS memory, BIOS page, HMA) identity
 * mapped. The Windows loader makes its own early allocations (NLS tables, heaps, TEBs) in
 * low memory, so the process relaunches itself suspended and reserves the range in the
 * child before the child's loader runs; lowmem_init() then commits the reservation. */
static int relaunch_with_low_memory_reserved(int *exit_code)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR path[MAX_PATH];
    void *reserved;
    DWORD code = 1;
    if (getenv("KE_CHILD"))
        return 0;
    SetEnvironmentVariableA("KE_CHILD", "1");
    GetModuleFileNameW(NULL, path, MAX_PATH);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessW(path, GetCommandLineW(), NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL,
                        &si, &pi))
        return 0;
    reserved = VirtualAllocEx(pi.hProcess, (void *)LOWMEM_BASE, LOWMEM_END - LOWMEM_BASE,
                              MEM_RESERVE, PAGE_READWRITE);
    if (reserved != (void *)LOWMEM_BASE) {
        fprintf(stderr, "ke_sdl3: cannot reserve low memory in the child (%lu); running in-process\n",
                GetLastError());
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        SetEnvironmentVariableA("KE_CHILD", "0");
        return 0;
    }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    *exit_code = (int)code;
    return 1;
}

int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    char log_path[MAX_PATH];
    const char *exit_after = getenv("KE_EXIT_AFTER_MS");
    uint64_t exit_deadline = 0, close_requested_at = 0;
    uint64_t start_ns = 0, shot_ns = 0;
    uint32_t last_presented_frame = 0;
    int running = 1, quit_requested = 0, have_presented_frame = 0;
    int vhw_ready = 0, sdl_ready = 0, present_ready = 0, game_started = 0;
    int game_terminated = 0, result = 2, fullscreen = 0;
    int child_exit;
    char shot_path[MAX_PATH] = "";

    if (relaunch_with_low_memory_reserved(&child_exit))
        return child_exit;

    ke_config_load(argc, argv);
    setvbuf(stdout, NULL, _IONBF, 0);   /* DOS console output is unbuffered */
    SetConsoleOutputCP(437);            /* the game's text uses CP437 box characters */
    parse_auto_keys();
    if (getenv("KE_SCREENSHOT")) {
        unsigned ms = 0;
        if (sscanf(getenv("KE_SCREENSHOT"), "%u:%259s", &ms, shot_path) == 2)
            shot_ns = (uint64_t)ms * 1000000ull;
    }
    if (getenv("KE_LOG"))
        snprintf(log_path, sizeof log_path, "%s", getenv("KE_LOG"));
    else
        default_log_path(log_path, sizeof log_path);
    ke_log_init(log_path);
    ke_log(KE_LOG_INFO, "main", "Krypton Egg SDL3 port (historical source + virtual PC)");
    if (ke_config.data_dir && _chdir(ke_config.data_dir) != 0) {
        ke_log(KE_LOG_ERROR, "main", "cannot enter data directory %s", ke_config.data_dir);
        return 2;
    }
    /* Map low memory before SDL loads anything that could take the range. */
    if (vhw_init() != 0)
        goto cleanup;
    vhw_ready = 1;

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        ke_log(KE_LOG_ERROR, "main", "SDL_Init: %s", SDL_GetError());
        goto cleanup;
    }
    sdl_ready = 1;
    if (ke_config.scale < 1 || ke_config.scale > 8) {
        ke_log(KE_LOG_WARN, "main", "KE_SCALE must be 1..8; using 3");
        ke_config.scale = 3;
    }
    ke_config.aspect = !!ke_config.aspect;
    if (!SDL_CreateWindowAndRenderer("Krypton Egg (F11 fullscreen)", 320 * ke_config.scale,
                                     (ke_config.aspect ? 240 : 200) * ke_config.scale,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        ke_log(KE_LOG_ERROR, "main", "SDL window: %s", SDL_GetError());
        goto cleanup;
    }
    set_window_icon(window);
    if (!SDL_SetRenderVSync(renderer, 0))
        ke_log(KE_LOG_WARN, "main", "cannot disable renderer vsync: %s", SDL_GetError());
    if (ke_present_init(window, renderer) != 0) {
        ke_log(KE_LOG_ERROR, "main", "texture: %s", SDL_GetError());
        goto cleanup;
    }
    present_ready = 1;
    vsb_init(); /* opens its SDL audio stream, so after SDL_Init */
    if (exit_after)
        exit_deadline = ke_now_ns() + (uint64_t)atoi(exit_after) * 1000000ull;

    start_ns = ke_now_ns();
    if (ke_game_thread_start() != 0) {
        ke_log(KE_LOG_ERROR, "main", "cannot start the game thread");
        goto cleanup;
    }
    game_started = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                request_game_quit(&quit_requested, &close_requested_at, "at quit request");
            } else if (is_fullscreen_shortcut(&e)) {
                fullscreen = !fullscreen;
                if (!SDL_SetWindowFullscreen(window, fullscreen)) {
                    fullscreen = !fullscreen;
                    ke_log(KE_LOG_WARN, "main", "fullscreen toggle: %s", SDL_GetError());
                }
            } else {
                ke_input_event(&e);
            }
        }
        if (exit_deadline && ke_now_ns() > exit_deadline && !quit_requested) {
            ke_log(KE_LOG_INFO, "main", "KE_EXIT_AFTER_MS elapsed: requesting quit");
            request_game_quit(&quit_requested, &close_requested_at, "at quit request");
        }
        if (ke_game_thread_finished())
            running = 0;
        else if (quit_requested && ke_now_ns() - close_requested_at >= 100000000ull) {
            ke_log(KE_LOG_WARN, "main", "game thread did not reach a service boundary within 100 ms; "
                   "terminating it without game atexit handlers");
            TerminateThread((HANDLE)ke_game_thread_handle(), 4);
            WaitForSingleObject((HANDLE)ke_game_thread_handle(), INFINITE);
            game_terminated = 1;
            result = 4;
            running = 0;
        }
        run_auto_keys(ke_now_ns() - start_ns);
        {
            uint32_t frame = vga_frame_counter();
            if (!have_presented_frame || frame != last_presented_frame) {
                ke_present_frame();
                last_presented_frame = frame;
                have_presented_frame = 1;
                if (shot_ns && ke_now_ns() - start_ns >= shot_ns) {
                    ke_log(KE_LOG_INFO, "main", "screenshot %s: %s", shot_path,
                           ke_present_save_bmp(shot_path) == 0 ? "saved" : "no graphics frame");
                    shot_ns = 0;
                }
            } else {
                SDL_Delay(1);
            }
        }
    }
    ke_stub_report();
    if (!game_terminated)
        result = ke_game_exit_code();
    ke_log(KE_LOG_INFO, "main", "exit code %d", result);

cleanup:
    if (game_started) {
        HANDLE thread = (HANDLE)ke_game_thread_handle();
        if (thread) {
            if (!game_terminated && !ke_game_thread_finished()) {
                ke_request_quit();
                if (WaitForSingleObject(thread, 100) == WAIT_TIMEOUT) {
                    TerminateThread(thread, 4);
                    WaitForSingleObject(thread, INFINITE);
                    game_terminated = 1;
                }
            } else {
                WaitForSingleObject(thread, INFINITE);
            }
            CloseHandle(thread);
        }
    }
    if (vhw_ready)
        vhw_shutdown();
    if (present_ready)
        ke_present_shutdown();
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    if (sdl_ready)
        SDL_Quit();
    return result;
}
