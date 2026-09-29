/* platform_posix.c - POSIX (Android bionic / Linux glibc) implementation of ke_platform.h.
 *
 * Mirrors platform_win32.c. The one structural difference is asynchronous interrupt
 * delivery: Win32 freezes the game thread (SuspendThread) and runs the handler on the IRQ
 * thread; here the IRQ thread sends a real-time signal to the game thread and the handler
 * runs on the game thread inside the signal handler, after the same checks (IF=1, not in a
 * virtual-PC service, no handler running, program counter inside the game module or at the
 * scheduler's safe wait) - docs/android/architecture.md, "Asynchronous interrupts".
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include "ke_platform.h"
#include "../include/ke_port.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

/* ---- mutex / once / thread id ---------------------------------------------------------- */
void ke_mutex_init(KeMutex *m)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
}

void ke_once(KeOnce *once, void (*fn)(void)) { pthread_once(&once->once, fn); }

ke_thread_id ke_thread_current_id(void) { return (ke_thread_id)syscall(SYS_gettid); }

/* ---- time ------------------------------------------------------------------------------ */
static uint64_t raw_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t time_origin_ns;
static uint64_t time_paused_total_ns;   /* virtual time removed by ke_time_pause/resume */
static uint64_t time_pause_start_ns;
static volatile int time_paused;
static pthread_mutex_t time_lock = PTHREAD_MUTEX_INITIALIZER;

uint64_t ke_now_ns(void)
{
    uint64_t now, result;
    if (!time_origin_ns)
        __atomic_compare_exchange_n(&time_origin_ns, &(uint64_t){0}, raw_now_ns(), 0,
                                    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    now = raw_now_ns();
    if (!time_paused && !time_paused_total_ns)
        return now - time_origin_ns;
    pthread_mutex_lock(&time_lock);
    if (time_paused)
        now = time_pause_start_ns;
    result = now - time_origin_ns - time_paused_total_ns;
    pthread_mutex_unlock(&time_lock);
    return result;
}

void ke_time_pause(void)
{
    pthread_mutex_lock(&time_lock);
    if (!time_paused) {
        time_pause_start_ns = raw_now_ns();
        time_paused = 1;
    }
    pthread_mutex_unlock(&time_lock);
}

void ke_time_resume(void)
{
    pthread_mutex_lock(&time_lock);
    if (time_paused) {
        time_paused_total_ns += raw_now_ns() - time_pause_start_ns;
        time_paused = 0;
    }
    pthread_mutex_unlock(&time_lock);
}

static void sleep_raw_ns(uint64_t ns)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ull);
    ts.tv_nsec = (long)(ns % 1000000000ull);
    nanosleep(&ts, NULL);   /* an interrupted sleep is a shorter wait, as on DOS */
}

/* As the Win32 version: coarse sleep to 0.5 ms before the deadline, then spin. The deadline
 * is host time, so a paused virtual clock cannot stall a sleeping thread. */
void ke_sleep_ns(uint64_t ns)
{
    uint64_t end = raw_now_ns() + ns;
    if (ns >= 1500000)
        sleep_raw_ns(ns - 500000);
    while (raw_now_ns() < end)
        ke_cpu_relax();
}

uint64_t ke_monotonic_raw_ns(void) { return raw_now_ns(); }
void ke_timer_resolution_begin(void) {}
void ke_sleep_ms(uint32_t ms) { sleep_raw_ns((uint64_t)ms * 1000000ull); }

void ke_poll_wait_100us(void) { sleep_raw_ns(100000); }

/* ---- events ---------------------------------------------------------------------------- */
struct KeEvent {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int signalled;
};

KeEvent *ke_event_create(void)
{
    KeEvent *event = (KeEvent *)calloc(1, sizeof *event);
    pthread_condattr_t attr;
    if (!event)
        return NULL;
    pthread_mutex_init(&event->lock, NULL);
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&event->cond, &attr);
    pthread_condattr_destroy(&attr);
    return event;
}

void ke_event_destroy(KeEvent *event)
{
    if (!event)
        return;
    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->lock);
    free(event);
}

void ke_event_set(KeEvent *event)
{
    pthread_mutex_lock(&event->lock);
    event->signalled = 1;
    pthread_cond_signal(&event->cond);
    pthread_mutex_unlock(&event->lock);
}

