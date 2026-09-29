/* platform_win32.c - Win32 implementation of ke_platform.h.
 *
 * Every function here is the Win32 code the port used before the platform seam existed
 * (moved from vhw/cpu.c, pic.c, pit.c, lowmem.c, fault.c, vhw.c and host/log.c,
 * gamethread.c, config.c, files.c), so the Windows build keeps its exact behaviour.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <share.h>
#include <windows.h>
#include <timeapi.h>
#include "ke_platform.h"
#include "../include/ke_port.h"

/* ---- once ------------------------------------------------------------------------------ */
static BOOL CALLBACK once_trampoline(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    (void)once;
    (void)context;
    ((void (*)(void))parameter)();
    return TRUE;
}

void ke_once(KeOnce *once, void (*fn)(void))
{
    InitOnceExecuteOnce(&once->once, once_trampoline, (PVOID)fn, NULL);
}

/* ---- events and timers ----------------------------------------------------------------- */
KeEvent *ke_event_create(void) { return (KeEvent *)CreateEventW(NULL, FALSE, FALSE, NULL); }
void ke_event_destroy(KeEvent *event) { if (event) CloseHandle((HANDLE)event); }
void ke_event_set(KeEvent *event) { SetEvent((HANDLE)event); }

int ke_event_wait_ms(KeEvent *event, uint32_t ms)
{
    return WaitForSingleObject((HANDLE)event, ms == KE_WAIT_INFINITE ? INFINITE : ms) ==
           WAIT_OBJECT_0;
}

KeTimer *ke_timer_create(void)
{
    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
    if (!timer)
        timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    return (KeTimer *)timer;
}

void ke_timer_destroy(KeTimer *timer)
{
    if (timer)
        CloseHandle((HANDLE)timer);
}

int ke_event_wait_timer_ns(KeEvent *event, KeTimer *timer, uint64_t ns)
{
    HANDLE waits[2] = { (HANDLE)event, (HANDLE)timer };
    LARGE_INTEGER due;
    DWORD wait_result;
    due.QuadPart = -(LONGLONG)(ns / 100);
    if (due.QuadPart == 0)
        due.QuadPart = -1;
    SetWaitableTimer((HANDLE)timer, &due, 0, NULL, NULL, FALSE);
    wait_result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    CancelWaitableTimer((HANDLE)timer);
    return wait_result == WAIT_OBJECT_0;
}

void ke_poll_wait_100us(void)
{
    static __thread HANDLE poll_timer;
    LARGE_INTEGER due;
    if (!poll_timer)
        poll_timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                            TIMER_ALL_ACCESS);
    due.QuadPart = -1000;           /* wait 100 us once per 4096 polls */
    if (poll_timer && SetWaitableTimer(poll_timer, &due, 0, NULL, NULL, FALSE))
        WaitForSingleObject(poll_timer, INFINITE);
    else
        Sleep(1);                   /* bounded safe-point wait if high-res timers fail */
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

uint64_t ke_monotonic_raw_ns(void)
{
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    if (!frequency.QuadPart)
        QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart > 0 && QueryPerformanceCounter(&now))
        return (uint64_t)((double)now.QuadPart * 1000000000.0 / (double)frequency.QuadPart);
    return 0;
}

void ke_timer_resolution_begin(void) { timeBeginPeriod(1); }
void ke_sleep_ms(uint32_t ms) { Sleep(ms); }
void ke_time_pause(void) {}
void ke_time_resume(void) {}

/* ---- threads --------------------------------------------------------------------------- */
struct KeThread {
    HANDLE handle;
    DWORD id;
};

typedef struct ThreadStart { KeThreadFn fn; void *arg; } ThreadStart;

static DWORD WINAPI thread_trampoline(LPVOID parameter)
{
    ThreadStart start = *(ThreadStart *)parameter;
    free(parameter);
    return (DWORD)start.fn(start.arg);
}

KeThread *ke_thread_create(KeThreadFn fn, void *arg, size_t stack_size, unsigned flags)
{
    KeThread *thread = (KeThread *)calloc(1, sizeof *thread);
    ThreadStart *start = (ThreadStart *)malloc(sizeof *start);
    if (!thread || !start) {
        free(thread);
        free(start);
        return NULL;
    }
    start->fn = fn;
    start->arg = arg;
    thread->handle = CreateThread(NULL, stack_size, thread_trampoline, start,
                                  (flags & KE_THREAD_HELD) ? CREATE_SUSPENDED : 0, &thread->id);
    if (!thread->handle) {
        free(start);
        free(thread);
        return NULL;
    }
    return thread;
}

void ke_thread_start(KeThread *thread) { ResumeThread(thread->handle); }

