/* log.c - logging, configuration, host time and stub bookkeeping. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "ke_port.h"

KeConfig ke_config;
static FILE *log_file;
static CRITICAL_SECTION log_lock;
static int log_ready;
static const char *level_name[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

static int env_int(const char *name, int dflt)
{
    const char *v = getenv(name);
    return (v && *v) ? atoi(v) : dflt;
}

void ke_config_load(int argc, char **argv)
{
    const char *irq = getenv("KE_IRQ");
    ke_config.irq_async = !(irq && strcmp(irq, "sync") == 0);
    ke_config.windows_host = env_int("KE_WINDOWS", 0);
    ke_config.sound_blaster = env_int("KE_SB", 0);
    ke_config.joystick = env_int("KE_JOY", 0);
    ke_config.scale = env_int("KE_SCALE", 3);
    ke_config.aspect = env_int("KE_ASPECT", 1);
    ke_config.log_level = env_int("KE_LOG_LEVEL", KE_LOG_INFO);
    ke_config.data_dir = (argc > 1) ? argv[1] : getenv("KE_DATA");
}

void ke_log_init(const char *path)
{
    InitializeCriticalSection(&log_lock);
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
        EnterCriticalSection(&log_lock);
    fprintf(stderr, "%s\n", line);
    if (log_file) {
        fprintf(log_file, "%s\n", line);
        fflush(log_file);
    }
    if (log_ready)
        LeaveCriticalSection(&log_lock);
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
        EnterCriticalSection(&log_lock);
    for (i = 0; i < ONCE_SLOTS; i++) {
        unsigned slot = (h + i) % ONCE_SLOTS;
        if (!once_keys[slot]) {
            once_keys[slot] = _strdup(key);
            first = 1;
            break;
        }
        if (strcmp(once_keys[slot], key) == 0)
            break;
    }
    if (log_ready)
        LeaveCriticalSection(&log_lock);
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
        EnterCriticalSection(&log_lock);
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
        LeaveCriticalSection(&log_lock);
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

/* ---- time ------------------------------------------------------------------------------ */
uint64_t ke_now_ns(void)
{
    static LARGE_INTEGER freq, origin;
    LARGE_INTEGER now;
    if (!freq.QuadPart) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&origin);
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)((double)(now.QuadPart - origin.QuadPart) * 1e9 / (double)freq.QuadPart);
}

void ke_sleep_ns(uint64_t ns)
{
    static __thread HANDLE timer; /* one waitable timer per thread */
    LARGE_INTEGER due;
    uint64_t end = ke_now_ns() + ns;
    if (ns >= 1500000) {
        if (!timer)
            timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                           TIMER_ALL_ACCESS);
        if (timer) {
            due.QuadPart = -(LONGLONG)((ns - 500000) / 100);
            SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
            WaitForSingleObject(timer, INFINITE);
        } else {
            Sleep((DWORD)(ns / 1000000));
        }
    }
    while (ke_now_ns() < end)
        YieldProcessor();
}