static int event_wait_until(KeEvent *event, const struct timespec *deadline)
{
    int got;
    pthread_mutex_lock(&event->lock);
    while (!event->signalled) {
        int rc = deadline ? pthread_cond_timedwait(&event->cond, &event->lock, deadline)
                          : pthread_cond_wait(&event->cond, &event->lock);
        if (rc == ETIMEDOUT)
            break;
    }
    got = event->signalled;
    event->signalled = 0;
    pthread_mutex_unlock(&event->lock);
    return got;
}

static struct timespec deadline_after_ns(uint64_t ns)
{
    uint64_t at = raw_now_ns() + ns;
    struct timespec ts;
    ts.tv_sec = (time_t)(at / 1000000000ull);
    ts.tv_nsec = (long)(at % 1000000000ull);
    return ts;
}

int ke_event_wait_ms(KeEvent *event, uint32_t ms)
{
    struct timespec deadline;
    if (ms == KE_WAIT_INFINITE)
        return event_wait_until(event, NULL);
    deadline = deadline_after_ns((uint64_t)ms * 1000000ull);
    return event_wait_until(event, &deadline);
}

struct KeTimer { int unused; };
KeTimer *ke_timer_create(void) { return (KeTimer *)calloc(1, sizeof(KeTimer)); }
void ke_timer_destroy(KeTimer *timer) { free(timer); }

int ke_event_wait_timer_ns(KeEvent *event, KeTimer *timer, uint64_t ns)
{
    struct timespec deadline = deadline_after_ns(ns ? ns : 100);
    (void)timer;
    return event_wait_until(event, &deadline);
}

/* ---- threads --------------------------------------------------------------------------- */
struct KeThread {
    pthread_t pthread;
    volatile ke_thread_id tid;
    KeThreadFn fn;
    void *arg;
    KeEvent *started;      /* KE_THREAD_HELD gate */
    KeEvent *finished;
    volatile int done;
    int joined, adopted;
};

static void *game_stack_base;
static size_t game_stack_size;

void ke_platform_set_game_stack(void *base, size_t size)
{
    game_stack_base = base;
    game_stack_size = size;
}

static void *thread_trampoline(void *parameter)
{
    KeThread *thread = (KeThread *)parameter;
    thread->tid = ke_thread_current_id();
    if (thread->started)
        ke_event_wait_ms(thread->started, KE_WAIT_INFINITE);
    thread->fn(thread->arg);
    thread->done = 1;
    ke_event_set(thread->finished);
    return NULL;
}

KeThread *ke_thread_create(KeThreadFn fn, void *arg, size_t stack_size, unsigned flags)
{
    pthread_attr_t attr;
    KeThread *thread = (KeThread *)calloc(1, sizeof *thread);
    int rc;
    if (!thread)
        return NULL;
    thread->fn = fn;
    thread->arg = arg;
    thread->finished = ke_event_create();
    if (flags & KE_THREAD_HELD)
        thread->started = ke_event_create();
    pthread_attr_init(&attr);
    if ((flags & KE_THREAD_GAME_STACK) && game_stack_base)
        pthread_attr_setstack(&attr, game_stack_base, game_stack_size);
    else if (stack_size)
        pthread_attr_setstacksize(&attr, stack_size < 65536 ? 65536 : stack_size);
    rc = pthread_create(&thread->pthread, &attr, thread_trampoline, thread);
    pthread_attr_destroy(&attr);
    if (rc) {
        ke_event_destroy(thread->finished);
        ke_event_destroy(thread->started);
        free(thread);
        return NULL;
    }
    return thread;
}

void ke_thread_start(KeThread *thread)
{
    if (thread->started)
        ke_event_set(thread->started);
}

int ke_thread_join_ms(KeThread *thread, uint32_t ms)
{
    if (thread->adopted)
        return 0;
    if (!thread->done && !ke_event_wait_ms(thread->finished, ms))
        return thread->done;
    if (!thread->joined) {
        pthread_join(thread->pthread, NULL);
        thread->joined = 1;
    }
    return 1;
}

int ke_thread_terminate(KeThread *thread)
{
    (void)thread;
    return 0;               /* POSIX has no safe equivalent of TerminateThread */
}

void ke_thread_close(KeThread *thread)
{
    if (!thread)
        return;
    if (!thread->adopted && !thread->joined) {
        if (thread->done) {
            pthread_join(thread->pthread, NULL);
        } else {
            pthread_detach(thread->pthread);
            return;         /* still running: its KeThread stays valid */
        }
    }
    ke_event_destroy(thread->finished);
    ke_event_destroy(thread->started);
    free(thread);
}