int ke_thread_join_ms(KeThread *thread, uint32_t ms)
{
    return WaitForSingleObject(thread->handle, ms == KE_WAIT_INFINITE ? INFINITE : ms) !=
           WAIT_TIMEOUT;
}

int ke_thread_terminate(KeThread *thread)
{
    TerminateThread(thread->handle, 4);
    WaitForSingleObject(thread->handle, INFINITE);
    return 1;
}

void ke_thread_close(KeThread *thread)
{
    if (!thread)
        return;
    CloseHandle(thread->handle);
    free(thread);
}

KeThread *ke_thread_adopt_current(void)
{
    KeThread *thread = (KeThread *)calloc(1, sizeof *thread);
    if (!thread)
        return NULL;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                    &thread->handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    thread->id = GetCurrentThreadId();
    return thread;
}

void ke_thread_set_time_critical(void)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
}

void ke_platform_set_game_stack(void *base, size_t size) { (void)base; (void)size; }

int ke_thread_interrupt_runs_on_target(void) { return 0; }
void ke_thread_interrupt_leaving(void) {}

/* The target is frozen by SuspendThread; fn decides with its program counter whether an
 * interrupt handler may run now and runs it on this (the IRQ) thread. */
int ke_thread_interrupt(KeThread *target, KeInterruptFn fn, void *arg)
{
    CONTEXT ctx;
    KeRedirect redirect = {0, 0};
    int result = 0;
    if (SuspendThread(target->handle) == (DWORD)-1)
        return -1;
    memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (GetThreadContext(target->handle, &ctx))
        result = fn((uintptr_t)ctx.Eip, arg, &redirect);
    else
        result = fn(UINTPTR_MAX, arg, &redirect);  /* no context: not interruptible */
    if (redirect.fn) {
        uint32_t *sp;
        memset(&ctx, 0, sizeof ctx);
        ctx.ContextFlags = CONTEXT_CONTROL;
        GetThreadContext(target->handle, &ctx);
        sp = (uint32_t *)(uintptr_t)(ctx.Esp - 8);
        sp[0] = 0;                 /* return address: the redirect target never returns */
        sp[1] = (uint32_t)redirect.arg;
        ctx.Esp -= 8;
        ctx.Eip = (DWORD)(uintptr_t)redirect.fn;
        SetThreadContext(target->handle, &ctx);
    }
    ResumeThread(target->handle);
    return result;
}

void ke_platform_code_range(uintptr_t *lo, uintptr_t *hi)
{
    uint8_t *base = (uint8_t *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    int i;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
        if (memcmp(sec->Name, ".text", 5) == 0) {
            *lo = (uint32_t)(uintptr_t)(base + sec->VirtualAddress);
            *hi = *lo + sec->Misc.VirtualSize;
        }
}

/* Diagnostics: log the game thread's EIP and EBP-chain return addresses (game code is built
 * with -O0, so frame pointers are intact). Resolve with `addr2line -f -e ke_sdl3.exe ADDR`. */
void ke_platform_log_thread_backtrace(KeThread *thread, const char *why)
{
    CONTEXT ctx;
    uint32_t ebp, frames[12];
    int n = 0, i;
    char text[256];
    int len;
    if (!thread)
        return;
    if (SuspendThread(thread->handle) == (DWORD)-1)
        return;
    memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(thread->handle, &ctx)) {
        frames[n++] = ctx.Eip;
        ebp = ctx.Ebp;
        while (n < 12 && ebp && !IsBadReadPtr((void *)(uintptr_t)ebp, 8)) {
            uint32_t next = ((uint32_t *)(uintptr_t)ebp)[0];
            frames[n++] = ((uint32_t *)(uintptr_t)ebp)[1];
            if (next <= ebp)
                break;
            ebp = next;
        }
    }
    ResumeThread(thread->handle);
    /* print link-time addresses so addr2line works despite ASLR */
    {
        uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
        uint32_t delta = (uint32_t)(base - nt->OptionalHeader.ImageBase);
        len = snprintf(text, sizeof text, "game thread (%s), link addresses:", why);
        for (i = 0; i < n && len < (int)sizeof text - 12; i++)
            len += snprintf(text + len, sizeof text - (size_t)len, " %08X", frames[i] - delta);
    }
    ke_log(KE_LOG_INFO, "game", "%s", text);
}

