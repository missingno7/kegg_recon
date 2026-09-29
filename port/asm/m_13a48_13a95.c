/* m_13a48_13a95.c - literal translation of asm/m_13a48_13a95.asm.
 *
 * write_dac_palette keeps the original EAX arithmetic and signed branches, including the
 * use of the full brightness DWORD in SUB EAX,EDI. copy_ds_to_es stores only the selector
 * word into the historical DWORD, leaving its high word untouched.
 */
#include <stdint.h>
#include "../include/ke_port.h"

#ifdef KE_ORACLE
#include "../oracle/oracle.h"
#endif

#define VGA_DAC_WRITE_INDEX_PORT 0x3c8
#define VGA_DAC_COMPONENT_PORT   0x3c9
#define VGA_DAC_COMPONENT_MAX   0x3f

int outp(int port, int value);

uint32_t saved_ds;

static void out8(int port, uint8_t value)
{
    outp(port, value);
#ifdef KE_ORACLE
    oracle_trace_add('O', (uint16_t)port, value, 1);
#endif
}

void write_dac_palette(void *rgb_source, int start_index, int color_count, int brightness)
{
    const uint8_t *esi = (const uint8_t *)rgb_source;
    uint32_t eax = (uint32_t)start_index;
    uint32_t ebx = (uint32_t)color_count;
    uint32_t ecx = ebx + ebx + ebx;  /* EDI = count * 3; 32-bit wrap is intentional. */
    uint32_t edi = (uint32_t)brightness;

    out8(VGA_DAC_WRITE_INDEX_PORT, (uint8_t)eax);
    /* LOOP tests after the first body: zero components underflows and iterates 2^32 times. */
    do {
        /* LODSB replaces AL only; SUB uses all 32 bits of EAX, as in the original. */
        eax = (eax & 0xffffff00u) | *esi++;
        eax -= edi;
        if ((int32_t)eax < 0)
            eax = 0;
        else if ((int32_t)eax > VGA_DAC_COMPONENT_MAX)
            eax = VGA_DAC_COMPONENT_MAX;
        out8(VGA_DAC_COMPONENT_PORT, (uint8_t)eax);
    } while (--ecx != 0);
}
void write_dac_palette_entry(void *rgb_source, int start_index, int color_count, int brightness)
{
    write_dac_palette(rgb_source, start_index, color_count, brightness);
}

void copy_ds_to_es(void)
{
    uint16_t ds;
#if defined(__i386__)
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    ((uint16_t *)&saved_ds)[0] = ds;
    __asm__ volatile("mov %0, %%es" : : "r"(ds));
#else
    /* 64-bit hosts (the ILP32 game world on Android) have no usable data segment: record
     * the virtual PC's flat selector, the value FP_SEG() reports (watcom/i86.h). The game
     * never reads saved_ds back, and ES is meaningless in the flat model. */
    ds = 0x0170;
    ((uint16_t *)&saved_ds)[0] = ds;
#endif
}
