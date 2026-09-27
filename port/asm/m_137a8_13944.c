/* m_137a8_13944.c - C translation of asm/m_137a8_13944.asm (TASM 3.1, obj1 0x137a8..0x13944).
 *
 * Pilot translation for the "asm module" work packages: literal, instruction-order
 * preserving C over the virtual PC. Register-level quirks are kept on purpose and marked
 * QUIRK; each is covered by port/oracle (oracle_m_137a8.c) against the original bytes.
 *
 *   probe_cpu_environment   CPU generation / mode / IOPL probe
 *   clear_video_bytes       zero a range; plane/GC setup when it starts in the A000h window
 *   mov_mem                 overlap-aware move; VGA latch copy for video-to-video moves
 */
#include <stdint.h>
#include <string.h>
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

/* EQUs of the module */
#define VGA_A000_MEMORY_START 0xA0000u
#define VGA_B000_MEMORY_START 0xB0000u
#define VGA_SEQ_INDEX_PORT 0x3C4
#define VGA_GC_INDEX_PORT 0x3CE
#define VGA_ALL_PLANES_MASK 0x0F
#define VGA_GC_PLANAR_MODE_VALUE 0x40
#define VGA_SEQ_ALL_PLANES_COMMAND 0x0F02
#define VGA_GC_PLANAR_MODE_COMMAND 0x4005
#define VGA_GC_LATCH_COPY_MODE 0x41
#define VGA_GC_LATCH_COPY_COMMAND 0x4105
/* VGA_STATE STRUC offsets (byte-packed DisplayModeInfo, docs/types.md) */
#define VGA_STATE_RENDER_STATE 0x00
#define VGA_STATE_GC_MODE 0x60
#define VGA_STATE_SEQ_PLANE_MASK 0x61
#define CPU_TYPE_80386 0x386
#define CPU_TYPE_80486 0x486
#define CPU_MODE_PROTECTED 1

extern unsigned char vga_state[];           /* struct DisplayModeInfo in src/u_0dfc3.c */
unsigned outpw(int port, int value);

/* _DATA */
int cpu_type = -1;                          /* DD 0FFFFFFFFh */
int cpu_mode = -1;
int cpu_iopl = -1;

/* ---- memory helpers: the A000h..BFFFFh window is the virtual VGA ----------------------- */
static int is_vga(uint32_t a) { return a >= 0xA0000u && a < 0xC0000u; }

static uint8_t rd8(uint32_t a)
{
    return is_vga(a) ? vga_mem_read8(a) : *(volatile uint8_t *)(uintptr_t)a;
}

static void wr8(uint32_t a, uint8_t v)
{
    if (is_vga(a))
        vga_mem_write8(a, v);
    else
        *(volatile uint8_t *)(uintptr_t)a = v;
}

/* ---- probe_cpu_environment ------------------------------------------------------------ */
/* The host CPU is at least a 486 (EFLAGS.AC toggles), the program runs in protected mode
 * outside V86 (DOS/4GW), IOPL as DOS/4GW leaves it: 3 (HYPOTHESIS: value not observed on
 * the historical machine; only printed in the startup report). */
void probe_cpu_environment(void)
{
    vhw_enter();
    vcpu_cli();                               /* cli */
    cpu_type = CPU_TYPE_80386;
    cpu_type = CPU_TYPE_80486;                /* AC flag writable on every host CPU */
    cpu_mode = CPU_MODE_PROTECTED;            /* smsw: PE=1; EFLAGS.VM=0 */
    cpu_iopl = 3;
    vcpu_sti();                               /* QUIRK: always leaves IF=1 (sti) */
    vhw_leave();
}
void probe_cpu_environment_entry(void) { probe_cpu_environment(); }

