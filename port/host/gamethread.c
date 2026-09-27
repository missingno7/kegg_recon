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
#include <windows.h>
#include "ke_port.h"
#include "../vhw/vhw.h"

#define MAX_ATEXIT 32
extern short timer_ok;
extern unsigned timer_delta;
extern int timer_enabled09;
static void (*atexit_fns[MAX_ATEXIT])(void);
static int atexit_count;
static jmp_buf exit_jump;
static volatile LONG finished;
static volatile LONG quit_requested;
static int exit_code;
static HANDLE game_thread;
static DWORD game_thread_id;
static volatile LONG exiting;

int ke_on_game_thread(void) { return GetCurrentThreadId() == game_thread_id; }
void *ke_game_thread_handle(void) { return game_thread; }
int ke_game_thread_finished(void) { return finished; }
int ke_game_exit_code(void) { return exit_code; }
void ke_request_quit(void) { InterlockedExchange(&quit_requested, 1); }
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
    }
    vhw_reset_nesting();
    if (InterlockedExchange(&exiting, 1) == 0) {
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
    InterlockedExchange(&exiting, 1); /* no atexit handlers after a fault */
    longjmp(exit_jump, 2);
}

static DWORD WINAPI game_thread_main(LPVOID unused)
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
    InterlockedExchange(&finished, 1);
    return 0;
}

int ke_game_thread_start(void)
{
    /* 4 MiB stack; DOS/4GW gave the game far less, deep recursion is not expected. */
    game_thread = CreateThread(NULL, 4u << 20, game_thread_main, NULL, CREATE_SUSPENDED,
                               &game_thread_id);
    if (!game_thread)
        return -1;
    vhw_start_devices();
    ResumeThread(game_thread);
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
    InterlockedExchange(&finished, 1);
    return exit_code;
}

/* Tests (ke_oracle): make the calling thread the "game thread" without running main(). */
void ke_game_thread_adopt(void)
{
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &game_thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    game_thread_id = GetCurrentThreadId();
    vhw_bind_game_thread();
}

/* Diagnostics: log the game thread's EIP and EBP-chain return addresses (game code is built
 * with -O0, so frame pointers are intact). Resolve with `addr2line -f -e ke_sdl3.exe ADDR`. */
void ke_log_game_backtrace(const char *why)
{
    CONTEXT ctx;
    uint32_t ebp, frames[12];
    int n = 0, i;
    char text[256];
    int len;
    if (!game_thread || finished)
        return;
    if (SuspendThread(game_thread) == (DWORD)-1)
        return;
    memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(game_thread, &ctx)) {
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
    ResumeThread(game_thread);
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
