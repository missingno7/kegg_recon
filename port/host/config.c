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

static void apply_ini_value(const char *key, const char *value)
{
    int parsed;
    if (_stricmp(key, "asset_dir") == 0 || _stricmp(key, "data_dir") == 0) {
        assign_data_dir(value, exe_dir);
    } else if (_stricmp(key, "window_scale") == 0 || _stricmp(key, "scale") == 0) {
        ke_config.scale = atoi(value);
    } else if (_stricmp(key, "fullscreen") == 0 && parse_bool(value, &parsed)) {
        fullscreen = parsed;
    } else if ((_stricmp(key, "integer_scaling") == 0 ||
                _stricmp(key, "integer_scale") == 0) && parse_bool(value, &parsed)) {
        integer_scale = parsed;
    } else if (_stricmp(key, "aspect") == 0 || _stricmp(key, "aspect_4_3") == 0) {
        if (parse_bool(value, &parsed))
            ke_config.aspect = parsed;
        else if (_stricmp(value, "4:3") == 0 || _stricmp(value, "4/3") == 0)
            ke_config.aspect = 1;
        else if (_stricmp(value, "square") == 0 || _stricmp(value, "square_pixels") == 0)
            ke_config.aspect = 0;
    } else if (_stricmp(key, "audio") == 0 && parse_bool(value, &parsed)) {
        ke_config.sound_blaster = parsed;
    } else if (_stricmp(key, "joystick") == 0 && parse_bool(value, &parsed)) {
        ke_config.joystick = parsed;
    } else if (_stricmp(key, "volume") == 0) {
        volume = atoi(value);
    }
}

static void read_ini(void)
{
    char path[CONFIG_PATH_CAP], line[1024];
    FILE *file;
    if (!path_join(path, sizeof path, exe_dir, "ke_sdl3.ini"))
        return;
    file = fopen(path, "r");
    if (!file)
        return;
    while (fgets(line, sizeof line, file)) {
        char *entry = trim(line), *equals, *comment;
        if (!*entry || *entry == '#' || *entry == ';' || *entry == '[')
            continue;
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
        apply_ini_value(entry, equals);
    }
    fclose(file);
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
    ke_config.scale = 3;
    ke_config.aspect = 1;
    ke_config.log_level = KE_LOG_INFO;
    fullscreen = 0;
    integer_scale = 1;
    volume = 100;
    explicit_data_dir = 0;
    find_exe_dir();
    read_ini();

    irq = getenv("KE_IRQ");
    ke_config.irq_async = !(irq && strcmp(irq, "sync") == 0);
    ke_config.windows_host = env_int("KE_WINDOWS", 0);
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
