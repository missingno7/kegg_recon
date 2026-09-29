/* clib.c - Watcom C runtime entry points that must not reach the host C runtime directly.
 * String, stdio streams, malloc, and similar calls use the host runtime, whose DOS-heritage
 * text/binary behavior matches Watcom. fopen is routed through files.c for game-file policy. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ke_port.h"
#include "../vhw/vhw.h"

/* Route the historical game's fopen calls through the DOS-file policy in files.c.  The
 * host logger and oracle use fopen too; ke_fopen passes absolute/non-game paths through. */
FILE *ke_fopen(const char *path, const char *mode);

#if !defined(KE_GAME_ILP32)
/* On 64-bit hosts the game world calls ke32_fopen/ke32_spawn_refused instead
 * (port/android/ilp32); the host's own fopen stays the C library's. */
FILE *fopen(const char *path, const char *mode)
{
    return ke_fopen(path, mode);
}

/* launch_print_order_form() spawns the DOS print utility: never start host programs. */
int spawnlp(int mode, const char *path, const char *arg0, ...)
{
    (void)mode; (void)arg0;
    ke_log(KE_LOG_WARN, "clib", "spawnlp(\"%s\") refused: the port does not run DOS programs",
           path ? path : "");
    return -1;
}
#endif

void delay(unsigned int milliseconds)
{
    vhw_enter();
    vhw_idle((uint64_t)milliseconds * 1000000ull);
    vhw_leave();
}

void sound(unsigned int hz) { (void)hz; }
void nosound(void) {}

/* The DOS environment the game sees (watcom_compat.h maps getenv): COMSPEC and PATH,
 * BLASTER when a Sound Blaster is attached, WINDIR with KE_WINDOWS=1 (the game treats
 * WINDIR as "running under Windows"), plus KE_DOSENV="NAME=value;NAME=value". */
char *ke_getenv(const char *name)
{
    static char env[1024];
    static size_t env_len;
    size_t n = strlen(name), i;
    if (!env_len) {
        const char *extra = getenv("KE_DOSENV");
        int used = snprintf(env, sizeof env, "COMSPEC=C:\\COMMAND.COM;PATH=C:\\DOS;");
        if (ke_config.sound_blaster)
            used += snprintf(env + used, sizeof env - (size_t)used, "BLASTER=A220 I7 D1 T3;");
        if (ke_config.windows_host)
            used += snprintf(env + used, sizeof env - (size_t)used, "WINDIR=C:\\WINDOWS;");
        if (extra)
            used += snprintf(env + used, sizeof env - (size_t)used, "%s;", extra);
        env_len = strlen(env);
        for (i = 0; i < env_len; i++)
            if (env[i] == ';')
                env[i] = 0;
    }
    for (i = 0; i < env_len; i += strlen(env + i) + 1)
        if (strncmp(env + i, name, n) == 0 && env[i + n] == '=')
            return env + i + n + 1;
    return NULL;
}