KeThread *ke_thread_adopt_current(void)
{
    KeThread *thread = (KeThread *)calloc(1, sizeof *thread);
    if (!thread)
        return NULL;
    thread->pthread = pthread_self();
    thread->tid = ke_thread_current_id();
    thread->adopted = 1;
    thread->finished = ke_event_create();
    return thread;
}

void ke_thread_set_time_critical(void)
{
    /* Android allows an app thread down to nice -19 (THREAD_PRIORITY_URGENT_AUDIO). */
    if (setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), -19) != 0)
        setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), -10);
}

/* ---- asynchronous interrupts ----------------------------------------------------------- */
static int irq_signal;
static KeInterruptFn irq_request_fn;
static void *irq_request_arg;
static volatile uint64_t irq_request_seq, irq_done_seq;
static volatile int irq_result;
static sem_t irq_done;
static pthread_once_t irq_once = PTHREAD_ONCE_INIT;
static __thread volatile int in_interrupt_handler;

static uintptr_t context_pc(void *uctx)
{
    ucontext_t *uc = (ucontext_t *)uctx;
#if defined(__aarch64__)
    return (uintptr_t)uc->uc_mcontext.pc;
#elif defined(__x86_64__)
    return (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__i386__)
    return (uintptr_t)uc->uc_mcontext.gregs[REG_EIP];
#elif defined(__arm__)
    return (uintptr_t)uc->uc_mcontext.arm_pc;
#else
    (void)uc;
    return 0;
#endif
}

static void irq_signal_handler(int sig, siginfo_t *info, void *uctx)
{
    int saved_errno = errno;
    uint64_t seq = irq_request_seq;
    KeRedirect redirect = {0, 0};
    (void)sig;
    (void)info;
    if (seq != irq_done_seq && irq_request_fn) {
        in_interrupt_handler = 1;
        irq_result = irq_request_fn(context_pc(uctx), irq_request_arg, &redirect);
        in_interrupt_handler = 0;
        __atomic_store_n(&irq_done_seq, seq, __ATOMIC_SEQ_CST);
        sem_post(&irq_done);
    }
    errno = saved_errno;
}

static void irq_signal_init(void)
{
    struct sigaction sa;
    irq_signal = SIGRTMIN + 5;
    sem_init(&irq_done, 0, 0);
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = irq_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(irq_signal, &sa, NULL);
}

int ke_thread_interrupt_runs_on_target(void) { return 1; }

void ke_thread_interrupt_leaving(void)
{
    if (in_interrupt_handler) {
        in_interrupt_handler = 0;
        irq_result = 1;
        __atomic_store_n(&irq_done_seq, irq_request_seq, __ATOMIC_SEQ_CST);
        sem_post(&irq_done);
    }
}

int ke_thread_interrupt(KeThread *target, KeInterruptFn fn, void *arg)
{
    uint64_t seq, waited = 0;
    pthread_once(&irq_once, irq_signal_init);
    while (sem_trywait(&irq_done) == 0)
        ;                           /* drop a late answer to an abandoned request */
    irq_request_fn = fn;
    irq_request_arg = arg;
    seq = irq_request_seq + 1;
    __atomic_store_n(&irq_request_seq, seq, __ATOMIC_SEQ_CST);
    if (pthread_kill(target->pthread, irq_signal) != 0)
        return -1;
    /* The handler (the game's ISR) may legitimately run for a frame: IRQ0 waits for the
     * vertical retrace inside the handler. */
    while (__atomic_load_n(&irq_done_seq, __ATOMIC_SEQ_CST) != seq) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_nsec += 20000000;
        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000;
        }
        if (sem_timedwait(&irq_done, &deadline) != 0 && errno != EINTR)
            waited += 20;
        if (target->done || waited >= 2000)
            return -1;
    }
    return irq_result;
}

/* Executable PT_LOAD range of the shared object (or executable) that holds this code. */
struct CodeRangeSearch { uintptr_t probe, lo, hi; };

static int code_range_callback(struct dl_phdr_info *info, size_t size, void *data)
{
    struct CodeRangeSearch *search = (struct CodeRangeSearch *)data;
    int i;
    (void)size;
    for (i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        uintptr_t lo, hi;
        if (ph->p_type != PT_LOAD || !(ph->p_flags & PF_X))
            continue;
        lo = (uintptr_t)info->dlpi_addr + ph->p_vaddr;
        hi = lo + ph->p_memsz;
        if (search->probe >= lo && search->probe < hi) {
            search->lo = lo;
            search->hi = hi;
            return 1;
        }
    }
    return 0;
}

