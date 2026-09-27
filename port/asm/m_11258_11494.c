/* m_11258_11494.c - instruction-order translation of asm/m_11258_11494.asm.
 * Sound Blaster DSP commands, DMA stream setup, IRQ acknowledgement and PIC EOI.
 * Port operations use the virtual-PC conio bridge so each I/O has a service boundary.
 */
#include <stdint.h>
#include "../vhw/vhw.h"

extern int inp(int port);
extern int outp(int port, int value);

/* Historical globals imported from the C sound setup and stream units. */
extern short sound_blaster_base_port;
extern unsigned char sound_blaster_irq;
extern unsigned char sound_blaster_dma_channel;
extern unsigned char sound_dma_mode_bits;
extern unsigned char sound_dma_channel;
extern unsigned short sound_dma_block_length;
extern short audio_stream_flag;
extern int active_audio_rate;
extern int audio_dma_half_bytes;
extern int last_audio_sample_rate;
extern void mask_sound_dma_channel(void);
extern void program_sound_dma_channel(void);
extern void transfer_audio_stream_block(void);

/* Public _DATA labels (kept as independent C objects; no code overlays this block). */
unsigned short sound_dma_block_length;
unsigned char sound_blaster_response_byte;
unsigned char sound_blaster_command_byte;

#define SB_RESET_OFFSET       0x06
#define SB_READ_DATA_OFFSET   0x0a
#define SB_WRITE_OFFSET       0x0c
#define SB_READ_STATUS_OFFSET 0x0e
#define SB_STATUS_BUSY        0x80
#define SB_POLL_LIMIT         0x3e8u
#define SB_RESET_DELAY        0xffu
#define SB_RESET_ACK          0xaau
#define SB_CMD_RATE           0x40
#define SB_CMD_DMA            0x14
#define SB_CMD_STOP           0xd0
#define SB_CMD_SPEAKER_ON     0xd1
#define SB_CMD_SPEAKER_OFF    0xd3
#define PIC_MASTER_COMMAND    0x20
#define PIC_SLAVE_COMMAND     0xa0
#define PIC_EOI               0x20
#define PIC_SLAVE_IRQ_BASE    8

static uint16_t sb_base(void)
{
    return (uint16_t)sound_blaster_base_port;
}

/* Wait while the DSP write-status busy bit is set. QUIRK: LOOPNE consumes ECX before
 * testing ZF, so readiness on the final (1000th) retry still takes the timeout branch. */
static int dsp_write_command_byte(void)
{
    uint16_t port = (uint16_t)(sb_base() + SB_WRITE_OFFSET);
    unsigned int count;
    unsigned char status = (unsigned char)inp(port);
    if (!(status & SB_STATUS_BUSY)) {
        outp(port, sound_blaster_command_byte);
        return 0;
    }
    for (count = SB_POLL_LIMIT; count != 0; --count) {
        status = (unsigned char)inp(port);
        if (!(status & SB_STATUS_BUSY)) {
            if (count == 1)
                return 1;
            outp(port, sound_blaster_command_byte);
            return 0;
        }
    }
    return 1;
}

int write_sound_blaster_byte(void)
{
    return dsp_write_command_byte();
}

void sound_blaster_write_entry(void)
{
    (void)write_sound_blaster_byte();
}

/* LOope waits until bit 7 becomes set (response available). QUIRK: readiness on the final
 * poll still reaches JECXZ and times out. Carry is represented as 1 for that branch. */
static int dsp_read_response_byte(void)
{
    uint16_t port = (uint16_t)(sb_base() + SB_READ_STATUS_OFFSET);
    unsigned int count;
    unsigned char status = 0;
    count = SB_POLL_LIMIT;
    while (count != 0) {
        status = (unsigned char)inp(port);
        --count;
        if (status & SB_STATUS_BUSY)
            break;
    }
    if (count == 0)
        return 1;
    sound_blaster_response_byte = (unsigned char)inp((uint16_t)(sb_base() + SB_READ_DATA_OFFSET));
    return 0;
}

int read_sound_blaster_byte(void)
{
    return dsp_read_response_byte();
}

void sound_blaster_read_entry(void)
{
    (void)read_sound_blaster_byte();
}

int reset_sound_blaster_dsp(void)
{
    volatile unsigned int delay;
    outp((uint16_t)(sb_base() + SB_RESET_OFFSET), 1);
    for (delay = SB_RESET_DELAY; delay != 0; --delay) { /* QUIRK: exact 16-bit busy delay */ }
    outp((uint16_t)(sb_base() + SB_RESET_OFFSET), 0);
    if (dsp_read_response_byte())
        return 0;                         /* QUIRK: timeout leaves the default success EAX */
    return sound_blaster_response_byte == SB_RESET_ACK ? 0 : -1;
}

