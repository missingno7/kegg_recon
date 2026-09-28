/* config.c - command line, ke_sdl3.ini and runtime asset discovery. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "ke_port.h"

#define CONFIG_PATH_CAP MAX_PATH

KeConfig ke_config;

static int fullscreen;
static int integer_scale = 1;
static int volume = 100;
static int explicit_data_dir;
static char exe_dir[CONFIG_PATH_CAP];
static char configured_data_dir[CONFIG_PATH_CAP];
static char selected_data_dir[CONFIG_PATH_CAP];
static char user_config_path[CONFIG_PATH_CAP];

static const char *const required_assets[] = {
    "KE_ALL.PAL", "KE_BRICK.BOB", "KE_DIGIT.BOB", "KE_END.DIG", "KE_FILL.BOB",
    "KE_FONT.BOB", "KE_GO.DIG", "KE_INFOS.DIG", "KE_LDCWC.TAB", "KE_LVL.DIG",
    "KE_MAIN.DIG", "KE_MENU.BOB", "KE_MENU.DIG", "KE_MENU.GIF", "KE_MONST.BOB",
    "KE_MONST.GIF", "KE_NMY.BOB", "KE_ORDER.GIF", "KE_PAUSE.DIG", "KE_RACK.BOB", "KE_SCORE.DIG",
    "KE_SCORE.GIF", "KE_SPELL.BOB", "KE_TIT.DIG", "KE_TIT.GIF"
};

static int env_int(const char *name, int fallback)
{
    const char *value = getenv(name);
    char *end;
    long parsed;
    if (!value || !*value)
        return fallback;
    parsed = strtol(value, &end, 10);
    return end == value ? fallback : (int)parsed;
}

static int parse_bool(const char *value, int *result)
{
    if (!value)
        return 0;
    if (_stricmp(value, "1") == 0 || _stricmp(value, "true") == 0 ||
        _stricmp(value, "yes") == 0 || _stricmp(value, "on") == 0) {
        *result = 1;
        return 1;
    }
    if (_stricmp(value, "0") == 0 || _stricmp(value, "false") == 0 ||
        _stricmp(value, "no") == 0 || _stricmp(value, "off") == 0) {
        *result = 0;
        return 1;
    }
    return 0;
}

static char *trim(char *text)
{
    char *end;
    while (*text && isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return text;
}

static void lowercase(char *text)
{
    for (; *text; text++)
        *text = (char)tolower((unsigned char)*text);
}

static int path_join(char *out, size_t cap, const char *dir, const char *leaf)
{
    size_t n;
    int written;
    if (!dir || !*dir)
        return 0;
    n = strlen(dir);
    written = snprintf(out, cap, "%s%s%s", dir,
                       n && dir[n - 1] != '\\' && dir[n - 1] != '/' ? "\\" : "", leaf);
    return written >= 0 && (size_t)written < cap;
}

static int full_path(char *out, size_t cap, const char *path, const char *base)
{
    char joined[CONFIG_PATH_CAP * 2];
    DWORD n;
    const char *source = path;
    if (!path || !*path)
        return 0;
    if (!(strlen(path) > 2 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) &&
        path[0] != '\\' && path[0] != '/') {
        if (base) {
            if (!path_join(joined, sizeof joined, base, path))
                return 0;
            source = joined;
        }
    }
    n = GetFullPathNameA(source, (DWORD)cap, out, NULL);
    return n != 0 && n < cap;
}

static void find_exe_dir(void)
{
    char path[CONFIG_PATH_CAP];
    char *slash;
    DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    if (!n || n >= sizeof path) {
        GetCurrentDirectoryA(sizeof exe_dir, exe_dir);
        return;
    }
    slash = strrchr(path, '\\');
    if (!slash)
        slash = strrchr(path, '/');
    if (slash)
        *slash = '\0';
    snprintf(exe_dir, sizeof exe_dir, "%s", path);
}

static void assign_data_dir(const char *path, const char *base)
{
    if (full_path(configured_data_dir, sizeof configured_data_dir, path, base)) {
        explicit_data_dir = 1;
        ke_config.data_dir = configured_data_dir;
    }
}

static int parse_int(const char *value, int *result)
{
    char *end;
    long parsed;
    if (!value || !*value)
        return 0;
    parsed = strtol(value, &end, 10);
    while (*end && isspace((unsigned char)*end))
        end++;
    if (end == value || *end || parsed < -2147483647L - 1L || parsed > 2147483647L)
        return 0;
    *result = (int)parsed;
    return 1;
}

static void invalid_value(const char *section, const char *key, const char *value,
                          const char *fallback)
{
    ke_log(KE_LOG_WARN, "config", "invalid %s.%s '%s'; using %s",
           section && *section ? section : "legacy", key, value, fallback);
}

static void apply_ini_value(const char *section, const char *key, const char *value)
{
    int parsed;
    int is_video = _stricmp(section, "video") == 0;
    int is_audio = _stricmp(section, "audio") == 0;
    int is_input = _stricmp(section, "input") == 0;
    int is_system = _stricmp(section, "system") == 0;
    int is_debug = _stricmp(section, "debug") == 0;
    int is_paths = _stricmp(section, "paths") == 0;

    if ((is_paths && (_stricmp(key, "asset_dir") == 0 || _stricmp(key, "data_dir") == 0)) ||
        (!*section && (_stricmp(key, "asset_dir") == 0 || _stricmp(key, "data_dir") == 0))) {
        assign_data_dir(value, exe_dir);
    } else if (((is_video || !*section) &&
                (_stricmp(key, "window_scale") == 0 || _stricmp(key, "scale") == 0))) {
        if (parse_int(value, &parsed) && parsed >= 1 && parsed <= 8)
            ke_config.scale = parsed;
        else
            invalid_value(section, key, value, "scale 3");
    } else if (((is_video || !*section) && _stricmp(key, "fullscreen") == 0)) {
        if (parse_bool(value, &parsed))
            fullscreen = parsed;
        else
            invalid_value(section, key, value, "false");
    } else if (((is_video || !*section) &&
                (_stricmp(key, "integer_scaling") == 0 ||
                 _stricmp(key, "integer_scale") == 0))) {
        if (parse_bool(value, &parsed))
            integer_scale = parsed;
        else
            invalid_value(section, key, value, "true");
    } else if (((is_video || !*section) &&
                (_stricmp(key, "aspect") == 0 || _stricmp(key, "aspect_4_3") == 0))) {
        if (parse_bool(value, &parsed))
            ke_config.aspect = parsed;
        else if (_stricmp(value, "4:3") == 0 || _stricmp(value, "4/3") == 0)
            ke_config.aspect = 1;
        else if (_stricmp(value, "square") == 0 || _stricmp(value, "square_pixels") == 0)
            ke_config.aspect = 0;
        else
            invalid_value(section, key, value, "4:3");
    } else if (((is_audio && (_stricmp(key, "sound_blaster") == 0 ||
                              _stricmp(key, "enabled") == 0 || _stricmp(key, "audio") == 0)) ||
                (!*section && _stricmp(key, "audio") == 0))) {
        if (parse_bool(value, &parsed))
            ke_config.sound_blaster = parsed;
        else
            invalid_value(section, key, value, "true");
    } else if (((is_audio || !*section) && _stricmp(key, "volume") == 0)) {
        if (parse_int(value, &parsed) && parsed >= 0 && parsed <= 100)
            volume = parsed;
        else
            invalid_value(section, key, value, "volume 100");
    } else if (((is_input || !*section) && _stricmp(key, "joystick") == 0)) {
        if (parse_bool(value, &parsed))
            ke_config.joystick = parsed;
        else
            invalid_value(section, key, value, "false");
    } else if (is_input && _stricmp(key, "mouse_mode") == 0) {
        if (_stricmp(value, "faithful") == 0)
            ke_config.mouse_native = 0;
        else if (_stricmp(value, "native") == 0)
            ke_config.mouse_native = 1;
        else {
            invalid_value(section, key, value, "faithful");
            ke_config.mouse_native = 0;
        }
    } else if (is_input && _stricmp(key, "mouse_sensitivity") == 0) {
        /* Reserved for future relative-native input. Absolute positioning ignores it. */
        char *end;
        double sensitivity = strtod(value, &end);
        while (*end && isspace((unsigned char)*end))
            end++;
        if (end == value || *end || sensitivity < 0.1 || sensitivity > 10.0)
            invalid_value(section, key, value, "1.0 (reserved)");
    } else if (is_system && _stricmp(key, "irq") == 0) {
        if (_stricmp(value, "async") == 0)
            ke_config.irq_async = 1;
        else if (_stricmp(value, "sync") == 0)
            ke_config.irq_async = 0;
        else
            invalid_value(section, key, value, "async");
    } else if (is_system && _stricmp(key, "windows_host") == 0) {
        if (parse_bool(value, &parsed))
            ke_config.windows_host = parsed;
        else
            invalid_value(section, key, value, "false");
    } else if (is_debug && _stricmp(key, "log_level") == 0) {
        if (parse_int(value, &parsed) && parsed >= KE_LOG_ERROR && parsed <= KE_LOG_TRACE)
            ke_config.log_level = parsed;
        else
            invalid_value(section, key, value, "2");
    }
}

