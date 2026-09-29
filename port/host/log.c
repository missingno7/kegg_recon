/* log.c - logging, configuration, host time and stub bookkeeping. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../platform/ke_platform.h"
#include "ke_port.h"

static FILE *log_file;
static KeMutex log_lock;
static int log_ready;
static const char *level_name[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

void ke_log_init(const char *path)
{
    ke_mutex_init(&log_lock);
    log_ready = 1;
    if (path)
        log_file = fopen(path, "w");
}

int ke_log_enabled(int level) { return level <= ke_config.log_level; }

void ke_vlog(int level, const char *subsystem, const char *fmt, va_list ap)
{
    char line[1024];
    int n;
    if (!ke_log_enabled(level))
        return;
    n = snprintf(line, sizeof line, "[%8.3f %-5s %-6s] ", (double)ke_now_ns() / 1e9,
                 level_name[level], subsystem);
    vsnprintf(line + n, sizeof line - n, fmt, ap);
    if (log_ready)
        ke_mutex_lock(&log_lock);
    fprintf(stderr, "%s\n", line);
    if (log_file) {
        fprintf(log_file, "%s\n", line);
        fflush(log_file);
    }
    if (log_ready)
        ke_mutex_unlock(&log_lock);
}

void ke_log(int level, const char *subsystem, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ke_vlog(level, subsystem, fmt, ap);
    va_end(ap);
}

/* ---- once-only keys (small open-addressing set; never freed) --------------------------- */
#define ONCE_SLOTS 1024
static char *once_keys[ONCE_SLOTS];

static int once_first(const char *key)
{
    unsigned h = 5381;
    const char *p;
    int i, first = 0;
    for (p = key; *p; p++)
        h = h * 33 + (unsigned char)*p;
    if (log_ready)
        ke_mutex_lock(&log_lock);
    for (i = 0; i < ONCE_SLOTS; i++) {
        unsigned slot = (h + i) % ONCE_SLOTS;
        if (!once_keys[slot]) {
            once_keys[slot] = ke_strdup(key);
            first = 1;
            break;
        }
        if (strcmp(once_keys[slot], key) == 0)
            break;
    }
    if (log_ready)
        ke_mutex_unlock(&log_lock);
    return first;
}

void ke_log_once(const char *key, int level, const char *subsystem, const char *fmt, ...)
{
    va_list ap;
    if (!once_first(key))
        return;
    va_start(ap, fmt);
    ke_vlog(level, subsystem, fmt, ap);
    va_end(ap);
}

/* ---- stubs ----------------------------------------------------------------------------- */
#define MAX_STUBS 256
static const char *stub_names[MAX_STUBS];
static const char *stub_owner[MAX_STUBS];
static long stub_calls[MAX_STUBS];
static int stub_count;

void ke_stub_hit(const char *name, const char *owner)
{
    int i;
    if (log_ready)
        ke_mutex_lock(&log_lock);
    for (i = 0; i < stub_count; i++)
        if (stub_names[i] == name)
            break;
    if (i == stub_count && stub_count < MAX_STUBS) {
        stub_names[i] = name;
        stub_owner[i] = owner;
        stub_count++;
    }
    if (i < MAX_STUBS)
        stub_calls[i]++;
    if (log_ready)
        ke_mutex_unlock(&log_lock);
    if (i < MAX_STUBS && stub_calls[i] == 1)
        ke_log(KE_LOG_WARN, "stub", "STUB %s (%s) reached; returns 0", name, owner);
}

void ke_stub_report(void)
{
    int i;
    ke_log(KE_LOG_INFO, "stub", "%d distinct stubs reached:", stub_count);
    for (i = 0; i < stub_count; i++)
        ke_log(KE_LOG_INFO, "stub", "  %-40s %-28s calls=%ld", stub_names[i], stub_owner[i],
               stub_calls[i]);
}

/* ---- time: port/platform (ke_now_ns, ke_sleep_ns) ------------------------------------ */
