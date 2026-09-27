int sound_irq_test_complete_l;
int saved_sound_mixer_value;
short g_e2fe;
/* TU [0xcac2, 0xd2f0): _TEXT tables, detect_sound_blaster..read_dos_version (sound-card detection); from worker u12 T12.c */
#include <conio.h>
int sound_dma_test_result;
short sound_blaster_base_port;

#include <stdlib.h>
#include <string.h>
#include <i86.h>
struct SoundBlasterConfig { unsigned char bytes[57]; };
struct SoundBlasterIrqChoices { unsigned char bytes[7]; };
struct SoundBlasterDmaChoices { unsigned char bytes[4]; };
struct DPMIMapRecord { unsigned char opaque[0x2d]; char *allocation; unsigned int base; char *mapped; };
extern unsigned char g_7db2;
extern unsigned char g_7db3;
extern int select_sound_blaster_port(void);
extern int detect_sound_blaster_irq(void);
extern int detect_sound_blaster_dma(void);
extern int f_1144d(void);
extern int f_11420(void);
extern int f_113f8(void);
extern int f_11485(void);
extern void f_d656(unsigned char *, int);
extern int f_da01(unsigned char *);
extern void f_d7b8(unsigned char *);
extern void __interrupt sound_test_irq_handler(void);
extern unsigned int g_e31c;
extern unsigned int g_7db4;
extern short g_7db8;
extern unsigned char g_7dba;
extern unsigned char g_7dbb;
extern unsigned int f_dea6(int);
extern void f_df49(unsigned int);
extern void f_11494(void);
extern void f_114a0(void);
extern void f_11377(unsigned int);
extern void copy_ds_to_es(void);
extern void f_113bd(void);
extern unsigned int g_e300;
extern unsigned int g_e304;
extern unsigned int g_1258;
int f_d2f0(void);
int f_d36c(void);
int f_d408(void);
extern int g_e308;
extern int g_e30c;
extern int g_e310;
extern void f_14197(void *);
extern int g_75c4;
extern void f_13889(int, int, int);

extern short g_7498;
extern unsigned short u_749a;
extern unsigned short u_749c;
extern unsigned long g_749e;
extern unsigned short u_74a2;
extern short g_74a4;
extern unsigned long g_74a6;
extern unsigned short u_74aa;
extern short g_74ac;
extern unsigned long g_74ae;
extern unsigned short u_74b2;
extern short g_74b4;
extern unsigned long g_74b6;
extern unsigned short u_74ba;
extern short g_74bc;
extern unsigned long g_74be;
extern unsigned short u_74c2;
extern unsigned char g_74c4[57];
extern unsigned char g_74fd[57];
extern unsigned char g_7536[57];
extern unsigned char g_756f[57];
extern unsigned int g_75a8;
extern unsigned int g_75ac;
/* Watcom emits const aggregates in _TEXT before the function bodies. */
const unsigned char empty_sound_irq_config[57] = { 0 };
const unsigned char sound_blaster_irq_candidates[7] = { 7, 5, 3, 10, 9, 2, 0xff };
const unsigned char empty_sound_dma_config[57] = { 0 };
const unsigned char sound_blaster_dma_candidates[4] = { 1, 3, 0, 0xff };


/* _DATA [0x747c,0x7498) */
short sound_blaster_detected = 0;
unsigned int sound_blaster_mixer_test = 0xffffffffU;
unsigned int sound_blaster_dsp_version = 0xffffffffU;
unsigned char sound_blaster_irq = 0xff;
unsigned char sound_blaster_dma_channel = 0xff;
unsigned int u_7488 = 0;
char *sound_blaster_env_name = "BLASTER";
short dos_version_query_succeeded = 0;
unsigned long dos_version_packed = 0xffffffffUL;
unsigned short u_7496 = 0;

