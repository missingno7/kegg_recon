int sound_irq_test_flag;
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
extern unsigned char sound_blaster_response_byte;
extern unsigned char sound_blaster_command_byte;
extern int select_sound_blaster_port(void);
extern int detect_sound_blaster_irq(void);
extern int detect_sound_blaster_dma(void);
extern int reset_sound_blaster_dsp(void);
extern int write_sound_blaster_byte(void);
extern int read_sound_blaster_byte(void);
extern int acknowledge_sound_blaster_irq(void);
extern void f_d656(unsigned char *, int);
extern int f_da01(unsigned char *);
extern void f_d7b8(unsigned char *);
extern void __interrupt sound_test_irq_handler(void);
extern unsigned int dpmi_selector_or_failure_marker;
extern unsigned int sound_dma_buffer_address;
extern short sound_dma_transfer_count;
extern unsigned char sound_dma_mode_bits;
extern unsigned char sound_dma_channel;
extern unsigned int allocate_dpmi_memory(int);
extern void free_dpmi_memory(unsigned int);
extern void mask_sound_dma_channel(void);
extern void program_sound_dma_channel(void);
extern void set_sound_blaster_sample_rate(unsigned int);
extern void copy_ds_to_es(void);
extern void send_pic_end_of_interrupt(void);
extern unsigned int xms_entry_offset;
extern unsigned int xms_entry_segment;
extern unsigned int g_1258;
int detect_vga_bios_mode(void);
int detect_xms_driver(void);
int check_ems_manager_signature(void);
extern int dpmi_entry_selector;
extern int dpmi_private_data_paragraphs;
extern int dpmi_entry_offset;
extern void f_14197(void *);
extern int g_75c4;
extern void f_13889(int, int, int);

extern short vga_bios_mode_supported;
extern unsigned short unidentified_word_749a;
extern unsigned short unidentified_word_749c;
extern unsigned long vga_bios_version;
extern unsigned short unidentified_word_74a2;
extern short xms_driver_available;
extern unsigned long xms_driver_version;
extern unsigned short unidentified_word_74aa;
extern short ems_manager_signature_found;
extern unsigned long ems_manager_version;
extern unsigned short unidentified_word_74b2;
extern short dpmi_host_available;
extern unsigned long dpmi_version_bcd;
extern unsigned short u_74ba;
extern short ems_manager_available;
extern unsigned long ems_manager_handle;
extern unsigned short u_74c2;
extern unsigned char sndirq[57];
extern unsigned char scbctx[57];
extern unsigned char key_irq[57];
extern unsigned char g_756f[57];
extern unsigned int g_75a8;
extern unsigned int slave_pic_vector_base;
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
                reset_sound_blaster_dsp();
                sound_blaster_command_byte = 0xd1;
                write_sound_blaster_byte();
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
                sound_blaster_command_byte = 0xe1;
                write_sound_blaster_byte();
                read_sound_blaster_byte();
                sound_blaster_dsp_version = sound_blaster_response_byte << 8;
                read_sound_blaster_byte();
                sound_blaster_dsp_version += ((sound_blaster_response_byte / 10) << 4) | (sound_blaster_response_byte % 10);
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
                    if (reset_sound_blaster_dsp() == 0)
                        return 0;
                }
            }
        }
    }

    /* Probe the conventional 0x210..0x250 Sound Blaster base ports. */
    for (sound_blaster_base_port = 0x210; sound_blaster_base_port < 0x260; sound_blaster_base_port += 0x10) {
        for (candidate_index = 0; candidate_index < 5; candidate_index++) {
            if (reset_sound_blaster_dsp() == 0)
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
    sound_irq_test_flag = -1;
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
            interrupt_config.bytes[0x16] += (unsigned char)slave_pic_vector_base - 8 - (unsigned char)g_75a8;
        *(unsigned int *)(interrupt_config.bytes + 0x19) = 4;
        f_d656(interrupt_config.bytes, 0);
        *(unsigned int *)(interrupt_config.bytes + 0x1d) = (unsigned int)sound_test_irq_handler;
        f_da01(interrupt_config.bytes);
        acknowledge_sound_blaster_irq();
        sound_blaster_command_byte = 0xf2;
        write_sound_blaster_byte();
        for (wait_count = 0; wait_count < 0xc350; wait_count++) {
            if (sound_irq_test_flag == 0)
                break;
        }
        if (sound_irq_test_flag == -1)
            acknowledge_sound_blaster_irq();
        sound_blaster_command_byte = 0x80;
        write_sound_blaster_byte();
        sound_blaster_command_byte = 3;
        write_sound_blaster_byte();
        sound_blaster_command_byte = 0;
        write_sound_blaster_byte();
        for (wait_count = 0; wait_count < 0xc350; wait_count++) {
            if (sound_irq_test_flag == 0)
                break;
        }
        if (sound_irq_test_flag == -1)
            sound_blaster_irq = 0xff;
        f_d7b8(interrupt_config.bytes);
        if (irq_candidates.bytes[candidate_index] == 0xff)
            break;
        if (sound_irq_test_flag != -1)
            break;
    }
    return sound_irq_test_flag;
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
    test_buffer = allocate_dpmi_memory(0x1080);
    if (test_buffer != 0) {
        test_buffer = (test_buffer + 0x3ffc) & 0xffffefff;
        allocation_bytes = dpmi_selector_or_failure_marker;
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
        dma_config.bytes[0x16] += (unsigned char)slave_pic_vector_base - 8 - (unsigned char)g_75a8;
    *(unsigned int *)(dma_config.bytes + 0x19) = 4;
    f_d656(dma_config.bytes, 0);
    *(unsigned int *)(dma_config.bytes + 0x1d) = (unsigned int)sound_test_irq_handler;
    f_da01(dma_config.bytes);
    sound_dma_channel = sound_blaster_dma_channel;
    mask_sound_dma_channel();
    sound_dma_buffer_address = test_buffer;
    sound_dma_transfer_count = 4;
    sound_dma_mode_bits = 0x44;
    program_sound_dma_channel();
    *(unsigned int *)test_buffer = 0x12345678;
    set_sound_blaster_sample_rate(0x3e80);
    sound_blaster_command_byte = 0x24;
    write_sound_blaster_byte();
    sound_blaster_command_byte = sound_dma_transfer_count - 1;
    write_sound_blaster_byte();
    sound_blaster_command_byte = (sound_dma_transfer_count - 1) >> 8;
    write_sound_blaster_byte();
    for (wait_count = 0; wait_count < 0xc350; wait_count++) {
        if (*(unsigned int *)test_buffer != 0x12345678) {
            sound_dma_test_result = 0;
        }
    }
    mask_sound_dma_channel();
    f_d7b8(dma_config.bytes);
    if (dma_candidates.bytes[candidate_index] == 0xff)
        break;
    if (sound_dma_test_result != -1)
        break;
    }
    free_dpmi_memory(allocation_bytes);
    allocation_bytes = 0;
    return sound_dma_test_result;
}

void __interrupt sound_test_irq_handler(void) {
    copy_ds_to_es();
    acknowledge_sound_blaster_irq();
    send_pic_end_of_interrupt();
    sound_irq_test_flag = 0;
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
