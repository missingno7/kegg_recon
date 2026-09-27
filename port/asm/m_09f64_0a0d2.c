/* m_09f64_0a0d2.c - literal C translation of asm/m_09f64_0a0d2.asm.
 *
 * The delay loop and the order of PIT/PIC/RTC accesses are retained because the
 * calibration samples one retrace interval. The two dwords also retain their
 * historical order: pit_sample_auxiliary, then g_pit_elapsed_ticks.
 */
#include <stdint.h>
#include "../vhw/vhw.h"
#include "../include/ke_port.h"
#ifdef KE_ORACLE
#include "../oracle/oracle.h"
#endif

#define RTC_INDEX_PORT 0x70
#define RTC_NMI_DISABLE_BIT 0x80
#define RTC_INDEX_MASK 0x7f
#define PIC_MASTER_COMMAND_PORT 0x20
#define PIC_MASTER_MASK_PORT 0x21
#define PIC_SLAVE_MASK_PORT 0xa1
#define PIC_EOI_COMMAND 0x20
#define PIC_MASK_ALL 0xff
#define PIT_CHANNEL0_PORT 0x40
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_RATEGEN_LH 0x34
#define PIT_RELOAD_ALL_ONES 0xffff
#define PIT_COUNTER_MODULUS 0x10000u
#define PIT_SETTLE_LOOP_SEED 0xfffffc18u

extern void wait_for_vsync(void);
extern void process_timer_events(void);
extern int pit_rollover_value;
extern int timer_enabled09;
int inp(int port);
int outp(int port, int value);

uint32_t pit_sample_auxiliary = 0;
uint32_t g_pit_elapsed_ticks = 0;

static uint8_t pit_in8(uint16_t port)
{
    uint8_t value = (uint8_t)inp(port);
#ifdef KE_ORACLE
    oracle_trace_add('I', port, value, 1);
#endif
    return value;
}

static void pit_out8(uint16_t port, uint8_t value)
{
    outp(port, value);
#ifdef KE_ORACLE
    oracle_trace_add('O', port, value, 1);
#endif
}

static int cli_save_if(void)
{
    int was_enabled = vcpu_interrupts_enabled();
    vhw_enter();
#ifdef KE_ORACLE
    oracle_trace_add('C', 0, 0, 0);
#endif
    vcpu_cli();
    vhw_leave();
    return was_enabled;
}

static void restore_if(int was_enabled)
{
    vhw_enter();
    if (was_enabled)
        vcpu_sti();
    else
        vcpu_cli();
    vhw_leave();
}

/* The Watcom C wait helper leaves its last IN result in AL. Re-read status immediately
 * after it returns so the literal C translation preserves that command byte (08h or 09h)
 * for the following PIT latch write. */
static uint8_t wait_for_vsync_al(void)
{
    wait_for_vsync();
    return (uint8_t)inp(0x3da);
}

int measure_pit_channel0(void)
{
    uint8_t rtc_index, saved_nmi, master_mask, slave_mask;
    uint8_t al, low, high;
    uint32_t eax = 0;
    uint32_t ecx = PIT_SETTLE_LOOP_SEED;
    volatile uint32_t settle_sink = 0;
    int saved_if = cli_save_if();

    rtc_index = pit_in8(RTC_INDEX_PORT);
    saved_nmi = rtc_index & RTC_NMI_DISABLE_BIT;
    pit_out8(RTC_INDEX_PORT, (uint8_t)(rtc_index | RTC_NMI_DISABLE_BIT));

    master_mask = pit_in8(PIC_MASTER_MASK_PORT);
    slave_mask = pit_in8(PIC_SLAVE_MASK_PORT);
    pit_out8(PIC_MASTER_MASK_PORT, PIC_MASK_ALL);
    pit_out8(PIC_SLAVE_MASK_PORT, PIC_MASK_ALL);

    wait_for_vsync();
    wait_for_vsync();
    pit_out8(PIT_COMMAND_PORT, PIT_CHANNEL0_RATEGEN_LH);
    pit_out8(PIT_CHANNEL0_PORT, 0);
    pit_out8(PIT_CHANNEL0_PORT, 0);

    /* 1000 * 16 BTC operations. The volatile accumulator keeps the delay observable to C. */
    do {
        unsigned bit;
        for (bit = 0; bit < 16; bit++) {
            eax ^= 2;
            settle_sink = eax;
        }
        ecx++;
    } while (ecx != 0);
    (void)settle_sink;

    al = wait_for_vsync_al();
    pit_out8(PIT_COMMAND_PORT, al);
    low = pit_in8(PIT_CHANNEL0_PORT);
    high = pit_in8(PIT_CHANNEL0_PORT);
    pit_sample_auxiliary = ((uint32_t)high << 8) | low;
    g_pit_elapsed_ticks = PIT_COUNTER_MODULUS - pit_sample_auxiliary;

    pit_out8(PIC_SLAVE_MASK_PORT, slave_mask);
    pit_out8(PIC_MASTER_MASK_PORT, master_mask);
    al = pit_in8(RTC_INDEX_PORT);
    al = (uint8_t)((al & RTC_INDEX_MASK) | saved_nmi);
    pit_out8(RTC_INDEX_PORT, al);
    restore_if(saved_if);
    return (int)g_pit_elapsed_ticks;
}

int set_pit_channel0_reload(int value)
{
    uint32_t reload = (uint32_t)value;
    int saved_if = cli_save_if();
    pit_out8(PIT_COMMAND_PORT, PIT_CHANNEL0_RATEGEN_LH);
    pit_out8(PIT_CHANNEL0_PORT, (uint8_t)reload);
    pit_out8(PIT_CHANNEL0_PORT, (uint8_t)(reload >> 8));
    restore_if(saved_if);
    return value;
}

void pit_channel0_interrupt(void)
{
    uint32_t reload;
    pit_out8(PIT_COMMAND_PORT, PIT_CHANNEL0_RATEGEN_LH);
    pit_out8(PIT_CHANNEL0_PORT, (uint8_t)PIT_RELOAD_ALL_ONES);
    pit_out8(PIT_CHANNEL0_PORT, (uint8_t)(PIT_RELOAD_ALL_ONES >> 8));
    wait_for_vsync();
    pit_out8(PIT_COMMAND_PORT, PIT_CHANNEL0_RATEGEN_LH);
    reload = (uint32_t)pit_rollover_value;
    pit_out8(PIT_CHANNEL0_PORT, (uint8_t)reload);
    pit_out8(PIT_CHANNEL0_PORT, (uint8_t)(reload >> 8));
    ++timer_enabled09;
    process_timer_events();
    pit_out8(PIC_MASTER_COMMAND_PORT, PIC_EOI_COMMAND);
}
