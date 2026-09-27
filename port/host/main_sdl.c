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
    SDL_Window *window;
    SDL_Renderer *renderer;
    char log_path[MAX_PATH];
    const char *exit_after = getenv("KE_EXIT_AFTER_MS");
    uint64_t exit_deadline = 0, close_requested_at = 0;
    int running = 1;
    int child_exit;
    uint64_t start_ns, shot_ns = 0;
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
        return 2;

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        ke_log(KE_LOG_ERROR, "main", "SDL_Init: %s", SDL_GetError());
        return 2;
    }
    if (!SDL_CreateWindowAndRenderer("Krypton Egg", 320 * ke_config.scale, 240 * ke_config.scale,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        ke_log(KE_LOG_ERROR, "main", "SDL window: %s", SDL_GetError());
        return 2;
    }
    SDL_SetRenderVSync(renderer, 1);
    if (ke_present_init(window, renderer) != 0) {
        ke_log(KE_LOG_ERROR, "main", "texture: %s", SDL_GetError());
        return 2;
    }
    vsb_init(); /* opens its SDL audio stream, so after SDL_Init */
    if (exit_after)
        exit_deadline = ke_now_ns() + (uint64_t)atoi(exit_after) * 1000000ull;

    start_ns = ke_now_ns();
    if (ke_game_thread_start() != 0) {
        ke_log(KE_LOG_ERROR, "main", "cannot start the game thread");
        return 2;
    }
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                if (!close_requested_at)
                    ke_log_game_backtrace("at quit request");
                ke_request_quit();
                if (!close_requested_at)
                    close_requested_at = ke_now_ns();
            } else {
                ke_input_event(&e);
            }
        }
        if (exit_deadline && ke_now_ns() > exit_deadline && !close_requested_at) {
            ke_log(KE_LOG_INFO, "main", "KE_EXIT_AFTER_MS elapsed: requesting quit");
            ke_log_game_backtrace("at quit request");
            ke_request_quit();
            close_requested_at = ke_now_ns();
        }
        if (ke_game_thread_finished())
            running = 0;
        else if (close_requested_at && ke_now_ns() - close_requested_at > 3000000000ull) {
            ke_log(KE_LOG_WARN, "main", "game thread did not reach a service boundary within 3 s; "
                   "terminating it");
            TerminateThread((HANDLE)ke_game_thread_handle(), 4);
            running = 0;
        }
        run_auto_keys(ke_now_ns() - start_ns);
        ke_present_frame();
        if (shot_ns && ke_now_ns() - start_ns >= shot_ns) {
            ke_log(KE_LOG_INFO, "main", "screenshot %s: %s", shot_path,
                   ke_present_save_bmp(shot_path) == 0 ? "saved" : "no graphics frame");
            shot_ns = 0;
        }
    }
    ke_stub_report();
    vhw_shutdown();
    ke_present_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    ke_log(KE_LOG_INFO, "main", "exit code %d", ke_game_exit_code());
    return ke_game_exit_code();
}
