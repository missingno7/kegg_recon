/* ke_platform.h - the host operating-system seam under port/vhw and port/host.
 *
 * The virtual PC and the SDL host are the same C code on every host; everything that is an
 * operating-system primitive goes through this header (docs/android/architecture.md,
 * "Platform abstraction"):
 *
 *   mutexes        recursive (Win32 CRITICAL_SECTION semantics; the POSIX version is a
 *                  PTHREAD_MUTEX_RECURSIVE mutex)
 *   atomics        on `ke_atomic_t` (= volatile long); Win32 keeps the Interlocked* calls
 *   events         auto-reset, with millisecond and high-resolution nanosecond waits
 *   threads        create (optionally held until started), join with timeout, priority
 *   interrupts     ke_thread_interrupt(): run a callback while the game thread is stopped
 *                  at an instruction boundary (Win32: SuspendThread/GetThreadContext on the
 *                  calling thread; POSIX: a real-time signal handled on the game thread)
 *   memory         fixed-address mappings for the identity-mapped DOS memory
 *   time           monotonic clock, precise sleeps
 *   files          directories, full paths, the per-user data/config locations
 *
 * platform_win32.c keeps the exact calls the Windows port has always made, so the Windows
 * build is behaviourally unchanged; platform_posix.c is the Android (and Linux) version.
 */
#ifndef KE_PLATFORM_H
#define KE_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>

typedef CRITICAL_SECTION KeMutex;
#define ke_mutex_init(m) InitializeCriticalSection(m)
#define ke_mutex_lock(m) EnterCriticalSection(m)
#define ke_mutex_unlock(m) LeaveCriticalSection(m)
#define ke_mutex_destroy(m) DeleteCriticalSection(m)
#define ke_thread_yield() SwitchToThread()

typedef LONG ke_atomic_value;
typedef volatile LONG ke_atomic_t;
#define ke_atomic_exchange(p, v) InterlockedExchange((p), (v))
#define ke_atomic_increment(p) InterlockedIncrement(p)
#define ke_atomic_decrement(p) InterlockedDecrement(p)
#define ke_atomic_compare_exchange(p, v, cmp) InterlockedCompareExchange((p), (v), (cmp))
#define ke_atomic_load(p) InterlockedCompareExchange((p), 0, 0)
#define ke_cpu_relax() YieldProcessor()

typedef DWORD ke_thread_id;
#define ke_thread_current_id() GetCurrentThreadId()

#define KE_PATH_SEP '\\'
#define KE_PATH_SEP_STR "\\"
#define ke_stricmp _stricmp
#define ke_strdup _strdup
#define KE_MAX_PATH MAX_PATH

#else /* POSIX */
#include <pthread.h>
#include <strings.h>
#include <string.h>

typedef pthread_mutex_t KeMutex;
void ke_mutex_init(KeMutex *m); /* recursive, like a CRITICAL_SECTION */
#define ke_mutex_lock(m) pthread_mutex_lock(m)
#define ke_mutex_unlock(m) pthread_mutex_unlock(m)
#define ke_mutex_destroy(m) pthread_mutex_destroy(m)
#include <sched.h>
#define ke_thread_yield() sched_yield()

typedef long ke_atomic_value;
typedef volatile long ke_atomic_t;
static inline long ke_atomic_exchange(ke_atomic_t *p, long v)
{
    return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST);
}
static inline long ke_atomic_increment(ke_atomic_t *p)
{
    return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}
static inline long ke_atomic_decrement(ke_atomic_t *p)
{
    return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}