void sound_blaster_reset_entry(void)
{
    (void)reset_sound_blaster_dsp();
}

void acknowledge_sound_blaster_irq(void)
{
    (void)inp((uint16_t)(sb_base() + SB_READ_STATUS_OFFSET));
}

void sound_blaster_ack_entry(void)
{
    acknowledge_sound_blaster_irq();
}

void send_pic_end_of_interrupt(void)
{
    if ((int8_t)sound_blaster_irq >= PIC_SLAVE_IRQ_BASE) /* QUIRK: original JL is signed */
        outp(PIC_SLAVE_COMMAND, PIC_EOI);
    outp(PIC_MASTER_COMMAND, PIC_EOI);
}

void pic_eoi_entry(void)
{
    send_pic_end_of_interrupt();
}

void set_sound_blaster_sample_rate(unsigned int sample_rate)
{
    uint16_t rounded = (uint16_t)((uint16_t)sample_rate + 0x7f);
    uint16_t divisor = (uint16_t)(rounded >> 8);
    uint16_t time_constant;
    if (divisor == 0)
        return;
    time_constant = (uint16_t)(0x0f42u / divisor);
    time_constant = (uint16_t)(0x100u - time_constant);
    sound_blaster_command_byte = SB_CMD_RATE;
    if (dsp_write_command_byte())
        return;
    sound_blaster_command_byte = (unsigned char)time_constant;
    (void)dsp_write_command_byte();
}

void sound_blaster_rate_entry(unsigned int sample_rate)
{
    set_sound_blaster_sample_rate(sample_rate);
}

void start_sound_blaster_dma_playback(void)
{
    uint16_t remaining = (uint16_t)(sound_dma_block_length - 1);
    sound_blaster_command_byte = SB_CMD_DMA;
    (void)dsp_write_command_byte();
    sound_blaster_command_byte = (unsigned char)remaining;
    (void)dsp_write_command_byte();
    sound_blaster_command_byte = (unsigned char)(remaining >> 8);
    (void)dsp_write_command_byte();
}

void sound_blaster_dma_start_entry(void)
{
    start_sound_blaster_dma_playback();
}

void configure_sound_dma_input(void)
{
    sound_dma_mode_bits = 0x58;           /* QUIRK: 8-bit auto-init mode remains in 8237 */
    sound_dma_channel = sound_blaster_dma_channel;
    program_sound_dma_channel();
}

void stop_sound_blaster_dma(void)
{
    sound_blaster_command_byte = SB_CMD_STOP;
    (void)dsp_write_command_byte();
}

void mask_active_sound_dma_channel(void)
{
    sound_dma_channel = sound_blaster_dma_channel;
    mask_sound_dma_channel();
}

static void wait_for_dsp_write_ready(void)
{
    unsigned int count;
    for (count = SB_POLL_LIMIT; count != 0; --count)
        if (!((unsigned char)inp((uint16_t)(sb_base() + SB_WRITE_OFFSET)) & SB_STATUS_BUSY))
            break;
}

void sound_blaster_irq_handler(void)
{
    uint16_t write_port = (uint16_t)(sb_base() + SB_WRITE_OFFSET);
    (void)inp((uint16_t)(sb_base() + SB_READ_STATUS_OFFSET));
    if (audio_dma_half_bytes != 0) {
        if (last_audio_sample_rate != active_audio_rate) {
            last_audio_sample_rate = active_audio_rate;
            set_sound_blaster_sample_rate((unsigned int)active_audio_rate);
        }
        wait_for_dsp_write_ready();
        outp(write_port, SB_CMD_DMA);
        wait_for_dsp_write_ready();
        outp(write_port, 0x7f);
        wait_for_dsp_write_ready();
        outp(write_port, 0x02);
        audio_stream_flag = (short)-1;
    } else {
        audio_stream_flag = 0;
    }
    vhw_enter();
    vcpu_sti();
    vhw_leave();
    send_pic_end_of_interrupt();
    transfer_audio_stream_block();
}

void sound_blaster_speaker_on_entry(void)
{
    (void)reset_sound_blaster_dsp();
    sound_blaster_command_byte = SB_CMD_SPEAKER_ON;
    (void)dsp_write_command_byte();
}

void enable_sound_blaster_speaker(void)
{
    sound_blaster_speaker_on_entry();
}

void sound_blaster_stop_entry(void)
{
    stop_sound_blaster_dma();
    (void)reset_sound_blaster_dsp();
    sound_blaster_command_byte = SB_CMD_SPEAKER_OFF;
    (void)dsp_write_command_byte();
}

void stop_sound_blaster_playback(void)
{
    sound_blaster_stop_entry();
}