static int config_file_path(char *path, size_t cap)
{
    char base[CONFIG_PATH_CAP], directory[CONFIG_PATH_CAP];
    const char *override = getenv("KE_CONFIG_DIR");
    DWORD n;
#if defined(KE_ORACLE)
    if (!override || !*override)
        return path_join(path, cap, exe_dir, "nm1-test-config\\krypton-egg.ini");
#endif
    if (override && *override) {
        if (!full_path(directory, sizeof directory, override, NULL))
            return 0;
    } else {
        n = GetEnvironmentVariableA("APPDATA", base, sizeof base);
        if (!n || n >= sizeof base) {
            n = GetEnvironmentVariableA("LOCALAPPDATA", base, sizeof base);
            if (!n || n >= sizeof base) {
                n = GetEnvironmentVariableA("USERPROFILE", base, sizeof base);
                if (!n || n >= sizeof base || !path_join(directory, sizeof directory,
                                                          base, "AppData\\Roaming"))
                    return 0;
                snprintf(base, sizeof base, "%s", directory);
            }
        }
        if (!path_join(directory, sizeof directory, base, "Krypton Egg"))
            return 0;
    }
    if (!path_join(path, cap, directory, "krypton-egg.ini"))
        return 0;
    return 1;
}

static int ensure_config_directory(const char *path)
{
    char directory[CONFIG_PATH_CAP];
    char *slash;
    DWORD attrs;
    snprintf(directory, sizeof directory, "%s", path);
    slash = strrchr(directory, '\\');
    if (!slash)
        slash = strrchr(directory, '/');
    if (!slash)
        return 0;
    *slash = '\0';
    attrs = GetFileAttributesA(directory);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
        return 1;
    if (CreateDirectoryA(directory, NULL))
        return 1;
    attrs = GetFileAttributesA(directory);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static int write_default_config(const char *path)
{
    FILE *file;
    if (!ensure_config_directory(path))
        return 0;
    file = fopen(path, "w");
    if (!file)
        return 0;
    fputs("# Krypton Egg SDL3 settings. Edit values, then restart the game.\n"
          "# Precedence: these defaults, this file, legacy ke_sdl3.ini, KE_* variables, CLI.\n"
          "[video]\n"
          "fullscreen = false\n"
          "scale = 3\n"
          "aspect = true\n"
          "integer_scale = true\n\n"
          "[audio]\n"
          "sound_blaster = true\n"
          "volume = 100\n\n"
          "[input]\n"
          "joystick = false\n"
          "mouse_mode = faithful\n"
          "mouse_sensitivity = 1.0  # reserved; absolute native mode ignores this\n\n"
          "[paths]\n"
          "asset_dir =  # blank selects the usual executable/assets search\n\n"
          "[system]\n"
          "irq = async\n"
          "windows_host = false\n\n"
          "[debug]\n"
          "log_level = 2\n", file);
    if (ferror(file)) {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int read_ini_file(const char *path)
{
    char line[1024], section[128] = "";
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    while (fgets(line, sizeof line, file)) {
        char *entry = trim(line), *equals, *comment;
        if (!*entry || *entry == '#' || *entry == ';')
            continue;
        if (*entry == '[') {
            char *end = strchr(entry + 1, ']');
            if (!end)
                continue;
            *end = '\0';
            snprintf(section, sizeof section, "%s", trim(entry + 1));
            lowercase(section);
            continue;
        }
        comment = strpbrk(entry, "#;");
        if (comment)
            *comment = '\0';
        equals = strchr(entry, '=');
        if (!equals)
            continue;
        *equals++ = '\0';
        entry = trim(entry);
        equals = trim(equals);
        lowercase(entry);
        apply_ini_value(section, entry, equals);
    }
    fclose(file);
    return 1;
}

static void load_user_and_legacy_ini(void)
{
    char legacy_path[CONFIG_PATH_CAP];
    user_config_path[0] = '\0';
    if (config_file_path(user_config_path, sizeof user_config_path)) {
        if (read_ini_file(user_config_path)) {
            ke_log(KE_LOG_INFO, "config", "loaded %s", user_config_path);
        } else if (GetFileAttributesA(user_config_path) == INVALID_FILE_ATTRIBUTES) {
            if (write_default_config(user_config_path) && read_ini_file(user_config_path)) {
                ke_log(KE_LOG_INFO, "config", "created default configuration at %s",
                       user_config_path);
            } else {
                ke_log(KE_LOG_WARN, "config", "cannot create or load %s; using defaults",
                       user_config_path);
            }
        } else {
            ke_log(KE_LOG_WARN, "config", "cannot read %s; using defaults", user_config_path);
        }
    } else {
        ke_log(KE_LOG_WARN, "config", "no writable per-user config directory; using defaults");
    }
    if (path_join(legacy_path, sizeof legacy_path, exe_dir, "ke_sdl3.ini") &&
        read_ini_file(legacy_path))
        ke_log(KE_LOG_INFO, "config", "loaded legacy %s", legacy_path);
}

static const char *option_value(int *index, int argc, char **argv, const char *arg,
                                const char *option)
{
    size_t n = strlen(option);
    if (strncmp(arg, option, n) == 0 && arg[n] == '=')
        return arg + n + 1;
    if (strcmp(arg, option) == 0 && *index + 1 < argc)
        return argv[++*index];
    return NULL;
}

static void parse_command_line(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i], *value;
        int parsed;
        if ((value = option_value(&i, argc, argv, arg, "--asset-dir")) ||
            (value = option_value(&i, argc, argv, arg, "--data-dir")) ||
            (value = option_value(&i, argc, argv, arg, "--assets"))) {
            assign_data_dir(value, NULL);
        } else if ((value = option_value(&i, argc, argv, arg, "--scale"))) {
            ke_config.scale = atoi(value);
        } else if ((value = option_value(&i, argc, argv, arg, "--volume"))) {
            volume = atoi(value);
        } else if ((value = option_value(&i, argc, argv, arg, "--audio")) &&
                   parse_bool(value, &parsed)) {
            ke_config.sound_blaster = parsed;
        } else if ((value = option_value(&i, argc, argv, arg, "--joystick")) &&
                   parse_bool(value, &parsed)) {
            ke_config.joystick = parsed;
        } else if ((value = option_value(&i, argc, argv, arg, "--mouse-mode"))) {
            if (_stricmp(value, "native") == 0)
                ke_config.mouse_native = 1;
            else if (_stricmp(value, "faithful") == 0)
                ke_config.mouse_native = 0;
            else {
                invalid_value("command-line", "mouse-mode", value, "faithful");
                ke_config.mouse_native = 0;
            }
        } else if ((value = option_value(&i, argc, argv, arg, "--aspect"))) {
            if (parse_bool(value, &parsed))
                ke_config.aspect = parsed;
            else if (_stricmp(value, "4:3") == 0 || _stricmp(value, "4/3") == 0)
                ke_config.aspect = 1;
            else if (_stricmp(value, "square") == 0 || _stricmp(value, "square_pixels") == 0)
                ke_config.aspect = 0;
        } else if (strcmp(arg, "--fullscreen") == 0) {
            fullscreen = 1;
        } else if (strcmp(arg, "--windowed") == 0) {
            fullscreen = 0;
        } else if (strcmp(arg, "--integer-scaling") == 0 || strcmp(arg, "--integer-scale") == 0) {
            integer_scale = 1;
        } else if (strcmp(arg, "--no-integer-scaling") == 0 ||
                   strcmp(arg, "--no-integer-scale") == 0) {
            integer_scale = 0;
        } else if (arg[0] != '-') {
            assign_data_dir(arg, NULL); /* retain the original [DATA_DIR] invocation */
        }
    }
}

static int missing_assets(const char *dir, char *missing, size_t missing_size)
{
    size_t i, used = 0;
    int count = 0;
    char path[CONFIG_PATH_CAP];
    if (missing && missing_size)
        missing[0] = '\0';
    for (i = 0; i < sizeof required_assets / sizeof required_assets[0]; i++) {
        DWORD attrs;
        if (!path_join(path, sizeof path, dir, required_assets[i])) {
            count++;
            continue;
        }
        attrs = GetFileAttributesA(path);
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            count++;
            if (missing && missing_size && used < missing_size - 1) {
                int n = snprintf(missing + used, missing_size - used, "%s%s",
                                 used ? "\r\n" : "", required_assets[i]);
                if (n > 0)
                    used += (size_t)n < missing_size - used ? (size_t)n : missing_size - used - 1;
            }
        }
    }
    return count;
}

