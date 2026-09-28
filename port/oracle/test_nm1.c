/* NM1 tests: persistent INI defaults/precedence and shared viewport mapping. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle_test.h"
#include "../include/ke_port.h"
#include "../include/viewport.h"
#include "../vhw/vhw.h"

typedef struct SavedEnvironment {
    const char *name;
    char value[1024];
    int was_set;
} SavedEnvironment;

static const char *const config_env_names[] = {
    "KE_CONFIG_DIR", "KE_MOUSE_MODE", "KE_SCALE", "KE_ASPECT", "KE_LOG_LEVEL",
    "KE_FULLSCREEN", "KE_INTEGER_SCALE", "KE_VOLUME", "KE_SB", "KE_JOY",
    "KE_IRQ", "KE_WINDOWS", "KE_AUDIO", "KE_DATA"
};

static int set_environment(const char *name, const char *value)
{
    char assignment[1200];
    snprintf(assignment, sizeof assignment, "%s=%s", name, value ? value : "");
    return _putenv(assignment) == 0;
}

static void save_and_clear_environment(SavedEnvironment *saved, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        const char *value = getenv(saved[i].name);
        saved[i].was_set = value != NULL;
        if (value)
            snprintf(saved[i].value, sizeof saved[i].value, "%s", value);
        else
            saved[i].value[0] = '\0';
        set_environment(saved[i].name, NULL);
    }
}

static void restore_environment(const SavedEnvironment *saved, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i)
        set_environment(saved[i].name, saved[i].was_set ? saved[i].value : NULL);
}

static int file_contains(const char *path, const char *needle)
{
    char contents[8192];
    size_t n;
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;
    n = fread(contents, 1, sizeof contents - 1, file);
    contents[n] = '\0';
    fclose(file);
    return strstr(contents, needle) != NULL;
}

static int write_config(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");
    int ok;
    if (!file)
        return 0;
    ok = fwrite(contents, 1, strlen(contents), file) == strlen(contents);
    return fclose(file) == 0 && ok;
}

static int test_config_persistence(void)
{
    SavedEnvironment saved[sizeof config_env_names / sizeof config_env_names[0]];
    char temp[MAX_PATH], directory[MAX_PATH * 2], config_path[MAX_PATH * 3];
    char *args[] = {"ke_config_test.exe", NULL};
    DWORD n;
    size_t i;
    int failures = 0;
    n = GetTempPathA((DWORD)sizeof temp, temp);
    if (!n || n >= sizeof temp) {
        printf("    cannot resolve temporary directory for config test\n");
        return 1;
    }
    snprintf(directory, sizeof directory, "%snm1-config-%lu", temp,
             (unsigned long)GetCurrentProcessId());
    snprintf(config_path, sizeof config_path, "%s\\krypton-egg.ini", directory);
    RemoveDirectoryA(directory);
    if (!CreateDirectoryA(directory, NULL)) {
        printf("    cannot create temporary config directory (%lu)\n",
               (unsigned long)GetLastError());
        return 1;
    }
    for (i = 0; i < sizeof saved / sizeof saved[0]; ++i)
        saved[i].name = config_env_names[i];
    save_and_clear_environment(saved, sizeof saved / sizeof saved[0]);
    set_environment("KE_CONFIG_DIR", directory);

    ke_config_load(1, args);
    if (GetFileAttributesA(config_path) == INVALID_FILE_ATTRIBUTES ||
        !file_contains(config_path, "[video]") || !file_contains(config_path, "fullscreen = false") ||
        !file_contains(config_path, "scale = 3") || !file_contains(config_path, "aspect = true") ||
        !file_contains(config_path, "integer_scale = true") || !file_contains(config_path, "[audio]") ||
        !file_contains(config_path, "sound_blaster = true") || !file_contains(config_path, "volume = 100") ||
        !file_contains(config_path, "[input]") || !file_contains(config_path, "joystick = false") ||
        !file_contains(config_path, "mouse_mode = faithful") ||
        !file_contains(config_path, "mouse_sensitivity = 1.0") ||
        !file_contains(config_path, "[paths]") || !file_contains(config_path, "asset_dir =") ||
        !file_contains(config_path, "[system]") || !file_contains(config_path, "irq = async") ||
        !file_contains(config_path, "windows_host = false") || !file_contains(config_path, "[debug]") ||
        !file_contains(config_path, "log_level = 2") || ke_config.scale != 3 ||
        ke_config.aspect != 1 || ke_config.mouse_native || ke_config.joystick ||
        !ke_config.sound_blaster || ke_config_fullscreen() || !ke_config_integer_scale() ||
        ke_config_volume() != 100 || ke_config.log_level != KE_LOG_INFO || !ke_config.irq_async ||
        ke_config.windows_host) {
        printf("    first-run config creation/defaults mismatch\n");
        failures++;
    }

    if (!write_config(config_path,
                      "[video]\nscale = 5\nfullscreen = true\n"
                      "[audio]\nsound_blaster = false\nvolume = 35\n"
                      "[input]\njoystick = true\nmouse_mode = native\n"
                      "[system]\nirq = sync\nwindows_host = true\n[debug]\nlog_level = 4\n")) {
        printf("    could not write config load fixture\n");
        failures++;
    } else {
        ke_config_load(1, args);
        if (ke_config.scale != 5 || !ke_config.mouse_native || !ke_config_fullscreen() ||
            ke_config.sound_blaster || ke_config_volume() != 35 || !ke_config.joystick ||
            ke_config.irq_async || !ke_config.windows_host || ke_config.log_level != KE_LOG_TRACE) {
            printf("    config loading mismatch\n");
            failures++;
        }
    }

    set_environment("KE_SCALE", "7");
    set_environment("KE_MOUSE_MODE", "faithful");
    ke_config_load(1, args);
    if (ke_config.scale != 7 || ke_config.mouse_native) {
        printf("    environment did not override config file\n");
        failures++;
    }

    {
        char *cli_args[] = {"ke_config_test.exe", "--scale", "2", "--mouse-mode", "native", NULL};
        ke_config_load(5, cli_args);
        if (ke_config.scale != 2 || !ke_config.mouse_native) {
            printf("    command-line did not override environment and config\n");
            failures++;
        }
    }

    set_environment("KE_SCALE", NULL);
    set_environment("KE_MOUSE_MODE", NULL);
    if (!write_config(config_path, "[video]\nscale = -99\n[input]\nmouse_mode = banana\n")) {
        printf("    could not write invalid config fixture\n");
        failures++;
    } else {
        ke_config_load(1, args);
        if (ke_config.scale != 3 || ke_config.mouse_native) {
            printf("    invalid config values did not fall back safely\n");
            failures++;
        }
    }

    restore_environment(saved, sizeof saved / sizeof saved[0]);
    DeleteFileA(config_path);
    RemoveDirectoryA(directory);
    return failures;
}

static int map_point(const KeViewport *viewport, int window_w, int window_h,
                     float x, float y, int expected_x, int expected_y)
{
    int game_x = -1, game_y = -1;
    if (!ke_viewport_map_window_point(viewport, window_w, window_h, x, y, &game_x, &game_y) ||
        game_x != expected_x || game_y != expected_y) {
        printf("    viewport map %.2f,%.2f -> %d,%d; expected %d,%d\n",
               x, y, game_x, game_y, expected_x, expected_y);
        return 1;
    }
    return 0;
}

static int test_viewport_mapping(void)
{
    KeViewport viewport;
    int failures = 0;
    if (!ke_viewport_calculate(&viewport, 320, 200, 960, 720, 1, 1))
        return 1;
    if (viewport.x != 0.0f || viewport.y != 0.0f || viewport.w != 960.0f ||
        viewport.h != 720.0f) {
        printf("    960x720 4:3 viewport dimensions mismatch\n");
        failures++;
    }
    failures += map_point(&viewport, 960, 720, 0, 0, 0, 0);
    failures += map_point(&viewport, 960, 720, 960, 0, 319, 0);
    failures += map_point(&viewport, 960, 720, 0, 720, 0, 199);
    failures += map_point(&viewport, 960, 720, 960, 720, 319, 199);
    failures += map_point(&viewport, 960, 720, 480, 360, 160, 100);

    if (!ke_viewport_calculate(&viewport, 320, 200, 1920, 1080, 1, 0))
        return failures + 1;
    if (viewport.x != 240.0f || viewport.y != 0.0f || viewport.w != 1440.0f ||
        viewport.h != 1080.0f) {
        printf("    widescreen pillarbox destination mismatch\n");
        failures++;
    }
    failures += map_point(&viewport, 1920, 1080, 0, 540, 0, 100);
    failures += map_point(&viewport, 1920, 1080, 240, 540, 0, 100);
    failures += map_point(&viewport, 1920, 1080, 1680, 540, 319, 100);
    failures += map_point(&viewport, 1920, 1080, 1920, 540, 319, 100);

    if (!ke_viewport_calculate(&viewport, 320, 200, 1280, 720, 1, 1))
        return failures + 1;
    if (viewport.w != 960.0f || viewport.h != 720.0f || viewport.x != 160.0f)
        failures++;
    failures += map_point(&viewport, 1280, 720, 640, 360, 160, 100);

    if (!ke_viewport_calculate(&viewport, 320, 200, 1280, 800, 1, 0))
        return failures + 1;
    failures += map_point(&viewport, 1280, 800, 640, 400, 160, 100);

    /* A 2x high-DPI output maps from SDL's half-size window-point coordinates. */
    if (!ke_viewport_calculate(&viewport, 320, 200, 1280, 960, 1, 1))
        return failures + 1;
    failures += map_point(&viewport, 640, 480, 320, 240, 160, 100);
    failures += map_point(&viewport, 640, 480, -10, 240, 0, 100);
    failures += map_point(&viewport, 640, 480, 650, 240, 319, 100);
    failures += map_point(&viewport, 640, 480, 320, -10, 160, 0);
    failures += map_point(&viewport, 640, 480, 320, 490, 160, 199);

    if (!ke_viewport_calculate(&viewport, 320, 400, 1920, 1440, 1, 1))
        return failures + 1;
    failures += map_point(&viewport, 1920, 1440, 960, 720, 160, 200);
    if (ke_viewport_calculate(&viewport, 0, 200, 960, 720, 1, 0))
        failures++;
    return failures;
}

static int test_absolute_mouse_position(void)
{
    union REGS regs;
    struct SREGS sregs;
    memset(&regs, 0, sizeof regs);
    memset(&sregs, 0, sizeof sregs);
    vmouse_init();
    regs.w.ax = 0;
    vmouse_int33(&regs, &sregs);
    ke_native_mouse_set_game_position(159, 98);
    regs.w.ax = 3;
    vmouse_int33(&regs, &sregs);
    if (regs.w.cx != 318 || regs.w.dx != 196) {
        printf("    direct mouse position was not visible through INT 33h function 03\n");
        return 1;
    }
    regs.w.ax = 0x0b;
    vmouse_int33(&regs, &sregs);
    if (regs.w.cx || regs.w.dx) {
        printf("    direct mouse position changed INT 33h mickey counters\n");
        return 1;
    }
    return 0;
}

void register_nm1_tests(void)
{
    oracle_register("NM1 config creation, loading, overrides and invalid values", test_config_persistence);
    oracle_register("NM1 shared viewport mapping", test_viewport_mapping);
    oracle_register("NM1 absolute position bypasses mickey counters", test_absolute_mouse_position);
}
