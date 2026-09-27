/* watcom_compat.h - force-included (gcc -include) into every historical src/*.c.
 *
 * Maps Watcom C/C++32 10.0 (flat model, -3s) language extensions onto gcc i686.
 * Nothing here changes game semantics; it only makes the historical text compile.
 * See docs/port/architecture.md "Compile issue classes".
 */
#ifndef KE_WATCOM_COMPAT_H
#define KE_WATCOM_COMPAT_H

/* Flat 32-bit model: near/far/huge pointer qualifiers carry no meaning. */
#define __far
#define __near
#define __huge
#define _far
#define _near
#define far
#define near
/* __interrupt: Watcom emits an IRET frame. The virtual PIC (port/vhw/pic.c) calls
 * handlers as plain cdecl functions, so the keyword is dropped. */
#define __interrupt
#define _interrupt
#define __loadds
#define __saveregs
/* Watcom -3s is stack based, caller cleans: gcc cdecl is ABI compatible for the
 * int/pointer signatures used here. */
#define __watcall
/* Real-mode low memory below 64 KiB (IVT, BIOS data area) cannot be mapped in a Windows
 * process; the few historical absolute addresses there are wrapped in KE_LOWMEM() (a port
 * edit, marked PORT:) and land in the virtual PC's shadow copy (port/vhw/lowmem.c).
 * Addresses from 64 KiB to 1 MiB + 64 KiB are identity mapped and need no wrapping. */
extern unsigned char ke_lowmem_shadow[];
#define KE_LOWMEM(address) ((int)(ke_lowmem_shadow + (address)))

/* Watcom runtime exit()/atexit(): run the game's handlers on the game thread, then unwind
 * the game thread instead of terminating the process (port/host/gamethread.c). */
#define exit ke_exit
#define atexit ke_atexit
/* getenv() reads the virtual DOS environment, not the host's (the game treats WINDIR as
 * "running under Windows" and reads BLASTER): port/host/clib.c. */
#define getenv ke_getenv

#endif
