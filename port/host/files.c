/* files.c - file policy for the historical DOS game.
 *
 * The game opens every input file with "r+b", although those files are only read.  Map
 * that mode to "rb" so a read-only data directory works.  Its high-score file is special:
 * prefer a per-user copy, fall back to the bundled default when no copy exists, and put
 * every write in the per-user directory.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "ke_port.h"
#include "../platform/ke_platform.h"

#define KE_SCORE_NAME "ke_score.lst"

static int is_score_file(const char *path)
{
    const char *base = path;
    const char *p;
    if (!path)
        return 0;
    for (p = path; *p; ++p)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    return ke_stricmp(base, KE_SCORE_NAME) == 0;
}

static int score_user_path(char *out, size_t out_size)
{
    char base[KE_MAX_PATH];
    int written;

    if (!ke_platform_local_data_base(base, sizeof base)) {
        ke_log(KE_LOG_WARN, "files", "LOCALAPPDATA is unavailable; score persistence is disabled");
        return 0;
    }
    written = snprintf(out, out_size, "%s" KE_PATH_SEP_STR "Krypton Egg", base);
    if (written < 0 || (size_t)written >= out_size)
        return 0;

    if (!ke_platform_mkdir(out)) {
        ke_log(KE_LOG_WARN, "files", "cannot create score directory %s", out);
        return 0;
    }

    written = snprintf(base, sizeof base, "%s" KE_PATH_SEP_STR "%s", out, KE_SCORE_NAME);
    if (written < 0 || (size_t)written >= sizeof base || (size_t)written >= out_size)
        return 0;
    memcpy(out, base, (size_t)written + 1);
    return 1;
}

static FILE *open_shared(const char *path, const char *mode)
{
    return ke_platform_fopen(path, mode);
}

static int is_read_update_mode(const char *mode)
{
    return mode && mode[0] == 'r' && strchr(mode, '+') != NULL;
}

FILE *ke_fopen(const char *path, const char *mode)
{
    char score_path[KE_MAX_PATH];
    const int score = is_score_file(path);

    if (!path || !mode) {
        errno = EINVAL;
        return NULL;
    }

    if (score && mode[0] != 'r') {
        if (!score_user_path(score_path, sizeof score_path)) {
            errno = ENOENT;
            return NULL;
        }
        return open_shared(score_path, mode);
    }

    if (score && is_read_update_mode(mode)) {
        if (score_user_path(score_path, sizeof score_path)) {
            FILE *user_file = open_shared(score_path, "rb");
            if (user_file)
                return user_file;
            if (errno != ENOENT)
                return NULL;
        }
        /* The process starts in the configured data directory (main_sdl.c). */
        return open_shared(path, "rb");
    }

    if (is_read_update_mode(mode))
        mode = "rb";
    return open_shared(path, mode);
}