void ke_platform_code_range(uintptr_t *lo, uintptr_t *hi)
{
    struct CodeRangeSearch search;
    search.probe = (uintptr_t)(void *)ke_platform_code_range;
    search.lo = search.hi = 0;
    dl_iterate_phdr(code_range_callback, &search);
    *lo = search.lo;
    *hi = search.hi;
}

void ke_platform_log_thread_backtrace(KeThread *thread, const char *why)
{
    (void)thread;
    ke_log(KE_LOG_INFO, "game", "game thread (%s): no backtrace on this platform", why);
}

/* ---- memory ---------------------------------------------------------------------------- */
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static void log_mappings_near(uintptr_t addr, size_t size)
{
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[512];
    if (!maps)
        return;
    while (fgets(line, sizeof line, maps)) {
        unsigned long lo, hi;
        if (sscanf(line, "%lx-%lx", &lo, &hi) == 2 && hi > addr - (addr > 0x100000 ? 0x100000 : addr) &&
            lo < addr + size + 0x100000) {
            line[strcspn(line, "\n")] = 0;
            ke_log(KE_LOG_ERROR, "lowmem", "  %s", line);
        }
    }
    fclose(maps);
}

int ke_platform_map_fixed(uintptr_t addr, size_t size)
{
    void *p = mmap((void *)addr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != MAP_FAILED && (uintptr_t)p != addr) {
        munmap(p, size);    /* an old kernel treated the address as a hint */
        p = MAP_FAILED;
        errno = EEXIST;
    }
    if (p == MAP_FAILED) {
        ke_log(KE_LOG_ERROR, "lowmem", "cannot map linear %05lX..%06lX: %s; mappings nearby:",
               (unsigned long)addr, (unsigned long)(addr + size - 1), strerror(errno));
        log_mappings_near(addr, size);
        return -1;
    }
    return 0;
}

static void (*fault_describe)(char *, size_t, int, uintptr_t, uintptr_t);
static struct sigaction previous_fault[32];
static char fault_text[256];

static void fault_handler(int sig, siginfo_t *info, void *uctx)
{
    uintptr_t pc = context_pc(uctx);
    if (sig == SIGSEGV || sig == SIGBUS) {
        /* The kernel does not report read/write for every architecture: say "access". */
        fault_describe(fault_text, sizeof fault_text, -1, (uintptr_t)info->si_addr, pc);
    } else {
        snprintf(fault_text, sizeof fault_text, "signal %d (code %d) at PC %lX", sig,
                 info->si_code, (unsigned long)pc);
    }
    ke_log(KE_LOG_ERROR, "fault", "%s thread: %s", ke_on_game_thread() ? "game" : "other",
           fault_text);
    if (ke_on_game_thread())
        ke_stop_game_from_fault(fault_text);    /* longjmp to the game thread's exit */
    /* Not ours: hand the signal to the previous handler (Android debuggerd/ART). */
    if (sig > 0 && sig < 32) {
        struct sigaction *old = &previous_fault[sig];
        if (old->sa_flags & SA_SIGINFO) {
            if (old->sa_sigaction) {
                old->sa_sigaction(sig, info, uctx);
                return;
            }
        } else if (old->sa_handler != SIG_DFL && old->sa_handler != SIG_IGN) {
            old->sa_handler(sig);
            return;
        }
        sigaction(sig, old, NULL);
        raise(sig);
    }
}

void ke_platform_fault_init(int first, void (*describe)(char *, size_t, int, uintptr_t, uintptr_t))
{
    static const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE};
    struct sigaction sa;
    size_t i;
    (void)first;
    fault_describe = describe;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = fault_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < sizeof signals / sizeof signals[0]; i++)
        sigaction(signals[i], &sa, &previous_fault[signals[i]]);
}

/* ---- files ----------------------------------------------------------------------------- */
static char exe_dir_override[KE_MAX_PATH];

/* Android has no executable directory: the host sets the app's data directory. */
void ke_platform_set_exe_dir(const char *dir)
{
    snprintf(exe_dir_override, sizeof exe_dir_override, "%s", dir ? dir : "");
}

