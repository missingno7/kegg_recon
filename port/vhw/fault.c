/* fault.c - diagnose faults of game code running on the virtual PC.
 *
 * Access violations and privileged instructions on the game thread are logged with the
 * faulting EIP (as an address inside ke_sdl3.exe: `addr2line -f -e ke_sdl3.exe 0xEIP`)
 * and the data address, classified (VGA window, real-mode low memory, null page), and the
 * game thread is unwound cleanly: the thread context is redirected to a trampoline that
 * calls ke_stop_game_from_fault(), so SDL shuts down normally and the log says where the
 * game stopped.
 */
#include <stdio.h>
#include <windows.h>
#include "vhw.h"
#include "../include/ke_port.h"

static char fault_text[256];

static void fault_trampoline(void) { ke_stop_game_from_fault(fault_text); }

static const char *classify(uintptr_t a)
{
    if (a < 0x10000) return "real-mode low memory/null page (needs KE_LOWMEM or a vhw service)";
    if (a >= 0xA0000 && a < 0xC0000) return "VGA window (must use vga_mem_read8/write8)";
    if (a < LOWMEM_BASE) return "real-mode memory below the mapped range";
    if (a < LOWMEM_END) return "mapped real-mode memory";
    return "host memory";
}

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
        snprintf(fault_text, sizeof fault_text,
                 "access violation (%s %08lX: %s) at EIP %08lX", rw ? "write" : "read",
                 (unsigned long)a, classify(a), (unsigned long)eip);
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

void vhw_fault_init(void)
{
    /* Lockstep runs original machine code whose emulation VEH (port/oracle) must see
     * privileged instructions first: this diagnostic handler goes last there. */
    AddVectoredExceptionHandler(vhw_lockstep ? 0 : 1, fault_handler);
}
