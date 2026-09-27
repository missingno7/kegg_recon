int sound_irq_test_flag;
int saved_sound_mixer_value;
/* Unreferenced word retained from the original sound setup data; meaning unknown. */
short g_e2fe;
/* TU [0xcac2, 0xd2f0): _TEXT tables, detect_sound_blaster..read_dos_version (sound-card detection); from worker u12 T12.c */
#include <conio.h>
int sound_dma_test_result;
short sound_blaster_base_port;

#include <stdlib.h>
#include <string.h>
#include <i86.h>
/* Sound IRQ and DMA probes build the same 57-byte record used by DPMI hooks. */
struct InterruptState {
    short status;
    short state_saved;
    short cleanup_registered;
    unsigned int old_dpmi_offset;
    unsigned short old_dpmi_selector;
    unsigned int old_dos_offset;
    unsigned short old_dos_segment;
    unsigned short descriptor_selector;
    unsigned short descriptor_offset;
    unsigned char interrupt_number;
    unsigned char irq_line;
    unsigned char saved_pic_mask;
    unsigned int hook_flags;
    unsigned int handler_address;
    unsigned int physical_start;
    unsigned int physical_end;
    unsigned int mapping_length;
    int allocated_base;
    unsigned int dpmi_memory_handle;
    char *mapped_address;
};
struct SoundBlasterIrqChoices { unsigned char irq_lines[7]; };
struct SoundBlasterDmaChoices { unsigned char dma_channels[4]; };

#define INTERRUPT_STATE_BYTES 57
#define DOS_INTERRUPT 0x21
#define DOS_GET_VERSION_FUNCTION 0x3000
#define DOS_PAGE_ALIGNED_ADDRESS_MASK 0xffffefff
#define DOS_VERSION_UNKNOWN 0xffffffffUL
#define SB_MIXER_INDEX_OFFSET 4
#define SB_MIXER_DATA_OFFSET 5
#define SB_DSP_RESET_OFFSET 6
#define SB_DSP_READ_DATA_OFFSET 0xa
#define SB_DSP_WRITE_DATA_OFFSET 0xc
#define SB_DSP_READ_STATUS_OFFSET 0xe
#define SB_MIXER_TEST_REGISTER 0x22
#define SB_MIXER_TEST_VALUE 0x55
#define SB_DSP_CMD_SPEAKER_ON 0xd1
#define SB_DSP_CMD_GET_VERSION 0xe1
#define SB_DSP_CMD_TEST_IRQ 0xf2
#define SB_DSP_CMD_PAUSE_DAC 0x80
#define SB_DSP_CMD_SINGLE_CYCLE_INPUT 0x24
#define SB_DSP_TEST_RATE 0x3e80
#define SB_DMA_TEST_BUFFER_BYTES 0x1080
#define SB_DMA_TEST_ALIGNMENT_BIAS 0x3ffc
#define SB_DMA_MODE_SINGLE_TRANSFER_DEVICE_TO_MEMORY 0x44
#define SB_DMA_TEST_WORD 0x12345678
#define SOUND_TEST_NOT_RUN 0xffffffffU
#define SB_IRQ_TEST_PAUSE_DURATION_LOW 3
#define SB_IRQ_TEST_PAUSE_DURATION_HIGH 0
#define SB_DETECTION_POLL_LIMIT 0xc350
#define SB_BASE_PORT_FIRST 0x210
#define SB_BASE_PORT_LIMIT 0x260
#define SB_BASE_PORT_STEP 0x10
#define BLASTER_BASE_PORT_MARKER 0x41
#define BLASTER_IRQ_MARKER 0x49
#define BLASTER_DMA_MARKER 0x44
#define DOS_ASCII_ZERO 0x30
#define SOUND_CANDIDATE_END 0xff
#define IRQ2_SLAVE_ALIAS 9
extern unsigned char sound_blaster_response_byte;
extern unsigned char sound_blaster_command_byte;
extern int select_sound_blaster_port(void);
extern int detect_sound_blaster_irq(void);
extern int detect_sound_blaster_dma(void);
extern int reset_sound_blaster_dsp(void);
extern int write_sound_blaster_byte(void);
extern int read_sound_blaster_byte(void);
extern int acknowledge_sound_blaster_irq(void);
/* Frozen T06 requires these interrupt-hook linker names. */
extern void save_irq(unsigned char *, int);
extern int install(unsigned char *);
extern void restore(unsigned char *);
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
extern unsigned int picvec;
extern unsigned int slave_pic_vector_base;
/* Watcom emits const aggregates in _TEXT before the function bodies. */
const unsigned char empty_sound_irq_config[INTERRUPT_STATE_BYTES] = { 0 };
const unsigned char sound_blaster_irq_candidates[7] = { 7, 5, 3, 10, 9, 2, SOUND_CANDIDATE_END };
const unsigned char empty_sound_dma_config[INTERRUPT_STATE_BYTES] = { 0 };
const unsigned char sound_blaster_dma_candidates[4] = { 1, 3, 0, SOUND_CANDIDATE_END };