int ke_platform_exe_dir(char *out, size_t cap)
{
    char path[KE_MAX_PATH];
    ssize_t n;
    char *slash;
    if (exe_dir_override[0])
        return snprintf(out, cap, "%s", exe_dir_override) < (int)cap;
    n = readlink("/proc/self/exe", path, sizeof path - 1);
    if (n <= 0)
        return ke_platform_cwd(out, cap);
    path[n] = 0;
    slash = strrchr(path, '/');
    if (slash)
        *slash = 0;
    return snprintf(out, cap, "%s", path) < (int)cap;
}

int ke_platform_cwd(char *out, size_t cap) { return getcwd(out, cap) != NULL; }
int ke_platform_chdir(const char *path) { return chdir(path); }
int ke_platform_is_absolute(const char *path) { return path[0] == '/'; }

int ke_platform_full_path(const char *path, char *out, size_t cap)
{
    char cwd[KE_MAX_PATH];
    int n;
    if (ke_platform_is_absolute(path))
        n = snprintf(out, cap, "%s", path);
    else if (getcwd(cwd, sizeof cwd))
        n = snprintf(out, cap, "%s/%s", cwd, path);
    else
        return 0;
    return n > 0 && (size_t)n < cap;
}

int ke_platform_path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int ke_platform_is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int ke_platform_mkdir(const char *path)
{
    if (ke_platform_is_dir(path))
        return 1;
    if (mkdir(path, 0700) == 0)
        return 1;
    return ke_platform_is_dir(path);
}

static char user_base[KE_MAX_PATH];

/* Android: the app's internal storage (SDL_GetAndroidInternalStoragePath), set by the host
 * before the configuration is loaded. Elsewhere: $XDG_CONFIG_HOME or ~/.config. */
void ke_platform_set_user_base(const char *dir)
{
    snprintf(user_base, sizeof user_base, "%s", dir ? dir : "");
}

int ke_platform_config_base(char *out, size_t cap)
{
    const char *env;
    if (user_base[0])
        return snprintf(out, cap, "%s", user_base) < (int)cap;
    env = getenv("XDG_CONFIG_HOME");
    if (env && *env)
        return snprintf(out, cap, "%s", env) < (int)cap;
    env = getenv("HOME");
    if (env && *env)
        return snprintf(out, cap, "%s/.config", env) < (int)cap;
    return 0;
}

int ke_platform_local_data_base(char *out, size_t cap)
{
    const char *env;
    if (user_base[0])
        return snprintf(out, cap, "%s", user_base) < (int)cap;
    env = getenv("XDG_DATA_HOME");
    if (env && *env)
        return snprintf(out, cap, "%s", env) < (int)cap;
    env = getenv("HOME");
    if (env && *env)
        return snprintf(out, cap, "%s/.local/share", env) < (int)cap;
    return 0;
}

/* fopen without the libc symbol (port/host/clib.c defines fopen for the game) and with a
 * case-insensitive leaf lookup: DOS file names are case-insensitive, Android's are not. */
static FILE *open_exact(const char *path, const char *mode)
{
    int flags, fd, plus = strchr(mode, '+') != NULL;
    FILE *file;
    switch (mode[0]) {
    case 'r': flags = plus ? O_RDWR : O_RDONLY; break;
    case 'w': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC; break;
    case 'a': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND; break;
    default: errno = EINVAL; return NULL;
    }
    fd = open(path, flags | O_CLOEXEC, 0600);
    if (fd < 0)
        return NULL;
    file = fdopen(fd, mode);
    if (!file)
        close(fd);
    return file;
}

FILE *ke_platform_fopen(const char *path, const char *mode)
{
    FILE *file = open_exact(path, mode);
    char dir[KE_MAX_PATH], found[KE_MAX_PATH];
    const char *leaf, *slash;
    DIR *d;
    struct dirent *entry;
    if (file || errno != ENOENT || !path)
        return file;
    slash = strrchr(path, '/');
    leaf = slash ? slash + 1 : path;
    if (slash)
        snprintf(dir, sizeof dir, "%.*s", (int)(slash - path), path);
    else
        snprintf(dir, sizeof dir, ".");
    d = opendir(dir[0] ? dir : "/");
    if (!d) {
        errno = ENOENT;
        return NULL;
    }
    found[0] = 0;
    while ((entry = readdir(d)) != NULL)
        if (strcasecmp(entry->d_name, leaf) == 0) {
            snprintf(found, sizeof found, "%s/%s", dir, entry->d_name);
            break;
        }
    closedir(d);
    if (!found[0]) {
        errno = ENOENT;
        return NULL;
    }
    return open_exact(found, mode);
}