/* ---- clear_video_bytes(destination, byte_count) -------------------------------------- */
void clear_video_bytes(uint32_t destination, uint32_t byte_count)
{
    uint32_t eax = 0, edi = destination, ecx, i;
    if (!((int32_t)edi >= (int32_t)VGA_B000_MEMORY_START) &&
        !((int32_t)edi < (int32_t)VGA_A000_MEMORY_START)) {
        if (vga_state[VGA_STATE_SEQ_PLANE_MASK] != VGA_ALL_PLANES_MASK) {
            vga_state[VGA_STATE_SEQ_PLANE_MASK] = VGA_ALL_PLANES_MASK;
            eax = VGA_SEQ_ALL_PLANES_COMMAND;  /* QUIRK: AX reused as the fill value */
            outpw(VGA_SEQ_INDEX_PORT, (int)eax);
        }
        if (vga_state[VGA_STATE_GC_MODE] != VGA_GC_PLANAR_MODE_VALUE) {
            vga_state[VGA_STATE_GC_MODE] = VGA_GC_PLANAR_MODE_VALUE;
            eax = VGA_GC_PLANAR_MODE_COMMAND;  /* QUIRK: idem */
            outpw(VGA_GC_INDEX_PORT, (int)eax);
        }
    }
    for (ecx = byte_count >> 2; ecx; ecx--, edi += 4)  /* rep stosd */
        for (i = 0; i < 4; i++)
            wr8(edi + i, (uint8_t)(eax >> (8 * i)));
    for (ecx = byte_count & 3; ecx; ecx--, edi++)      /* rep stosb */
        wr8(edi, (uint8_t)eax);
}
void clear_video_bytes_entry(uint32_t destination, uint32_t byte_count)
{
    clear_video_bytes(destination, byte_count);
}

/* ---- mov_mem(source, destination, byte_count) ----------------------------------------- */
/* rep movs with the direction flag: one element per iteration, read fully before write. */
static void rep_movs(uint32_t *esi, uint32_t *edi, uint32_t count, int size, int df)
{
    uint8_t tmp[4];
    int i;
    for (; count; count--) {
        for (i = 0; i < size; i++)
            tmp[i] = rd8(*esi + (uint32_t)i);
        for (i = 0; i < size; i++)
            wr8(*edi + (uint32_t)i, tmp[i]);
        *esi += df ? (uint32_t)-size : (uint32_t)size;
        *edi += df ? (uint32_t)-size : (uint32_t)size;
    }
}

void mov_mem(uint32_t source, uint32_t destination, uint32_t byte_count)
{
    uint32_t esi = source, edi = destination, ecx = byte_count;
    int df = 0;
    if (!((int32_t)esi >= (int32_t)edi)) {
        df = 1;                               /* std: copy from the last byte down */
        esi += ecx - 1;                       /* QUIRK: dword moves below then read up to 3 */
        edi += ecx - 1;                       /* bytes past the end of the range            */
    }
    if (!((int32_t)esi >= (int32_t)VGA_B000_MEMORY_START) &&
        !((int32_t)esi < (int32_t)VGA_A000_MEMORY_START)) {
        if (!((int32_t)edi >= (int32_t)VGA_B000_MEMORY_START) &&
            !((int32_t)edi < (int32_t)VGA_A000_MEMORY_START)) {
            /* video to video: latch copy, one byte moves all four planes */
            if (*(uint16_t *)(vga_state + VGA_STATE_RENDER_STATE) == 1 &&
                vga_state[VGA_STATE_GC_MODE] != VGA_GC_LATCH_COPY_MODE) {
                vga_state[VGA_STATE_GC_MODE] = VGA_GC_LATCH_COPY_MODE;
                outpw(VGA_GC_INDEX_PORT, VGA_GC_LATCH_COPY_COMMAND);
            }
            rep_movs(&esi, &edi, ecx, 1, df); /* copy_forward_tail with the full count */
            return;
        }
        goto destination_in_video_memory;     /* (label name kept from the source) */
    }
    if (!((int32_t)edi >= (int32_t)VGA_B000_MEMORY_START) &&
        !((int32_t)edi < (int32_t)VGA_A000_MEMORY_START)) {
destination_in_video_memory:
        if (vga_state[VGA_STATE_SEQ_PLANE_MASK] != VGA_ALL_PLANES_MASK) {
            vga_state[VGA_STATE_SEQ_PLANE_MASK] = VGA_ALL_PLANES_MASK;
            outpw(VGA_SEQ_INDEX_PORT, VGA_SEQ_ALL_PLANES_COMMAND);
        }
        if (vga_state[VGA_STATE_GC_MODE] != VGA_GC_PLANAR_MODE_VALUE) {
            vga_state[VGA_STATE_GC_MODE] = VGA_GC_PLANAR_MODE_VALUE;
            outpw(VGA_GC_INDEX_PORT, VGA_GC_PLANAR_MODE_COMMAND);
        }
    }
    rep_movs(&esi, &edi, ecx >> 2, 4, df);    /* rep movsd */
    rep_movs(&esi, &edi, byte_count & 3, 1, df); /* rep movsb */
}
void move_memory_bytes(uint32_t source, uint32_t destination, uint32_t byte_count)
{
    mov_mem(source, destination, byte_count);
}