/* Returns the initial value, as InterlockedCompareExchange. */
static inline long ke_atomic_compare_exchange(ke_atomic_t *p, long v, long cmp)
{
    long expected = cmp;
    __atomic_compare_exchange_n(p, &expected, v, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
static inline long ke_atomic_load(ke_atomic_t *p)
{
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}
#if defined(__x86_64__) || defined(__i386__)
#define ke_cpu_relax() __builtin_ia32_pause()
#elif defined(__aarch64__) || defined(__arm__)
#define ke_cpu_relax() __asm__ __volatile__("yield" ::: "memory")
#else
#define ke_cpu_relax() __asm__ __volatile__("" ::: "memory")
#endif

typedef uint64_t ke_thread_id;
ke_thread_id ke_thread_current_id(void);

#define KE_PATH_SEP '/'
#define KE_PATH_SEP_STR "/"
#define ke_stricmp strcasecmp
#define _stricmp strcasecmp   /* the MSVC/Watcom spelling port/host uses */
#define ke_strdup strdup
#define KE_MAX_PATH 1024
#endif

/* ---- once ------------------------------------------------------------------------------ */
typedef struct KeOnce {
#if defined(_WIN32)
    INIT_ONCE once;
#else
    pthread_once_t once;
#endif
} KeOnce;
#if defined(_WIN32)
#define KE_ONCE_INIT {INIT_ONCE_STATIC_INIT}
#else
#define KE_ONCE_INIT {PTHREAD_ONCE_INIT}
#endif
void ke_once(KeOnce *once, void (*fn)(void));

/* ---- auto-reset events and high-resolution waits ---------------------------------------- */
#define KE_WAIT_INFINITE 0xffffffffu
typedef struct KeEvent KeEvent;
KeEvent *ke_event_create(void);
void ke_event_destroy(KeEvent *event);
void ke_event_set(KeEvent *event);
/* 1 = signalled (and reset), 0 = timed out. ms = 0 polls, KE_WAIT_INFINITE blocks. */
int ke_event_wait_ms(KeEvent *event, uint32_t ms);
/* High-resolution wait for the event or `ns` elapsed (pit.c). A timer object is per thread. */
typedef struct KeTimer KeTimer;
KeTimer *ke_timer_create(void);
void ke_timer_destroy(KeTimer *timer);
int ke_event_wait_timer_ns(KeEvent *event, KeTimer *timer, uint64_t ns);
/* The historical memory-poll yield: a short precise wait at a safe point (cpu.c). */
void ke_poll_wait_100us(void);

/* ---- time ------------------------------------------------------------------------------ */
uint64_t ke_now_ns(void);                 /* monotonic since first use (ke_port.h)         */
void ke_sleep_ns(uint64_t ns);            /* precise sleep (ke_port.h)                     */
uint64_t ke_monotonic_raw_ns(void);       /* absolute monotonic counter (mouse timestamps) */
void ke_timer_resolution_begin(void);     /* Win32 timeBeginPeriod(1); no-op elsewhere     */
void ke_sleep_ms(uint32_t ms);            /* scheduler sleep (Win32 Sleep)                 */
/* Emulation pause (app backgrounding): while paused ke_now_ns() stands still, and after
 * resume it continues from the paused value, so no virtual time passes (POSIX only). */
void ke_time_pause(void);
void ke_time_resume(void);

/* ---- threads --------------------------------------------------------------------------- */
typedef struct KeThread KeThread;
typedef unsigned (*KeThreadFn)(void *arg);
#define KE_THREAD_HELD 1u        /* created but not running until ke_thread_start()        */
#define KE_THREAD_GAME_STACK 2u  /* POSIX: run on the stack from ke_platform_set_game_stack */
KeThread *ke_thread_create(KeThreadFn fn, void *arg, size_t stack_size, unsigned flags);
void ke_thread_start(KeThread *thread);
int ke_thread_join_ms(KeThread *thread, uint32_t ms);   /* 1 = the thread has finished     */
int ke_thread_terminate(KeThread *thread);               /* 1 = killed (Win32 only)        */
void ke_thread_close(KeThread *thread);
KeThread *ke_thread_adopt_current(void);                 /* handle for the calling thread  */
void ke_thread_set_time_critical(void);                  /* calling thread                  */
void ke_platform_set_game_stack(void *base, size_t size);

/* Stop `target` at an instruction boundary and call fn(pc, arg, &redirect) (pc is UINTPTR_MAX
 * when the target's context could not be read). Win32: fn runs
 * on the calling thread while the target is suspended; setting redirect->fn makes the
 * target continue at redirect->fn(redirect->arg) (exit() inside an ISR). POSIX: fn runs on
 * the target thread inside a signal handler, redirect is ignored. Returns fn's result, or
 * -1 when the target could not be stopped (or did not answer in time). */
typedef struct KeRedirect { void (*fn)(int); int arg; } KeRedirect;
typedef int (*KeInterruptFn)(uintptr_t pc, void *arg, KeRedirect *redirect);
int ke_thread_interrupt(KeThread *target, KeInterruptFn fn, void *arg);
/* 1 when async interrupt callbacks run on the interrupted thread itself (POSIX). */
int ke_thread_interrupt_runs_on_target(void);
/* POSIX: the callback is about to leave by longjmp (exit() inside an ISR); tells the
 * waiting interrupter that it is done. No-op on Win32. */
void ke_thread_interrupt_leaving(void);
/* Executable range of the module holding the game code (interruptible program counters). */
void ke_platform_code_range(uintptr_t *lo, uintptr_t *hi);
/* Diagnostics: log the stopped thread's program counter and frame chain. */
void ke_platform_log_thread_backtrace(KeThread *thread, const char *why);

/* ---- memory ---------------------------------------------------------------------------- */
/* Map [addr, addr+size) read/write at exactly addr (DOS memory). 0 = ok; on failure the
 * address space around addr is logged. */
int ke_platform_map_fixed(uintptr_t addr, size_t size);
/* Fault diagnostics for game code (port/vhw/fault.c's classifier). */
void ke_platform_fault_init(int first, void (*describe)(char *out, size_t cap, int is_write,
                                                        uintptr_t address, uintptr_t pc));

/* ---- files ----------------------------------------------------------------------------- */
int ke_platform_exe_dir(char *out, size_t cap);
int ke_platform_cwd(char *out, size_t cap);
int ke_platform_chdir(const char *path);
int ke_platform_full_path(const char *path, char *out, size_t cap);
int ke_platform_is_absolute(const char *path);
int ke_platform_path_exists(const char *path);
int ke_platform_is_dir(const char *path);
int ke_platform_mkdir(const char *path);                 /* 1 = exists now as a directory */
/* Per-user directories: Win32 %APPDATA% / %LOCALAPPDATA% based; Android internal storage. */
int ke_platform_config_base(char *out, size_t cap);      /* ".../Krypton Egg" parent       */
int ke_platform_local_data_base(char *out, size_t cap);
#if !defined(_WIN32)
/* POSIX hosts without an executable directory or home (Android): set by the host first. */
void ke_platform_set_exe_dir(const char *dir);
void ke_platform_set_user_base(const char *dir);
#endif
/* fopen with share-deny-none (Win32) / case-insensitive leaf lookup (POSIX). */
#include <stdio.h>
FILE *ke_platform_fopen(const char *path, const char *mode);

#endif
