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
#include "vhw.h"
#include "../include/ke_port.h"
#include "../platform/ke_platform.h"

static const char *classify(uintptr_t a)
{
    if (a < 0x10000) return "real-mode low memory/null page (needs KE_LOWMEM or a vhw service)";
    if (a >= 0xA0000 && a < 0xC0000) return "VGA window (must use vga_mem_read8/write8)";
    if (a < LOWMEM_BASE) return "real-mode memory below the mapped range";
    if (a < LOWMEM_END) return "mapped real-mode memory";
    return "host memory";
}

/* Text of an access fault; is_write < 0 when the host does not say which. */
static void describe_access(char *out, size_t cap, int is_write, uintptr_t address, uintptr_t pc)
{
    snprintf(out, cap, "access violation (%s %08lX: %s) at EIP %08lX",
             is_write < 0 ? "access" : is_write ? "write" : "read", (unsigned long)address,
             classify(address), (unsigned long)pc);
}

void vhw_fault_init(void)
{
    /* Lockstep runs original machine code whose emulation VEH (port/oracle) must see
     * privileged instructions first: this diagnostic handler goes last there. */
    ke_platform_fault_init(!vhw_lockstep, describe_access);
}
