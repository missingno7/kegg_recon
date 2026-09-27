/* Differential tests for the Sound Blaster DSP and interrupt module. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

extern int inp(int port);
extern int outp(int port, int value);
extern short sound_blaster_base_port;
extern unsigned char sound_blaster_irq;
extern unsigned char sound_blaster_dma_channel;
extern unsigned short sound_dma_block_length;
extern unsigned int sound_dma_buffer_address;
extern short sound_dma_transfer_count;
extern unsigned char sound_dma_mode_bits;
extern unsigned char sound_dma_channel;
extern unsigned char sound_blaster_response_byte;
extern unsigned char sound_blaster_command_byte;
extern short audio_stream_flag;
extern int active_audio_rate, audio_dma_half_bytes, last_audio_sample_rate;

void sound_blaster_irq_handler(void);
void sound_blaster_dma_start_entry(void);
void start_sound_blaster_dma_playback(void);
void configure_sound_dma_input(void);
void stop_sound_blaster_dma(void);
void mask_active_sound_dma_channel(void);
void sound_blaster_rate_entry(unsigned int rate);
void set_sound_blaster_sample_rate(unsigned int rate);
void pic_eoi_entry(void);
void send_pic_end_of_interrupt(void);
void sound_blaster_speaker_on_entry(void);
void enable_sound_blaster_speaker(void);
void sound_blaster_stop_entry(void);
void stop_sound_blaster_playback(void);
void sound_blaster_read_entry(void);
int read_sound_blaster_byte(void);
void sound_blaster_write_entry(void);
int write_sound_blaster_byte(void);
void sound_blaster_reset_entry(void);
int reset_sound_blaster_dsp(void);
void sound_blaster_ack_entry(void);
void acknowledge_sound_blaster_irq(void);
void mask_sound_dma_channel(void);
void program_sound_dma_channel(void);

#define SB_BASE 0x220
#define MAX_EVENTS 4096
#define SB_POLL_LIMIT 1000u
typedef struct IoEvent { char kind; uint16_t port; uint8_t value; } IoEvent;
static IoEvent event_log[2][MAX_EVENTS];
static int event_count[2], capture_side;
static uint8_t write_status_script[SB_POLL_LIMIT + 1], read_status_script[SB_POLL_LIMIT];
static unsigned write_script_count, read_script_count, write_reads, read_reads;
static uint8_t default_write_status, default_read_status, data_byte;
static int overflow;

static uint32_t fake_oracle_in(uint16_t port, int size);
static void fake_oracle_out(uint16_t port, uint32_t value, int size);

static void record_io(char kind, uint16_t port, uint8_t value)
{
    int n = event_count[capture_side];
    if (n < MAX_EVENTS) {
        event_log[capture_side][n].kind = kind;
        event_log[capture_side][n].port = port;
        event_log[capture_side][n].value = value;
        event_count[capture_side]++;
    } else {
        overflow = 1;
    }
}

static uint32_t fake_in(void *ctx, uint16_t port, int size)
{
    uint8_t value = 0xff;
    (void)ctx; (void)size;
    if (port == SB_BASE + 0x0c) {
        unsigned n = write_reads++;
        value = n < write_script_count ? write_status_script[n] : default_write_status;
    } else if (port == SB_BASE + 0x0e) {
        unsigned n = read_reads++;
        value = n < read_script_count ? read_status_script[n] : default_read_status;
    } else if (port == SB_BASE + 0x0a) {
        value = data_byte;
    }
    record_io('I', port, value);
    return value;
}

static void fake_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    (void)ctx; (void)size;
    record_io('O', port, (uint8_t)value);
}

static void fake_reset(void)
{
    write_script_count = read_script_count = write_reads = read_reads = 0;
    default_write_status = 0x7f;
    default_read_status = 0x80;
    data_byte = 0xaa;
    memset(write_status_script, 0, sizeof write_status_script);
    memset(read_status_script, 0, sizeof read_status_script);
}

static void ports_init(void)
{
    vhw_register_ports(SB_BASE, SB_BASE + 0x0f, fake_in, fake_out, NULL, "test-sb");
    vhw_register_ports(0x00, 0x0f, fake_in, fake_out, NULL, "test-dma");
    vhw_register_ports(0x20, 0x20, fake_in, fake_out, NULL, "test-pic");
    vhw_register_ports(0xa0, 0xa0, fake_in, fake_out, NULL, "test-pic");
    vhw_register_ports(0x81, 0x83, fake_in, fake_out, NULL, "test-dma-page");
    vhw_register_ports(0x87, 0x87, fake_in, fake_out, NULL, "test-dma-page");
    oracle_set_port_hooks(fake_oracle_in, fake_oracle_out);
}

/* Oracle hooks use the same scripted device but omit the vhw callback arguments. */
static uint32_t fake_oracle_in(uint16_t port, int size)
{
    return fake_in(NULL, port, size);
}
static void fake_oracle_out(uint16_t port, uint32_t value, int size)
{
    fake_out(NULL, port, value, size);
}

