/* lowmem.c - real-mode memory of the virtual PC, identity mapped into the process.
 *
 * The game computes DOS addresses as segment << 4 and dereferences them directly, so DOS
 * memory must live at its own linear address below 1 MiB (+HMA). A Windows process owns
 * little of that range: the loader creates the process parameters, API-set map, initial
 * stack etc. at fixed places below 0xE0000 (the image has a fixed base, so the layout is
 * deterministic). ke_sdl3.exe therefore relaunches itself suspended and reserves
 * LOWMEM_BASE..LOWMEM_END in the child before its loader runs (host/main_sdl.c):
 *
 *   0x00000-0x0FFFF  never mappable: IVT + BIOS data area live in ke_lowmem_shadow;
 *                    historical source reaches them through KE_LOWMEM() (PORT: edits)
 *   0xE0000-0xFEFFF  conventional memory arena (DOS segments E000h-FEFFh) for DPMI 0100h
 *                    and INT 21h/48h: ~124 KiB instead of ~600 KiB (reported by the
 *                    game's startup memory line; the game needs < 64 KiB)
 *   0xFF000-0xFFFFF  system BIOS page: IRET (CFh) bytes; default real-mode vectors point at
 *                    F000:FF53, BIOS date at FFFF:0005
 *   0x100000-0x10FFFF HMA (zero)
 *   0xA0000-0xBFFFF  VGA window: NOT mapped by the port (other process data may live
 *                    there). Game code must reach video memory through vga_mem_read8/8
 *                    (asm translations); direct C dereferences are a porting bug.
 */
#include <string.h>
#include "../platform/ke_platform.h"
#include "vhw.h"
#include "../include/ke_port.h"

uint8_t ke_lowmem_shadow[0x10000];

#define ARENA_FIRST_SEG 0xE000u
#define ARENA_END_SEG 0xFF00u
/* one byte per paragraph would be 32 KiB; blocks are few, keep a small table instead */
typedef struct DosBlock { uint16_t seg, paras; } DosBlock;
static DosBlock blocks[64];
static int block_count;
static int mapped;

int lowmem_init(void)
{
    /* Win32: commit the range the parent reserved for us (main_sdl.c), or try to reserve it
     * now. POSIX: map it at its fixed address (Android: the game library's loader checked
     * that the range is free, docs/android/architecture.md "Memory"). */
    if (ke_platform_map_fixed(LOWMEM_BASE, LOWMEM_END - LOWMEM_BASE) != 0)
        return -1;
    mapped = 1;
    memset((void *)0xFF000, 0xCF, 0x1000);                     /* IRET everywhere */
    memcpy((void *)0xFFFF5, "01/01/94", 8);                   /* BIOS date       */
    memset(ke_lowmem_shadow, 0, sizeof ke_lowmem_shadow);
    /* BIOS data area */
    ke_lowmem_shadow[0x449] = 0x03;                            /* video mode      */
    ke_lowmem_shadow[0x44a] = 80;                              /* columns         */
    ke_lowmem_shadow[0x484] = 24;                              /* rows - 1        */
    ke_lowmem_shadow[0x413] = 0x80;                            /* 640 KiB (word)  */
    ke_lowmem_shadow[0x414] = 0x02;
    ke_lowmem_shadow[0x41a] = 0x1e;                            /* kbd head/tail   */
    ke_lowmem_shadow[0x41c] = 0x1e;
    ke_lowmem_shadow[0x480] = 0x1e;                            /* kbd buffer start */
    ke_lowmem_shadow[0x482] = 0x3e;                            /* kbd buffer end  */
    ke_lowmem_shadow[0x463] = 0xd4;                            /* CRTC 3D4h       */
    ke_lowmem_shadow[0x464] = 0x03;
    ke_log(KE_LOG_INFO, "lowmem", "linear %05X..%06X mapped (DOS arena %04X-%04X); IVT/BDA "
           "shadow at %p", LOWMEM_BASE, LOWMEM_END - 1, ARENA_FIRST_SEG, ARENA_END_SEG - 1,
           (void *)ke_lowmem_shadow);
    return 0;
}

void *lowmem_ptr(uint32_t linear)
{
    if (linear < 0x10000)
        return ke_lowmem_shadow + linear;
    return (void *)(uintptr_t)linear;
}

static uint16_t largest_gap(uint16_t *at)
{
    uint16_t cur = ARENA_FIRST_SEG, best = 0, best_at = ARENA_FIRST_SEG;
    int i;
    /* blocks kept sorted by segment */
    for (i = 0; i <= block_count; i++) {
        uint16_t next = (i < block_count) ? blocks[i].seg : ARENA_END_SEG;
        if (next > cur && (uint16_t)(next - cur) > best) {
            best = (uint16_t)(next - cur);
            best_at = cur;
        }
        if (i < block_count)
            cur = (uint16_t)(blocks[i].seg + blocks[i].paras);
    }
    if (at)
        *at = best_at;
    return best;
}

uint16_t lowmem_dos_largest(void) { return largest_gap(NULL); }

/* First fit. Returns 0 on success; else 8 (insufficient memory) with *largest set. */
int lowmem_dos_alloc(uint16_t paragraphs, uint16_t *segment, uint16_t *largest)
{
    uint16_t cur = ARENA_FIRST_SEG;
    int i, k;
    if (!mapped || block_count >= 64 || paragraphs == 0)
        goto fail;
    for (i = 0; i <= block_count; i++) {
        uint16_t next = (i < block_count) ? blocks[i].seg : ARENA_END_SEG;
        if (next >= cur && (uint32_t)(next - cur) >= paragraphs) {
            for (k = block_count; k > i; k--)
                blocks[k] = blocks[k - 1];
            blocks[i].seg = cur;
            blocks[i].paras = paragraphs;
            block_count++;
            memset((void *)((uintptr_t)cur << 4), 0, (size_t)paragraphs << 4);
            *segment = cur;
            return 0;
        }
        if (i < block_count)
            cur = (uint16_t)(blocks[i].seg + blocks[i].paras);
    }
fail:
    if (largest)
        *largest = largest_gap(NULL);
    return 8;
}

int lowmem_dos_free(uint16_t segment)
{
    int i;
    for (i = 0; i < block_count; i++)
        if (blocks[i].seg == segment) {
            for (; i < block_count - 1; i++)
                blocks[i] = blocks[i + 1];
            block_count--;
            return 0;
        }
    return 9; /* invalid memory block */
}