static int select_default_data_dir(void)
{
    char candidate[CONFIG_PATH_CAP], cwd[CONFIG_PATH_CAP];
    if (!path_join(candidate, sizeof candidate, exe_dir, ""))
        return 0;
    /* path_join with an empty leaf yields the executable directory itself. */
    if (!missing_assets(candidate, NULL, 0)) {
        snprintf(selected_data_dir, sizeof selected_data_dir, "%s", candidate);
        return 1;
    }
    if (path_join(candidate, sizeof candidate, exe_dir, "assets") &&
        !missing_assets(candidate, NULL, 0)) {
        snprintf(selected_data_dir, sizeof selected_data_dir, "%s", candidate);
        return 1;
    }
    if (GetCurrentDirectoryA(sizeof cwd, cwd) &&
        path_join(candidate, sizeof candidate, cwd, "assets") &&
        !missing_assets(candidate, NULL, 0)) {
        snprintf(selected_data_dir, sizeof selected_data_dir, "%s", candidate);
        return 1;
    }
    snprintf(selected_data_dir, sizeof selected_data_dir, "%s", exe_dir);
    return 0;
}

void ke_config_load(int argc, char **argv)
{
    const char *irq;
    int parsed;
    memset(&ke_config, 0, sizeof ke_config);
    ke_config.irq_async = 1;
    ke_config.sound_blaster = 1;
    ke_config.scale = 3;
    ke_config.aspect = 1;
    ke_config.log_level = KE_LOG_INFO;
    fullscreen = 0;
    integer_scale = 1;
    volume = 100;
    explicit_data_dir = 0;
    find_exe_dir();
    load_user_and_legacy_ini();

    irq = getenv("KE_IRQ");
    if (irq && *irq)
        ke_config.irq_async = strcmp(irq, "sync") != 0;
    ke_config.windows_host = env_int("KE_WINDOWS", ke_config.windows_host);
    ke_config.sound_blaster = env_int("KE_SB", ke_config.sound_blaster);
    ke_config.joystick = env_int("KE_JOY", ke_config.joystick);
    ke_config.scale = env_int("KE_SCALE", ke_config.scale);
    ke_config.aspect = env_int("KE_ASPECT", ke_config.aspect);
    ke_config.log_level = env_int("KE_LOG_LEVEL", ke_config.log_level);
    fullscreen = env_int("KE_FULLSCREEN", fullscreen);
    integer_scale = env_int("KE_INTEGER_SCALE", integer_scale);
    volume = env_int("KE_VOLUME", volume);
    if (getenv("KE_AUDIO") && parse_bool(getenv("KE_AUDIO"), &parsed))
        ke_config.sound_blaster = parsed;
    if (getenv("KE_MOUSE_MODE")) {
        const char *mode = getenv("KE_MOUSE_MODE");
        if (_stricmp(mode, "native") == 0)
            ke_config.mouse_native = 1;
        else if (_stricmp(mode, "faithful") == 0)
            ke_config.mouse_native = 0;
        else {
            invalid_value("environment", "KE_MOUSE_MODE", mode, "faithful");
            ke_config.mouse_native = 0;
        }
    }

    if (getenv("KE_DATA"))
        assign_data_dir(getenv("KE_DATA"), NULL);
    parse_command_line(argc, argv);
    if (ke_config.scale < 1 || ke_config.scale > 8)
        ke_config.scale = 3;
    if (volume < 0)
        volume = 0;
    if (volume > 100)
        volume = 100;
    ke_config.aspect = !!ke_config.aspect;
    fullscreen = !!fullscreen;
    integer_scale = !!integer_scale;
    ke_log(KE_LOG_INFO, "input", "mouse mode = %s",
           ke_config.mouse_native ? "native" : "faithful");

    if (explicit_data_dir) {
        snprintf(selected_data_dir, sizeof selected_data_dir, "%s", configured_data_dir);
    } else {
        select_default_data_dir();
    }
    ke_config.data_dir = selected_data_dir;
}

int ke_config_fullscreen(void) { return fullscreen; }
int ke_config_integer_scale(void) { return integer_scale; }
int ke_config_volume(void) { return volume; }

int ke_config_validate_assets(char *missing, size_t missing_size)
{
    return missing_assets(ke_config.data_dir ? ke_config.data_dir : "", missing, missing_size) == 0;
}