static int traces_equal(void)
{
    int i;
    if (event_count[0] != event_count[1] || overflow)
        return 0;
    for (i = 0; i < event_count[0]; i++)
        if (event_log[0][i].kind != event_log[1][i].kind ||
            event_log[0][i].port != event_log[1][i].port ||
            event_log[0][i].value != event_log[1][i].value)
            return 0;
    return 1;
}

static void prepare_original(void)
{
    fake_reset(); event_count[0] = 0; capture_side = 0;
}
static void prepare_translation(void)
{
    fake_reset(); event_count[1] = 0; capture_side = 1;
}
static void set_original(const char *name, const void *data, size_t n)
{
    memcpy(oracle_sym(name), data, n);
}
static void set_base_and_irq(unsigned char irq)
{
    short base = SB_BASE;
    set_original("sound_blaster_base_port", &base, sizeof base);
    set_original("sound_blaster_irq", &irq, sizeof irq);
    sound_blaster_base_port = base;
    sound_blaster_irq = irq;
}

static int test_reset_read_write_and_ack(void)
{
    uint32_t result, args[1];
    unsigned char response;
    int failures = 0;
    ports_init(); set_base_and_irq(7);

    prepare_original();
    result = oracle_call(oracle_sym("reset_sound_blaster_dsp"), 0, NULL);
    prepare_translation();
    if ((uint32_t)reset_sound_blaster_dsp() != result || !traces_equal()) { printf("    reset ack trace/result differs\n"); failures++; }

    /* Wrong reset byte follows the historical -1 path; timeout preserves its zero EAX quirk. */
    prepare_original(); data_byte = 0x45;
    result = oracle_call(oracle_sym("reset_sound_blaster_dsp"), 0, NULL);
    prepare_translation(); data_byte = 0x45;
    if ((uint32_t)reset_sound_blaster_dsp() != result || !traces_equal()) { printf("    reset wrong-byte trace/result differs\n"); failures++; }
    prepare_original(); default_read_status = 0; data_byte = 0x45;
    result = oracle_call(oracle_sym("reset_sound_blaster_dsp"), 0, NULL);
    prepare_translation(); default_read_status = 0; data_byte = 0x45;
    if ((uint32_t)reset_sound_blaster_dsp() != result || !traces_equal()) { printf("    reset timeout trace/result differs\n"); failures++; }

    prepare_original(); read_status_script[0] = 0; read_status_script[1] = 0; read_status_script[2] = 0x80;
    read_script_count = 3; data_byte = 0x37;
    oracle_call(oracle_sym("read_sound_blaster_byte"), 0, NULL);
    response = *(unsigned char *)oracle_sym("sound_blaster_response_byte");
    prepare_translation(); read_status_script[0] = 0; read_status_script[1] = 0;
    read_status_script[2] = 0x80; read_script_count = 3; data_byte = 0x37;
    (void)read_sound_blaster_byte();
    if (response != sound_blaster_response_byte || !traces_equal()) { printf("    response read differs\n"); failures++; }

    /* LOOPe decrements ECX before JECXZ: readiness on poll 1000 still times out. */
    prepare_original(); memset(read_status_script, 0, sizeof read_status_script);
    read_status_script[SB_POLL_LIMIT - 1] = 0x80; read_script_count = SB_POLL_LIMIT;
    oracle_call(oracle_sym("read_sound_blaster_byte"), 0, NULL);
    prepare_translation(); memset(read_status_script, 0, sizeof read_status_script);
    read_status_script[SB_POLL_LIMIT - 1] = 0x80; read_script_count = SB_POLL_LIMIT;
    (void)read_sound_blaster_byte();
    if (!traces_equal()) { printf("    final-poll response timeout trace differs\n"); failures++; }

    prepare_original(); write_status_script[0] = 0x80; write_status_script[1] = 0x80;
    write_status_script[2] = 0; write_script_count = 3;
    { unsigned char command = 0x51; set_original("sound_blaster_command_byte", &command, 1); sound_blaster_command_byte = command; }
    oracle_call(oracle_sym("write_sound_blaster_byte"), 0, NULL);
    prepare_translation(); write_status_script[0] = 0x80; write_status_script[1] = 0x80;
    write_status_script[2] = 0; write_script_count = 3; sound_blaster_command_byte = 0x51;
    (void)write_sound_blaster_byte();
    if (!traces_equal()) { printf("    DSP write ready trace differs\n"); failures++; }

    prepare_original(); default_write_status = 0x80;
    { unsigned char command = 0xa5; set_original("sound_blaster_command_byte", &command, 1); sound_blaster_command_byte = command; }
    oracle_call(oracle_sym("write_sound_blaster_byte"), 0, NULL);
    prepare_translation(); default_write_status = 0x80; sound_blaster_command_byte = 0xa5;
    (void)write_sound_blaster_byte();
    if (!traces_equal()) { printf("    DSP write timeout trace differs\n"); failures++; }

    /* LOOPNE plus the following ECX test also rejects readiness on retry 1000. */
    prepare_original(); memset(write_status_script, 0x80, sizeof write_status_script);
    write_status_script[SB_POLL_LIMIT] = 0; write_script_count = SB_POLL_LIMIT + 1;
    oracle_call(oracle_sym("write_sound_blaster_byte"), 0, NULL);
    prepare_translation(); memset(write_status_script, 0x80, sizeof write_status_script);
    write_status_script[SB_POLL_LIMIT] = 0; write_script_count = SB_POLL_LIMIT + 1;
    (void)write_sound_blaster_byte();
    if (!traces_equal()) { printf("    final-poll write timeout trace differs\n"); failures++; }

    prepare_original(); oracle_call(oracle_sym("acknowledge_sound_blaster_irq"), 0, NULL);
    prepare_translation(); acknowledge_sound_blaster_irq();
    if (!traces_equal()) { printf("    DSP IRQ acknowledge trace differs\n"); failures++; }

    args[0] = 0;
    for (args[0] = 0; args[0] < 3; args[0]++) {
        unsigned char irq = (unsigned char[]){7, 10, 0xff}[args[0]];
        set_base_and_irq(irq);
        prepare_original(); oracle_call(oracle_sym("send_pic_end_of_interrupt"), 0, NULL);
        prepare_translation(); send_pic_end_of_interrupt();
        if (!traces_equal()) { printf("    PIC EOI irq %u trace differs\n", irq); failures++; }
    }
    oracle_set_port_hooks(NULL, NULL);
    return failures;
}

