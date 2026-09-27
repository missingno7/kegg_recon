/* vhw.h - the small "virtual PC" underneath the unchanged game code.
 *
 * Every entry from game code into the virtual PC (port I/O, INT services, cli/sti, BIOS
 * console) is bracketed by vhw_enter()/vhw_leave(). Interrupts are delivered either
 * asynchronously by the IRQ thread while the game thread executes game code with IF=1, or
 * synchronously in vhw_leave() on the game thread (docs/port/architecture.md, "Interrupts").
 *
 * Addresses: the game's "linear" addresses are host addresses. LOWMEM_BASE..LOWMEM_END is
 * really mapped (DOS arena, BIOS ROM page, HMA); 0x0..0xFFFF (IVT/BDA) lives in a shadow
 * array that source reaches through KE_LOWMEM(); VGA memory (A0000h window) is only reached
 * through vga_mem_read8/vga_mem_write8 (port/asm translations).
 */
#ifndef KE_VHW_H
#define KE_VHW_H

#include <stdint.h>
#include "../include/watcom/i86.h"

/* ---- lifecycle ------------------------------------------------------------------------- */
int vhw_init(void);          /* main thread, before the game thread starts                */
void vhw_start_devices(void);/* after the game thread handle exists (IRQ/timer threads)   */
void vhw_shutdown(void);

/* ---- service boundaries / virtual CPU (cpu.c) ---------------------------------------- */
void vhw_enter(void);
void vhw_leave(void);
int vcpu_interrupts_enabled(void);
void vcpu_cli(void);
void vcpu_sti(void);
void vhw_cpu_poll_yield(void);     /* cooperative yield for long legacy memory polls */
void vhw_idle(uint64_t max_ns);  /* game thread waits for devices (blocking BIOS calls)   */
uint64_t vhw_clock_now_ns(void); /* shared PIT/VGA clock, with IRQ0 edge-time scope        */
void vhw_clock_irq0_enter(uint64_t edge_ns);
void vhw_clock_irq0_leave(void);
void vhw_bind_game_thread(void);
void vhw_bind_irq_thread(void);
int vhw_on_irq_thread(void);
void vhw_reset_nesting(void);   /* after ke_exit() abandons service/ISR frames          */
extern volatile long vcpu_if_flag, vhw_game_depth, vhw_in_isr;

/* ---- port I/O (portio.c) --------------------------------------------------------------- */
typedef uint32_t (*vhw_in_fn)(void *ctx, uint16_t port, int size);
typedef void (*vhw_out_fn)(void *ctx, uint16_t port, uint32_t value, int size);
void vhw_register_ports(uint16_t first, uint16_t last, vhw_in_fn in, vhw_out_fn out, void *ctx,
                        const char *device);
uint32_t vhw_port_in(uint16_t port, int size);
void vhw_port_out(uint16_t port, uint32_t value, int size);

/* ---- 8259 PIC pair + vector tables (pic.c) --------------------------------------------- */
void vpic_init(void);
void vpic_raise_irq(int irq);             /* any thread                                   */
void vpic_raise_irq_at(int irq, uint64_t edge_ns); /* device edge time for IRQ0               */
void vpic_lower_irq(int irq);
int vpic_vector_base(int slave);          /* 0x08 / 0x70                                   */
typedef void (*vhw_isr_fn)(void);
void vpic_set_pm_vector(int vector, uint16_t selector, uint32_t offset);
void vpic_get_pm_vector(int vector, uint16_t *selector, uint32_t *offset);
void vpic_set_rm_vector(int vector, uint16_t segment, uint16_t offset);
void vpic_get_rm_vector(int vector, uint16_t *segment, uint16_t *offset);
int vpic_deliver_pending(void);           /* on the calling thread; returns # delivered    */
int vpic_has_deliverable(void);
void virq_thread_start(void);             /* async delivery thread (KE_IRQ=async)          */
void virq_thread_stop(void);
void vpic_isr_exit_redirect(int code);    /* ke_exit() called inside an ISR on IRQ thread  */

