/* gamethread.c - runs the historical main() on its own thread (SDL keeps the main thread).
 *
 * Watcom exit()/atexit() are replaced (watcom_compat.h): ke_exit() runs the game's atexit
 * handlers in LIFO order on the game thread, exactly as the Watcom runtime would before
 * returning to DOS, then unwinds to the thread entry with longjmp. When exit() is reached
 * inside an interrupt handler that runs on the IRQ thread, the game thread is redirected
 * to perform the exit itself (vpic_isr_exit_redirect).
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../platform/ke_platform.h"
#include "ke_port.h"
#include "../vhw/vhw.h"

#define MAX_ATEXIT 32
extern short timer_ok;
extern unsigned timer_delta;
extern int timer_enabled09;
static void (*atexit_fns[MAX_ATEXIT])(void);
static int atexit_count;
static jmp_buf exit_jump;
static ke_atomic_t finished;
static ke_atomic_t quit_requested;
static int exit_code;
static KeThread *game_thread;
static ke_thread_id game_thread_id;
static ke_atomic_t exiting;
static ke_atomic_t pause_requested, pause_parked;
static KeEvent *pause_wake;

int ke_on_game_thread(void) { return ke_thread_current_id() == game_thread_id; }
KeThread *ke_game_thread_handle(void) { return game_thread; }
int ke_game_thread_finished(void) { return finished; }
int ke_game_exit_code(void) { return exit_code; }
void ke_request_quit(void) { ke_atomic_exchange(&quit_requested, 1); }
int ke_quit_requested(void) { return quit_requested; }

int ke_atexit(void (*fn)(void))
{
    if (atexit_count >= MAX_ATEXIT)
        return -1;
    atexit_fns[atexit_count++] = fn;
    return 0;
}

static void ke_exit_impl(int code, int log_exit, int run_atexit) __attribute__((noreturn));

static void ke_exit_impl(int code, int log_exit, int run_atexit)
{
    if (!ke_on_game_thread()) {
        /* exit() reached from an ISR running on the IRQ thread: make the game thread exit. */
        vpic_isr_exit_redirect(code);
    } else {
        /* exit() inside an ISR delivered on the game thread itself (POSIX signal) */
        vpic_isr_exit_on_game_thread();
    }
    vhw_reset_nesting();
    if (ke_atomic_exchange(&exiting, 1) == 0) {
        exit_code = code;
        if (log_exit)
            ke_log(KE_LOG_INFO, "game", "exit(%d) called; running %d atexit handlers", code,
                   atexit_count);
        if (run_atexit) {
            while (atexit_count > 0)
                atexit_fns[--atexit_count]();
        }
        fflush(stdout);
    }
    longjmp(exit_jump, 1);
}

void ke_exit(int code)
{
    ke_exit_impl(code, 1, 1);
}

void ke_check_quit(void)
{
    if (quit_requested && ke_on_game_thread() && !exiting) {
        int log_quit = 1;
        if (log_quit)
            ke_log(KE_LOG_INFO, "game", "window closed: unwinding the game thread");
        /* Host window shutdown owns virtual-device cleanup; game atexit handlers may
         * wait for emulated ticks and can outlast the host's quit grace period. */
        ke_exit_impl(0, log_quit, 0);
    }
}

void ke_stop_game_from_fault(const char *why)
{
    ke_log(KE_LOG_ERROR, "game", "game thread stopped: %s", why);
    exit_code = 3;
    ke_atomic_exchange(&exiting, 1); /* no atexit handlers after a fault */
    longjmp(exit_jump, 2);
}

/* ---- emulation pause (Android backgrounding) ------------------------------------------
 * The host asks for a pause; the game thread parks at its next virtual-PC boundary
 * (vhw_leave, the memory-poll yield or a blocking BIOS wait: never inside a device lock
 * or an interrupt handler) until the host resumes it. Virtual time is frozen separately by
 * ke_time_pause() so no emulated time passes while the app is in the background. */
void ke_pause_game(void)
{
    if (!pause_wake)
        pause_wake = ke_event_create();
    ke_atomic_exchange(&pause_requested, 1);
}

void ke_resume_game(void)
{
    ke_atomic_exchange(&pause_requested, 0);
    if (pause_wake)
        ke_event_set(pause_wake);
}

int ke_game_paused(void) { return (int)ke_atomic_load(&pause_parked); }

void ke_check_pause(void)
{
    if (!pause_requested || !ke_on_game_thread() || exiting)
        return;
    ke_atomic_exchange(&pause_parked, 1);
    ke_log(KE_LOG_INFO, "game", "emulation paused");
    while (ke_atomic_load(&pause_requested) && !quit_requested)
        ke_event_wait_ms(pause_wake, 100);
    ke_atomic_exchange(&pause_parked, 0);
    ke_log(KE_LOG_INFO, "game", "emulation resumed");
    if (quit_requested)
        ke_check_quit();
}

static unsigned game_thread_main(void *unused)
{
    (void)unused;
    vhw_bind_game_thread();
    if (setjmp(exit_jump) == 0) {
        ke_log(KE_LOG_INFO, "game", "entering historical main()");
        ke_game_main();
        ke_log(KE_LOG_INFO, "game", "historical main() returned");
        ke_exit(0);
    }
    if (getenv("KE_TIMER_DIAG"))
        ke_log(KE_LOG_INFO, "timer", "timer diagnostic: timer_ok=%d timer_delta=%u timer_enabled09=%d",
               timer_ok, timer_delta, timer_enabled09);
    ke_log(KE_LOG_INFO, "game", "game thread finished (exit code %d)", exit_code);
    ke_atomic_exchange(&finished, 1);
    return 0;
}

static unsigned game_thread_entry(void *unused)
{
    game_thread_id = ke_thread_current_id();
    return game_thread_main(unused);
}

int ke_game_thread_start(void)
{
    /* 4 MiB stack; DOS/4GW gave the game far less, deep recursion is not expected. On
     * Android the stack comes from the low game region (ke_platform_set_game_stack). */
    game_thread = ke_thread_create(game_thread_entry, NULL, 4u << 20,
                                   KE_THREAD_HELD | KE_THREAD_GAME_STACK);
    if (!game_thread)
        return -1;
    vhw_start_devices();
    ke_thread_start(game_thread);
    return 0;
}

/* Lockstep runner: run `entry` (the port's or the original's main) on the calling thread
 * with the same exit()/atexit() unwinding as the game thread. Returns the exit code. */
int ke_game_run_here(void (*entry)(void))
{
    ke_game_thread_adopt();
    if (setjmp(exit_jump) == 0) {
        ke_log(KE_LOG_INFO, "game", "entering main() on the lockstep thread");
        entry();
        ke_exit(0);
    }
    ke_atomic_exchange(&finished, 1);
    return exit_code;
}

/* Tests (ke_oracle): make the calling thread the "game thread" without running main(). */
void ke_game_thread_adopt(void)
{
    game_thread = ke_thread_adopt_current();
    game_thread_id = ke_thread_current_id();
    vhw_bind_game_thread();
}

/* Diagnostics: log the game thread's program counter and frame chain (Win32: EBP chain,
 * resolve with `addr2line -f -e ke_sdl3.exe ADDR`). */
void ke_log_game_backtrace(const char *why)
{
    if (!game_thread || finished)
        return;
    ke_platform_log_thread_backtrace(game_thread, why);
}