static int test_rate_and_playback_commands(void)
{
    static const unsigned rates[] = { 0, 1, 255, 11025, 16000, 22050, 44100, 65535, 65536, 0x12345678u };
    static const unsigned short blocks[] = { 0, 1, 127, 640, 2048, 65535 };
    unsigned i;
    int failures = 0;
    ports_init(); set_base_and_irq(7);
    for (i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        uint32_t arg = rates[i];
        prepare_original(); oracle_call(oracle_sym("set_sound_blaster_sample_rate"), 1, &arg);
        prepare_translation(); set_sound_blaster_sample_rate(arg);
        if (!traces_equal()) failures++;
    }
    for (i = 0; i < sizeof blocks / sizeof blocks[0]; i++) {
        uint16_t n = blocks[i];
        set_original("sound_dma_block_length", &n, sizeof n);
        sound_dma_block_length = n;
        prepare_original(); oracle_call(oracle_sym("start_sound_blaster_dma_playback"), 0, NULL);
        prepare_translation(); start_sound_blaster_dma_playback();
        if (!traces_equal()) failures++;
    }

    prepare_original(); oracle_call(oracle_sym("stop_sound_blaster_dma"), 0, NULL);
    prepare_translation(); stop_sound_blaster_dma();
    if (!traces_equal()) failures++;

    { unsigned char channel = 1;
      set_original("sound_blaster_dma_channel", &channel, 1); sound_blaster_dma_channel = channel; }
    prepare_original(); oracle_call(oracle_sym("configure_sound_dma_input"), 0, NULL);
    prepare_translation(); configure_sound_dma_input();
    if (!traces_equal()) failures++;

    { unsigned char channel = 3;
      set_original("sound_blaster_dma_channel", &channel, 1); sound_blaster_dma_channel = channel; }
    prepare_original(); oracle_call(oracle_sym("mask_active_sound_dma_channel"), 0, NULL);
    prepare_translation(); mask_active_sound_dma_channel();
    if (!traces_equal()) failures++;

    oracle_set_port_hooks(NULL, NULL);
    return failures;
}