/* Probe BLASTER settings, then verify the DSP and mixer registers. */
int detect_sound_blaster(void) {
    sound_blaster_detected = 0;
    sound_blaster_irq = 0xff;
    sound_blaster_dma_channel = 0xff;
    if (select_sound_blaster_port() == 0) {
        if (detect_sound_blaster_irq() == 0) {
            if (detect_sound_blaster_dma() == 0) {
                sound_blaster_detected = -1;
                f_1144d();
                g_7db3 = 0xd1;
                f_11420();
                outp(sound_blaster_base_port + 4, 0x22);
                saved_sound_mixer_value = inp(sound_blaster_base_port + 5);
                outp(sound_blaster_base_port + 4, 0x22);
                outp(sound_blaster_base_port + 5, 0x55);
                outp(sound_blaster_base_port + 4, 0x22);
                if (inp(sound_blaster_base_port + 5) == 0x55)
                    sound_blaster_mixer_test = 1;
                else
                    sound_blaster_mixer_test = 0;
                outp(sound_blaster_base_port + 4, 0x22);
                outp(sound_blaster_base_port + 5, saved_sound_mixer_value);
                g_7db3 = 0xe1;
                f_11420();
                f_113f8();
                sound_blaster_dsp_version = g_7db2 << 8;
                f_113f8();
                sound_blaster_dsp_version += ((g_7db2 / 10) << 4) | (g_7db2 % 10);
            }
        }
    }
    return sound_blaster_detected;
}

int select_sound_blaster_port(void)
{
    int candidate_index;
    int environment_port_seen;
    unsigned char *environment_value;

    environment_port_seen = 0;
    if (environment_port_seen == 0) {
        environment_port_seen = -1;
        sound_blaster_base_port = -1;
        environment_value = getenv((char *)sound_blaster_env_name);
        if (environment_value != 0) {
            environment_value = strchr((char *)environment_value, 0x41);
            if (environment_value != 0) {
                sound_blaster_base_port = (unsigned short)((((unsigned short)environment_value[1] - 0x30) << 8) + (((unsigned short)environment_value[2] - 0x30) << 4));
                for (candidate_index = 0; candidate_index < 5; candidate_index++) {
                    if (f_1144d() == 0)
                        return 0;
                }
            }
        }
    }

    /* Probe the conventional 0x210..0x250 Sound Blaster base ports. */
    for (sound_blaster_base_port = 0x210; sound_blaster_base_port < 0x260; sound_blaster_base_port += 0x10) {
        for (candidate_index = 0; candidate_index < 5; candidate_index++) {
            if (f_1144d() == 0)
                return 0;
        }
    }
    sound_blaster_base_port = -1;
    return -1;
}

int detect_sound_blaster_irq(void) {
    struct SoundBlasterConfig interrupt_config;
    unsigned char candidate_index;
    struct SoundBlasterIrqChoices irq_candidates;
    int wait_count;
    int first_attempt;
    char * environment_value;
    interrupt_config = *(struct SoundBlasterConfig *)empty_sound_irq_config;
    candidate_index = 0;
    irq_candidates = *(struct SoundBlasterIrqChoices *)sound_blaster_irq_candidates;
    first_attempt = 0;
    sound_irq_test_complete_l = -1;
    /* The DSP's IRQ response identifies the usable interrupt line. */
    for (;;) {
        if (first_attempt == 0) {
            first_attempt = -1;
            sound_blaster_irq = 0xff;
            environment_value = getenv(sound_blaster_env_name);
            if (environment_value != 0) {
                environment_value = strchr(environment_value, 0x49);
                if (environment_value != 0) {
                    sound_blaster_irq = (unsigned char)(environment_value[1] - 0x30);
                    if (environment_value[2] >= 0x30 && environment_value[2] <= 0x39)
                        sound_blaster_irq = (unsigned char)(sound_blaster_irq * 10 + environment_value[2] - 0x30);
                }
                if (sound_blaster_irq == 2)
                    sound_blaster_irq = 9;
            }
            if (sound_blaster_irq == 0xff)
                sound_blaster_irq = irq_candidates.bytes[candidate_index++];
        } else {
            sound_blaster_irq = irq_candidates.bytes[candidate_index++];
        }
        interrupt_config.bytes[0x17] = (unsigned char)(sound_blaster_irq + (unsigned char)g_75a8);
        interrupt_config.bytes[0x16] = interrupt_config.bytes[0x17];
        if (sound_blaster_irq >= 8)
            interrupt_config.bytes[0x16] += (unsigned char)g_75ac - 8 - (unsigned char)g_75a8;
        *(unsigned int *)(interrupt_config.bytes + 0x19) = 4;
        f_d656(interrupt_config.bytes, 0);
        *(unsigned int *)(interrupt_config.bytes + 0x1d) = (unsigned int)sound_test_irq_handler;
        f_da01(interrupt_config.bytes);
        f_11485();
        g_7db3 = 0xf2;
        f_11420();
        for (wait_count = 0; wait_count < 0xc350; wait_count++) {
            if (sound_irq_test_complete_l == 0)
                break;
        }
        if (sound_irq_test_complete_l == -1)
            f_11485();
        g_7db3 = 0x80;
        f_11420();
        g_7db3 = 3;
        f_11420();
        g_7db3 = 0;
        f_11420();
        for (wait_count = 0; wait_count < 0xc350; wait_count++) {
            if (sound_irq_test_complete_l == 0)
                break;
        }
        if (sound_irq_test_complete_l == -1)
            sound_blaster_irq = 0xff;
        f_d7b8(interrupt_config.bytes);
        if (irq_candidates.bytes[candidate_index] == 0xff)
            break;
        if (sound_irq_test_complete_l != -1)
            break;
    }
    return sound_irq_test_complete_l;
}