/* _DATA [0x747c,0x7498) */
short sound_blaster_detected = 0;
unsigned int sound_blaster_mixer_test = SOUND_TEST_NOT_RUN;
unsigned int sound_blaster_dsp_version = SOUND_TEST_NOT_RUN;
unsigned char sound_blaster_irq = SOUND_CANDIDATE_END;
unsigned char sound_blaster_dma_channel = SOUND_CANDIDATE_END;
/* Unused configuration words in the recovered data block; their roles are unknown. */
unsigned int u_7488 = 0;
char *sound_blaster_env_name = "BLASTER";
short dos_version_query_succeeded = 0;
unsigned long dos_version_packed = DOS_VERSION_UNKNOWN;
unsigned short u_7496 = 0;

/* Probe BLASTER settings, then verify the DSP and mixer registers. */
int detect_sound_blaster(void) {
    sound_blaster_detected = 0;
    sound_blaster_irq = SOUND_CANDIDATE_END;
    sound_blaster_dma_channel = SOUND_CANDIDATE_END;
    if (select_sound_blaster_port() == 0) {
        if (detect_sound_blaster_irq() == 0) {
            if (detect_sound_blaster_dma() == 0) {
                sound_blaster_detected = -1;
                reset_sound_blaster_dsp();
                sound_blaster_command_byte = SB_DSP_CMD_SPEAKER_ON;
                write_sound_blaster_byte();
                outp(sound_blaster_base_port + SB_MIXER_INDEX_OFFSET, SB_MIXER_TEST_REGISTER);
                saved_sound_mixer_value = inp(sound_blaster_base_port + SB_MIXER_DATA_OFFSET);
                outp(sound_blaster_base_port + SB_MIXER_INDEX_OFFSET, SB_MIXER_TEST_REGISTER);
                outp(sound_blaster_base_port + SB_MIXER_DATA_OFFSET, SB_MIXER_TEST_VALUE);
                outp(sound_blaster_base_port + SB_MIXER_INDEX_OFFSET, SB_MIXER_TEST_REGISTER);
                if (inp(sound_blaster_base_port + SB_MIXER_DATA_OFFSET) == SB_MIXER_TEST_VALUE)
                    sound_blaster_mixer_test = 1;
                else
                    sound_blaster_mixer_test = 0;
                outp(sound_blaster_base_port + SB_MIXER_INDEX_OFFSET, SB_MIXER_TEST_REGISTER);
                outp(sound_blaster_base_port + SB_MIXER_DATA_OFFSET, saved_sound_mixer_value);
                sound_blaster_command_byte = SB_DSP_CMD_GET_VERSION;
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
            environment_value = strchr((char *)environment_value, BLASTER_BASE_PORT_MARKER);
            if (environment_value != 0) {
                sound_blaster_base_port = (unsigned short)((((unsigned short)environment_value[1] - DOS_ASCII_ZERO) << 8) + (((unsigned short)environment_value[2] - DOS_ASCII_ZERO) << 4));
                for (candidate_index = 0; candidate_index < 5; candidate_index++) {
                    if (reset_sound_blaster_dsp() == 0)
                        return 0;
                }
            }
        }
    }

    /* Probe the conventional 0x210..0x250 Sound Blaster base ports. */
    for (sound_blaster_base_port = SB_BASE_PORT_FIRST; sound_blaster_base_port < SB_BASE_PORT_LIMIT; sound_blaster_base_port += SB_BASE_PORT_STEP) {
        for (candidate_index = 0; candidate_index < 5; candidate_index++) {
            if (reset_sound_blaster_dsp() == 0)
                return 0;
        }
    }
    sound_blaster_base_port = -1;
    return -1;
}

int detect_sound_blaster_irq(void) {
    struct InterruptState interrupt_config;
    unsigned char candidate_index;
    struct SoundBlasterIrqChoices irq_candidates;
    int wait_count;
    int first_attempt;
    char * environment_value;
    interrupt_config = *(struct InterruptState *)empty_sound_irq_config;
    candidate_index = 0;
    irq_candidates = *(struct SoundBlasterIrqChoices *)sound_blaster_irq_candidates;
    first_attempt = 0;
    sound_irq_test_flag = -1;
    /* The DSP's IRQ response identifies the usable interrupt line. */
    for (;;) {
        if (first_attempt == 0) {
            first_attempt = -1;
            sound_blaster_irq = SOUND_CANDIDATE_END;
            environment_value = getenv(sound_blaster_env_name);
            if (environment_value != 0) {
                environment_value = strchr(environment_value, BLASTER_IRQ_MARKER);
                if (environment_value != 0) {
                    sound_blaster_irq = (unsigned char)(environment_value[1] - DOS_ASCII_ZERO);
                    if (environment_value[2] >= DOS_ASCII_ZERO && environment_value[2] <= DOS_ASCII_ZERO + 9)
                        sound_blaster_irq = (unsigned char)(sound_blaster_irq * 10 + environment_value[2] - DOS_ASCII_ZERO);
                }
                if (sound_blaster_irq == 2)
                    sound_blaster_irq = IRQ2_SLAVE_ALIAS;
            }
            if (sound_blaster_irq == SOUND_CANDIDATE_END)
                sound_blaster_irq = irq_candidates.irq_lines[candidate_index++];
        } else {
            sound_blaster_irq = irq_candidates.irq_lines[candidate_index++];
        }
        interrupt_config.irq_line = (unsigned char)(sound_blaster_irq + (unsigned char)picvec);
        interrupt_config.interrupt_number = interrupt_config.irq_line;
        if (sound_blaster_irq >= 8)
            interrupt_config.interrupt_number += (unsigned char)slave_pic_vector_base - 8 - (unsigned char)picvec;
        interrupt_config.hook_flags = 4;
        save_irq((unsigned char *)&interrupt_config, 0);
        interrupt_config.handler_address = (unsigned int)sound_test_irq_handler;
        install((unsigned char *)&interrupt_config);
        acknowledge_sound_blaster_irq();
        sound_blaster_command_byte = SB_DSP_CMD_TEST_IRQ;
        write_sound_blaster_byte();
        for (wait_count = 0; wait_count < SB_DETECTION_POLL_LIMIT; wait_count++) {
            if (sound_irq_test_flag == 0)
                break;
        }
        if (sound_irq_test_flag == -1)
            acknowledge_sound_blaster_irq();
        /* The duration expires as a DSP interrupt, testing the selected IRQ line. */
        sound_blaster_command_byte = SB_DSP_CMD_PAUSE_DAC;
        write_sound_blaster_byte();
        sound_blaster_command_byte = SB_IRQ_TEST_PAUSE_DURATION_LOW;
        write_sound_blaster_byte();
        sound_blaster_command_byte = SB_IRQ_TEST_PAUSE_DURATION_HIGH;
        write_sound_blaster_byte();
        for (wait_count = 0; wait_count < SB_DETECTION_POLL_LIMIT; wait_count++) {
            if (sound_irq_test_flag == 0)
                break;
        }
        if (sound_irq_test_flag == -1)
            sound_blaster_irq = SOUND_CANDIDATE_END;
        restore((unsigned char *)&interrupt_config);
        if (irq_candidates.irq_lines[candidate_index] == SOUND_CANDIDATE_END)
            break;
        if (sound_irq_test_flag != -1)
            break;
    }
    return sound_irq_test_flag;
}

int detect_sound_blaster_dma(void) {
    struct InterruptState dma_config;
    unsigned char candidate_index;
    struct SoundBlasterDmaChoices dma_candidates;
    int wait_count;
    int first_attempt;
    char *environment_value;
    int allocation_bytes;
    unsigned int test_buffer;
    dma_config = *(struct InterruptState *)empty_sound_dma_config;
    candidate_index = 0;
    dma_candidates = *(struct SoundBlasterDmaChoices *)sound_blaster_dma_candidates;
    first_attempt = 0;
    allocation_bytes = 0;
    sound_dma_test_result = -1;
    test_buffer = allocate_dpmi_memory(SB_DMA_TEST_BUFFER_BYTES);
    if (test_buffer != 0) {
        test_buffer = (test_buffer + SB_DMA_TEST_ALIGNMENT_BIAS) & DOS_PAGE_ALIGNED_ADDRESS_MASK;
        allocation_bytes = dpmi_selector_or_failure_marker;
    } else {
        return sound_dma_test_result;
    }
    /* Verify each candidate DMA channel by watching the test word transfer. */
    for (;;) {
    if (first_attempt == 0) {
        first_attempt = -1;
        sound_blaster_dma_channel = SOUND_CANDIDATE_END;
        environment_value = getenv(sound_blaster_env_name);
        if (environment_value != 0) {
            environment_value = strchr(environment_value, BLASTER_DMA_MARKER);
            if (environment_value != 0)
            sound_blaster_dma_channel = (unsigned char)(environment_value[1] - DOS_ASCII_ZERO);
        }
        if (sound_blaster_dma_channel == SOUND_CANDIDATE_END)
            sound_blaster_dma_channel = dma_candidates.dma_channels[candidate_index++];
    } else {
        sound_blaster_dma_channel = dma_candidates.dma_channels[candidate_index++];
    }
    dma_config.irq_line = (unsigned char)(sound_blaster_irq + (unsigned char)picvec);
    dma_config.interrupt_number = dma_config.irq_line;
    if (sound_blaster_irq >= 8)
        dma_config.interrupt_number += (unsigned char)slave_pic_vector_base - 8 - (unsigned char)picvec;
    dma_config.hook_flags = 4;
    save_irq((unsigned char *)&dma_config, 0);
    dma_config.handler_address = (unsigned int)sound_test_irq_handler;
    install((unsigned char *)&dma_config);
    sound_dma_channel = sound_blaster_dma_channel;
    mask_sound_dma_channel();
    sound_dma_buffer_address = test_buffer;
    sound_dma_transfer_count = 4;
    sound_dma_mode_bits = SB_DMA_MODE_SINGLE_TRANSFER_DEVICE_TO_MEMORY;
    program_sound_dma_channel();
    *(unsigned int *)test_buffer = SB_DMA_TEST_WORD;
    set_sound_blaster_sample_rate(SB_DSP_TEST_RATE);
    sound_blaster_command_byte = SB_DSP_CMD_SINGLE_CYCLE_INPUT;
    write_sound_blaster_byte();
    sound_blaster_command_byte = sound_dma_transfer_count - 1;
    write_sound_blaster_byte();
    sound_blaster_command_byte = (sound_dma_transfer_count - 1) >> 8;
    write_sound_blaster_byte();
    for (wait_count = 0; wait_count < SB_DETECTION_POLL_LIMIT; wait_count++) {
        if (*(unsigned int *)test_buffer != SB_DMA_TEST_WORD) {
            sound_dma_test_result = 0;
        }
    }
    mask_sound_dma_channel();
    restore((unsigned char *)&dma_config);
    if (dma_candidates.dma_channels[candidate_index] == SOUND_CANDIDATE_END)
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
    registers.w.ax = DOS_GET_VERSION_FUNCTION;
    int386x(DOS_INTERRUPT, &registers, &registers, &segment_registers);
    dos_version_packed = (registers.h.al << 8) + registers.h.ah;
    return dos_version_query_succeeded = -1;
}