/* IRETD needs an interrupt frame; this small thunk gives the original ISR the real shape. */
static void call_original_irq(void *entry)
{
    __asm__ volatile(
        "pushfl\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "movw %%cs, %%cx\n\t"
        "pushl %%ecx\n\t"
        "pushl $1f\n\t"
        "jmp *%0\n"
        "1:\n\t"
        : : "r"(entry) : "eax", "ecx", "edx", "memory", "cc");
}

static int test_irq_handler(void)
{
    short zero16 = 0;
    int half = 640, rate = 11025, last = 0;
    uint32_t *p32;
    int failures = 0;
    ports_init(); set_base_and_irq(7);
    set_original("audio_stream_flag", &zero16, sizeof zero16);
    set_original("audio_dma_half_bytes", &half, sizeof half);
    set_original("active_audio_rate", &rate, sizeof rate);
    set_original("last_audio_sample_rate", &last, sizeof last);
    audio_stream_flag = 0; audio_dma_half_bytes = half;
    active_audio_rate = rate; last_audio_sample_rate = last;

    prepare_original(); call_original_irq(oracle_sym("sound_blaster_irq_handler"));
    p32 = (uint32_t *)oracle_sym("last_audio_sample_rate");
    if (*p32 != (uint32_t)rate) failures++;
    prepare_translation(); sound_blaster_irq_handler();
    if (last_audio_sample_rate != rate || audio_stream_flag != -1 || !traces_equal()) failures++;

    half = 0; set_original("audio_dma_half_bytes", &half, sizeof half);
    prepare_original(); call_original_irq(oracle_sym("sound_blaster_irq_handler"));
    prepare_translation(); audio_dma_half_bytes = 0; audio_stream_flag = -1; sound_blaster_irq_handler();
    if (audio_stream_flag != 0 || !traces_equal()) failures++;
    oracle_set_port_hooks(NULL, NULL);
    return failures;
}

void register_m_11258_tests(void)
{
    oracle_register("m_11258 Sound Blaster reset/read/write/ack/EOI", test_reset_read_write_and_ack);
    oracle_register("m_11258 Sound Blaster rate and playback commands", test_rate_and_playback_commands);
    oracle_register("m_11258 Sound Blaster IRQ handler", test_irq_handler);
}