/* ---- memory ---------------------------------------------------------------------------- */
int ke_platform_map_fixed(uintptr_t addr, size_t size)
{
    /* Commit the range the parent reserved for us (main_sdl.c), or try to reserve it now. */
    uint8_t *p = (uint8_t *)VirtualAlloc((void *)addr, size, MEM_COMMIT, PAGE_READWRITE);
    if (p != (uint8_t *)addr)
        p = (uint8_t *)VirtualAlloc((void *)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (p != (uint8_t *)addr) {
        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t a = 0;
        while (a < 0x200000 && VirtualQuery((void *)a, &mbi, sizeof mbi)) {
            ke_log(KE_LOG_ERROR, "lowmem", "  %08lX +%08lX state=%lX type=%lX",
                   (unsigned long)(uintptr_t)mbi.BaseAddress, (unsigned long)mbi.RegionSize,
                   mbi.State, mbi.Type);
            a = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        }
        ke_log(KE_LOG_ERROR, "lowmem", "cannot map linear %05lX..%06lX (got %p): the range is in "
               "use (started without the relaunch, e.g. KE_CHILD set?)", (unsigned long)addr,
               (unsigned long)(addr + size - 1), (void *)p);
        return -1;
    }
    return 0;
}

static char fault_text[256];
static void (*fault_describe)(char *, size_t, int, uintptr_t, uintptr_t);

static void fault_trampoline(void) { ke_stop_game_from_fault(fault_text); }

static LONG CALLBACK fault_handler(EXCEPTION_POINTERS *ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    uintptr_t eip = (uintptr_t)ep->ContextRecord->Eip;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_PRIV_INSTRUCTION &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR rw = ep->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR a = ep->ExceptionRecord->ExceptionInformation[1];
        fault_describe(fault_text, sizeof fault_text, rw != 0, a, eip);
    } else {
        snprintf(fault_text, sizeof fault_text, "exception %08lX at EIP %08lX",
                 (unsigned long)code, (unsigned long)eip);
    }
    ke_log(KE_LOG_ERROR, "fault", "%s thread: %s", ke_on_game_thread() ? "game" : "other",
           fault_text);
    if (!ke_on_game_thread())
        return EXCEPTION_CONTINUE_SEARCH;
    ep->ContextRecord->Esp = (ep->ContextRecord->Esp - 64) & ~15u;
    *(DWORD *)(uintptr_t)ep->ContextRecord->Esp = 0;          /* fake return address */
    ep->ContextRecord->Eip = (DWORD)(uintptr_t)fault_trampoline;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void ke_platform_fault_init(int first, void (*describe)(char *, size_t, int, uintptr_t, uintptr_t))
{
    fault_describe = describe;
    AddVectoredExceptionHandler(first ? 1 : 0, fault_handler);
}

/* ---- files ----------------------------------------------------------------------------- */
int ke_platform_exe_dir(char *out, size_t cap)
{
    char path[MAX_PATH];
    char *slash;
    DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    if (!n || n >= sizeof path) {
        GetCurrentDirectoryA((DWORD)cap, out);
        return 1;
    }
    slash = strrchr(path, '\\');
    if (!slash)
        slash = strrchr(path, '/');
    if (slash)
        *slash = '\0';
    snprintf(out, cap, "%s", path);
    return 1;
}

int ke_platform_cwd(char *out, size_t cap) { return GetCurrentDirectoryA((DWORD)cap, out) != 0; }

int ke_platform_chdir(const char *path) { return SetCurrentDirectoryA(path) ? 0 : -1; }

int ke_platform_full_path(const char *path, char *out, size_t cap)
{
    DWORD n = GetFullPathNameA(path, (DWORD)cap, out, NULL);
    return n != 0 && n < cap;
}

int ke_platform_is_absolute(const char *path)
{
    return (strlen(path) > 2 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) ||
           path[0] == '\\' || path[0] == '/';
}

int ke_platform_path_exists(const char *path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

int ke_platform_is_dir(const char *path)
{
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

int ke_platform_mkdir(const char *path)
{
    if (ke_platform_is_dir(path))
        return 1;
    if (CreateDirectoryA(path, NULL))
        return 1;
    return ke_platform_is_dir(path);
}

int ke_platform_config_base(char *out, size_t cap)
{
    char base[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("APPDATA", base, sizeof base);
    if (!n || n >= sizeof base) {
        n = GetEnvironmentVariableA("LOCALAPPDATA", base, sizeof base);
        if (!n || n >= sizeof base) {
            int written;
            n = GetEnvironmentVariableA("USERPROFILE", base, sizeof base);
            if (!n || n >= sizeof base)
                return 0;
            written = snprintf(out, cap, "%s%sAppData\\Roaming", base,
                               base[n - 1] != '\\' && base[n - 1] != '/' ? "\\" : "");
            return written >= 0 && (size_t)written < cap;
        }
    }
    return snprintf(out, cap, "%s", base) < (int)cap;
}

int ke_platform_local_data_base(char *out, size_t cap)
{
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", out, (DWORD)cap);
    return n && n < cap;
}

FILE *ke_platform_fopen(const char *path, const char *mode)
{
    return _fsopen(path, mode, _SH_DENYNO);
}
