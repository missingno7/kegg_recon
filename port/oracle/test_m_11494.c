/* Differential tests for the 8237 programming module. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

extern int inp(int port);
extern int outp(int port, int value);
extern unsigned int sound_dma_buffer_address;
extern short sound_dma_transfer_count;
extern unsigned char sound_dma_mode_bits;
extern unsigned char sound_dma_channel;
void sound_dma_mask_entry(void);
void mask_sound_dma_channel(void);
void program_sound_dma_channel(void);

#define MAX_EVENTS 128
typedef struct IoEvent { char kind; uint16_t port; uint8_t value; } IoEvent;
static IoEvent events[2][MAX_EVENTS];
static int counts[2], side;
static int overflow;

static void record(char kind, uint16_t port, uint8_t value)
{
    int n = counts[side];
    if (n < MAX_EVENTS) {
        events[side][n].kind = kind; events[side][n].port = port; events[side][n].value = value;
        counts[side]++;
    } else overflow = 1;
}
static uint32_t fake_in(void *ctx, uint16_t port, int size)
{
    (void)ctx; (void)size; record('I', port, 0xff); return 0xff;
}
static void fake_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    (void)ctx; (void)size; record('O', port, (uint8_t)value);
}
static uint32_t original_in(uint16_t port, int size) { return fake_in(NULL, port, size); }
static void original_out(uint16_t port, uint32_t value, int size) { fake_out(NULL, port, value, size); }

static void setup_ports(void)
{
    vhw_register_ports(0, 0x0f, fake_in, fake_out, NULL, "test-dma");
    vhw_register_ports(0x81, 0x83, fake_in, fake_out, NULL, "test-dma-page");
    vhw_register_ports(0x87, 0x87, fake_in, fake_out, NULL, "test-dma-page");
    oracle_set_port_hooks(original_in, original_out);
}
static int equal(void)
{
    int i;
    if (overflow || counts[0] != counts[1]) return 0;
    for (i = 0; i < counts[0]; i++)
        if (events[0][i].kind != events[1][i].kind || events[0][i].port != events[1][i].port ||
            events[0][i].value != events[1][i].value) return 0;
    return 1;
}
static void set_orig(const char *name, const void *p, size_t n) { memcpy(oracle_sym(name), p, n); }

static int test_mask_and_program(void)
{
    static const uint16_t counts_to_test[] = { 0, 1, 2, 255, 256, 640, 4096, 32768, 65535 };
    uint32_t rng = 0x51a9u;
    unsigned i;
    int failures = 0;
    setup_ports();
    for (i = 0; i < 160; i++) {
        unsigned char channel = (unsigned char)(i & 3);
        unsigned char mode = (unsigned char)((i * 37u) & 0xf8u);
        unsigned int address;
        short count = counts_to_test[i % (sizeof counts_to_test / sizeof counts_to_test[0])];
        rng = rng * 1664525u + 1013904223u;
        address = (i < 4) ? (unsigned int[]){0x0000fffeu,0x00010000u,0x0012ffffu,0x00ffffffu}[i] : rng;
        set_orig("sound_dma_channel", &channel, 1);
        set_orig("sound_dma_mode_bits", &mode, 1);
        set_orig("sound_dma_buffer_address", &address, sizeof address);
        set_orig("sound_dma_transfer_count", &count, sizeof count);
        sound_dma_channel = channel; sound_dma_mode_bits = mode;
        sound_dma_buffer_address = address; sound_dma_transfer_count = count;

        counts[0] = 0; side = 0; overflow = 0;
        oracle_call(oracle_sym("program_sound_dma_channel"), 0, NULL);
        counts[1] = 0; side = 1;
        program_sound_dma_channel();
        if (!equal()) {
            if (failures++ < 3) printf("    program channel %u address %08X count %u mode %02X: trace differs\n",
                                      channel, address, (unsigned short)count, mode);
        }
    }
    for (i = 0; i < 4; i++) {
        unsigned char channel = (unsigned char)i;
        set_orig("sound_dma_channel", &channel, 1); sound_dma_channel = channel;
        counts[0] = 0; side = 0;
        oracle_call(oracle_sym("mask_sound_dma_channel"), 0, NULL);
        counts[1] = 0; side = 1; mask_sound_dma_channel();
        if (!equal()) failures++;
    }
    oracle_set_port_hooks(NULL, NULL);
    return failures;
}

static int test_virtual_dma_data_path(void)
{
    uint16_t segment, largest = 0;
    uint32_t linear;
    uint8_t *memory, samples[4];
    int terminal, n, failures = 0;
    unsigned i;
    if (lowmem_init() != 0 || lowmem_dos_alloc(64, &segment, &largest) != 0) {
        printf("    low DOS memory allocation failed (largest=%u paragraphs)\n", largest);
        return 1;
    }
    vdma_init();
    linear = (uint32_t)segment << 4;
    memory = (uint8_t *)(uintptr_t)linear;
    sound_dma_channel = 1;
    sound_dma_buffer_address = linear;
    sound_dma_transfer_count = 4;

    memory[0] = 0x11; memory[1] = 0x82; memory[2] = 0xf0; memory[3] = 0x7f;
    sound_dma_mode_bits = 0x58;             /* memory to device, auto-initialize */
    program_sound_dma_channel();
    terminal = 0; n = vdma_read(1, samples, 4, &terminal);
    if (n != 4 || !terminal || memcmp(samples, memory, 4) != 0) failures++;
    terminal = 0; n = vdma_read(1, samples, 4, &terminal);
    if (n != 4 || !terminal || memcmp(samples, memory, 4) != 0) failures++;

    memset(memory, 0x45, 4);
    sound_dma_mode_bits = 0x44;             /* device to memory, single-cycle */
    program_sound_dma_channel();
    terminal = 0; n = vdma_write(1, 0x80, 4, &terminal);
    if (n != 4 || !terminal) failures++;
    for (i = 0; i < 4; i++)
        if (memory[i] != 0x80) failures++;
    if (vdma_write(1, 0x80, 1, &terminal) != 0) failures++; /* TC masks single-cycle DMA */

    sound_dma_mode_bits = 0x58;
    program_sound_dma_channel();
    if (vdma_write(1, 0x80, 1, &terminal) != 0) failures++; /* wrong direction */
    lowmem_dos_free(segment);
    return failures;
}

void register_m_11494_tests(void)
{
    oracle_register("m_11494 DMA mask and programming (random channels and counts)", test_mask_and_program);
    oracle_register("s1 virtual DMA directions and auto-init transfer", test_virtual_dma_data_path);
}