/* ---- 8253 PIT (pit.c) ------------------------------------------------------------------ */
void vpit_init(void);
void vpit_shutdown(void);
#define VPIT_HZ 1193182u

/* ---- VGA (vga.c) ----------------------------------------------------------------------- */
void vga_init(void);
uint8_t vga_mem_read8(uint32_t linear);   /* linear in 0xA0000..0xBFFFF                    */
void vga_mem_write8(uint32_t linear, uint8_t value);
void vga_bios_set_mode(int mode);         /* INT 10h AH=00                                 */
int vga_bios_mode(void);
/* Scan out the visible frame as 8-bit indexed pixels; returns 0 if no graphics frame. */
int vga_scanout(uint8_t *dst, int dst_pitch, int max_w, int max_h, int *out_w, int *out_h);
void vga_palette_rgb888(uint8_t rgb[256][3]);   /* DAC (6-bit) expanded to 8-bit        */
uint32_t vga_frame_counter(void);         /* completed virtual retraces                    */

/* ---- keyboard controller + BIOS keyboard (kbd.c) --------------------------------------- */
void vkbd_init(void);
void vkbd_push_scancode(uint8_t code);    /* main thread: XT set-1 byte (E0 prefixed ok)   */
void vkbd_bios_irq1(void);                /* default IRQ1 handler (no game handler)        */
int vkbd_bios_kbhit(void);
int vkbd_bios_getch(void);                /* blocks via vhw_idle                           */

/* ---- mouse driver INT 33h (mouse.c) ---------------------------------------------------- */
void vmouse_init(void);
void vmouse_motion(float dx, float dy);   /* main thread, relative host motion            */
void vmouse_motion_at(float dx, float dy, uint64_t timestamp_ns); /* SDL event time       */
void vmouse_buttons(int mask);            /* bit0 left, bit1 right, bit2 middle           */
void vmouse_int33(union REGS *r, struct SREGS *s);

/* ---- gameport (joy.c) ------------------------------------------------------------------ */
void vjoy_init(void);
void vjoy_set(int axis, float value, int buttons); /* value -1..1                          */

/* ---- Sound Blaster DSP + 8237 DMA (sb.c, dma.c) ---------------------------------------- */
void vdma_init(void);
/* Pull up to `len` bytes from an 8-bit channel; returns bytes read, *terminal set at TC.    */
int vdma_read(int channel, uint8_t *dst, int len, int *terminal);
/* Write `len` copies of a device sample to an 8-bit channel; returns bytes, *terminal at TC. */
int vdma_write(int channel, uint8_t sample, int len, int *terminal);
void vsb_init(void);
void vsb_shutdown(void);

/* ---- INT services (intsvc.c) ----------------------------------------------------------- */
int vhw_int(int intno, union REGS *in, union REGS *out, struct SREGS *s);

/* ---- low memory (lowmem.c) ------------------------------------------------------------- */
int lowmem_init(void);
extern uint8_t ke_lowmem_shadow[0x10000];   /* IVT + BIOS data area + DOS area < 64 KiB     */
void *lowmem_ptr(uint32_t linear);
int lowmem_dos_alloc(uint16_t paragraphs, uint16_t *segment, uint16_t *largest);
int lowmem_dos_free(uint16_t segment);
uint16_t lowmem_dos_largest(void);
#define LOWMEM_BASE 0xE0000u               /* identity-mapped real-mode range (lowmem.c)   */
#define LOWMEM_END 0x110000u
#define KE_BIOS_ROM_SEGMENT 0xF000u
#define KE_BIOS_IRET_OFFSET 0xFF53u         /* default real-mode vectors point at an IRET    */

/* ---- faults (fault.c) ------------------------------------------------------------------ */
void vhw_fault_init(void);

#endif