int detect_sound_blaster_dma(void) {
    struct SoundBlasterConfig dma_config;
    unsigned char candidate_index;
    struct SoundBlasterDmaChoices dma_candidates;
    int wait_count;
    int first_attempt;
    char *environment_value;
    int allocation_bytes;
    unsigned int test_buffer;
    dma_config = *(struct SoundBlasterConfig *)empty_sound_dma_config;
    candidate_index = 0;
    dma_candidates = *(struct SoundBlasterDmaChoices *)sound_blaster_dma_candidates;
    first_attempt = 0;
    allocation_bytes = 0;
    sound_dma_test_result = -1;
    test_buffer = f_dea6(0x1080);
    if (test_buffer != 0) {
        test_buffer = (test_buffer + 0x3ffc) & 0xffffefff;
        allocation_bytes = g_e31c;
    } else {
        return sound_dma_test_result;
    }
    /* Verify each candidate DMA channel by watching the test word transfer. */
    for (;;) {
    if (first_attempt == 0) {
        first_attempt = -1;
        sound_blaster_dma_channel = 0xff;
        environment_value = getenv(sound_blaster_env_name);
        if (environment_value != 0) {
            environment_value = strchr(environment_value, 0x44);
            if (environment_value != 0)
                sound_blaster_dma_channel = (unsigned char)(environment_value[1] - 0x30);
        }
        if (sound_blaster_dma_channel == 0xff)
            sound_blaster_dma_channel = dma_candidates.bytes[candidate_index++];
    } else {
        sound_blaster_dma_channel = dma_candidates.bytes[candidate_index++];
    }
    dma_config.bytes[0x17] = (unsigned char)(sound_blaster_irq + (unsigned char)g_75a8);
    dma_config.bytes[0x16] = dma_config.bytes[0x17];
    if (sound_blaster_irq >= 8)
        dma_config.bytes[0x16] += (unsigned char)g_75ac - 8 - (unsigned char)g_75a8;
    *(unsigned int *)(dma_config.bytes + 0x19) = 4;
    f_d656(dma_config.bytes, 0);
    *(unsigned int *)(dma_config.bytes + 0x1d) = (unsigned int)sound_test_irq_handler;
    f_da01(dma_config.bytes);
    g_7dbb = sound_blaster_dma_channel;
    f_11494();
    g_7db4 = test_buffer;
    g_7db8 = 4;
    g_7dba = 0x44;
    f_114a0();
    *(unsigned int *)test_buffer = 0x12345678;
    f_11377(0x3e80);
    g_7db3 = 0x24;
    f_11420();
    g_7db3 = g_7db8 - 1;
    f_11420();
    g_7db3 = (g_7db8 - 1) >> 8;
    f_11420();
    for (wait_count = 0; wait_count < 0xc350; wait_count++) {
        if (*(unsigned int *)test_buffer != 0x12345678) {
            sound_dma_test_result = 0;
        }
    }
    f_11494();
    f_d7b8(dma_config.bytes);
    if (dma_candidates.bytes[candidate_index] == 0xff)
        break;
    if (sound_dma_test_result != -1)
        break;
    }
    f_df49(allocation_bytes);
    allocation_bytes = 0;
    return sound_dma_test_result;
}

void __interrupt sound_test_irq_handler(void) {
    copy_ds_to_es();
    f_11485();
    f_113bd();
    sound_irq_test_complete_l = 0;
}

int read_dos_version(void) {
    union REGS registers;
    struct SREGS segment_registers;
    memset(&segment_registers, 0, 12);
    registers.w.ax = 0x3000;
    int386x(0x21, &registers, &registers, &segment_registers);
    dos_version_packed = (registers.h.al << 8) + registers.h.ah;
    return dos_version_query_succeeded = -1;
}
