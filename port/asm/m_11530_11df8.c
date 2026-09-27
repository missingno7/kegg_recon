/* m_11530_11df8.c - instruction-order translation of asm/m_11530_11df8.asm.
 *
 * ProTracker MOD parsing, row/tick effects, 8-bit PCM mixing, DMA setup, and the SB IRQ
 * path. Fixed-point carries, byte-width state, the 64-sample mixing groups, and DOS/DPMI
 * vector behavior follow the original 386 code. QUIRK comments call out its sharp edges.
 */
#include <stdint.h>
#include <string.h>
#include "../vhw/vhw.h"
#include "../include/watcom/i86.h"
#include "../include/ke_port.h"
#ifdef KE_ORACLE
#include "../oracle/oracle.h"
#endif

extern int inp(int port);
extern int outp(int port, int value);

#define MOD_MAGIC_MK                       0x2e4b2e4du
#define MOD_MAGIC_FLT4                     0x34544c46u
#define MOD_MAGIC_8CHN                     0x4e484338u
#define MOD_TAG_OFFSET                     0x438u
#define MOD_PATTERN_DATA_OFFSET            0x43cu
#define MOD_SONG_LENGTH_OFFSET             0x3b6u
#define MOD_ORDER_TABLE_OFFSET             0x3b8u
#define MOD_PATTERN_COUNT                  0x80u
#define MOD_PATTERN_ROWS                   0x40u
#define MOD_SAMPLE_COUNT                   0x1fu
#define MOD_SAMPLE_HEADER_BYTES            0x1eu
#define MOD_TITLE_BYTES                    0x14u
#define MOD_SAMPLE_SLOTS                   0x20u
#define TRACKER_CHANNEL_STATE_BYTES        0x16u
#define MOD_ORDER_POSITION_UNINITIALIZED   0xffu
#define TRACKER_FIXED_POINT_SHIFT          16u
#define TRACKER_SAMPLE_AREA_OFFSET         0x4200u
#define TRACKER_MEMORY_ALIGN_BIAS          0xffu
#define TRACKER_MEMORY_ALIGN_MASK          0xffffff00u
#define TRACKER_RENDER_CHUNK_MASK          0x3fu
#define TRACKER_RENDER_CHUNK_BOUNDARY      ((uint16_t)0xffc0u)
#define MOD_MIX_GROUP_MASK                 7u
#define MOD_MIX_GROUP_SHIFT                3u
#define MOD_CHANNEL_VOLUME_MASK            0xff00u
#define MOD_FOUR_CHANNELS                  4u
#define MOD_EIGHT_CHANNELS                 8u
#define MOD_DEFAULT_TICKS_PER_ROW          6u
#define MOD_DEFAULT_TEMPO_BPM              125u
#define MOD_MAX_VOLUME                     64u
#define MOD_TEMPO_THRESHOLD_BPM            32u
#define MOD_PERIOD_MASK                    0x0fffu
#define MOD_SAMPLE_CLOCK_HZ                0x372c00u
#define MOD_TICK_TIME_NUMERATOR            0x280u
#define MOD_MIN_LOOP_BYTES                 2u
#define MOD_EFFECT_SET_VOLUME              0x0cu
#define MOD_EFFECT_SET_SPEED_OR_TEMPO      0x0fu
#define MOD_EFFECT_POSITION_JUMP           0x0bu
#define MOD_EFFECT_PATTERN_BREAK           0x0du
#define MOD_EFFECT_SAMPLE_OFFSET           0x09u
#define MOD_EFFECT_VOLUME_SLIDE            0x0au
#define PLAYER_ERROR_BAD_MOD               0x602u
#define PLAYER_ERROR_DPMI_ALLOC            0x603u
#define PLAYER_ERROR_DSP_START             0x604u
#define DPMI_ALLOCATE_DOS_MEMORY           0x0100u
#define DPMI_FREE_DOS_MEMORY               0x0101u
#define DOS_SET_INTERRUPT_VECTOR           0x25u
#define DOS_GET_INTERRUPT_VECTOR           0x35u
#define DOS_SERVICES_INTERRUPT             0x21
#define DPMI_SERVICES_INTERRUPT            0x31
#define DOS_IRQ_MASTER_VECTOR_BASE         8u
#define DOS_IRQ_SLAVE_VECTOR_BASE          0x60u
#define TRACKER_DOS_MEMORY_PARAGRAPHS      0x434u
#define TRACKER_DMA_BUFFER_BYTES           0x120u
#define TRACKER_MIX_TABLE_CLEAR_BYTES      0x140u
#define TRACKER_MAX_POLL_COUNT             0x10000u
#define PCM_UNSIGNED_SILENCE               0x80u
#define SB_DSP_BUSY_MASK                   0x80u
#define SB_DSP_RESET_OFFSET                6u
#define SB_DSP_READ_DATA_OFFSET            0x0au
#define SB_DSP_WRITE_DATA_OFFSET           0x0cu
#define SB_DSP_READ_STATUS_OFFSET          0x0eu
#define SB_DSP_STATUS_TO_WRITE_DELTA       (-2)
#define SB_DSP_STATUS_TO_READ_DATA_DELTA   4u
#define SB_DSP_RESET_TO_STATUS_DELTA       8u
#define SB_DSP_CMD_SPEAKER_ON              0xd1u
#define SB_DSP_CMD_SET_TIME_CONSTANT       0x40u
#define SB_DSP_CMD_SINGLE_CYCLE_DMA        0x14u
#define SB_DSP_CMD_SPEAKER_OFF             0xd3u
#define SB_DSP_DMA_SINGLE_CYCLE_MODE       0x58u
#define SB_DSP_RESET_ACK                   0xaau
#define DMA_CHANNEL_MASK_PORT              0x0au
#define DMA_MODE_PORT                      0x0bu
#define DMA_CLEAR_FLIP_FLOP_PORT           0x0cu
#define DMA_CHANNEL_MASK_BIT               4u
#define DMA_PAGE_PORTS_PACKED              0x82818387u
#define PIC_MASTER_MASK_PORT               0x21u
#define PIC_SLAVE_MASK_PORT                0xa1u
#define PIC_MASTER_EOI_PORT                0x20u
#define PIC_SLAVE_EOI_PORT                 0xa0u
#define PIC_END_OF_INTERRUPT_COMMAND       0x20u
#define PIC_IRQ_BIT_BASE                   1u

#define SAMPLE_POSITION_OFFSET             0u
#define SAMPLE_FRACTION_OFFSET             4u
#define SAMPLE_LOOP_START_OFFSET           8u
#define SAMPLE_LOOP_END_OFFSET             12u
#define SAMPLE_PERIOD_OFFSET               16u
#define CHANNEL_VOLUME_OFFSET              20u
#define ROW_SAMPLE_NUMBER_OFFSET           2u
#define ROW_VOLUME_OFFSET                  3u
#define ROW_EFFECT_OFFSET                  4u

/* The exact public _DATA labels from the TASM module. */
uint16_t module_sound_io_base;
uint8_t module_sound_irq_number;
uint8_t module_sound_dma_channel;
uint16_t module_sample_rate;
uint32_t module_saved_irq_vector_offset;
uint16_t module_saved_irq_vector_segment;
uint16_t tracker_dos_memory_selector;
uint32_t module_volume_mix_table;
uint32_t module_audio_buffer;
uint16_t module_audio_buffer_bytes;
uint16_t module_audio_buffer_half_offset;
uint16_t module_audio_buffer_remaining_bytes;
uint16_t samples_until_next_tracker_tick;
uint16_t samples_per_tracker_tick;
uint32_t tracker_sample_period_scale;
uint16_t module_channel_count;
uint8_t module_channel_0_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t module_channel_1_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t module_channel_2_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t module_channel_3_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t module_channel_4_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t module_channel_5_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t module_channel_6_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t module_channel_7_state[TRACKER_CHANNEL_STATE_BYTES];
uint8_t *module_channel_state_table[8] = {
    module_channel_0_state, module_channel_1_state, module_channel_2_state,
    module_channel_3_state, module_channel_4_state, module_channel_5_state,
    module_channel_6_state, module_channel_7_state
};
uint8_t module_order_position;
uint8_t module_song_length;
uint8_t module_tick_counter;
uint8_t module_pattern_order_table[MOD_PATTERN_COUNT];
uint32_t module_current_pattern_row;
uint8_t module_ticks_per_row;
uint8_t module_row_tick_countdown;
uint8_t module_tempo_bpm;
uint8_t module_channel_row_events[8 * 6];
uint32_t module_pattern_addresses[MOD_PATTERN_COUNT];
uint32_t module_sample_addresses[MOD_SAMPLE_SLOTS];
uint32_t module_sample_loop_starts[MOD_SAMPLE_SLOTS];
uint32_t module_sample_loop_ends[MOD_SAMPLE_SLOTS];
uint8_t module_sample_volumes[MOD_SAMPLE_SLOTS];
uint32_t module_player_error_code;
uint32_t module_saved_es_segment;

static uint8_t *channel_state(unsigned channel)
{
    return module_channel_state_table[channel];
}

void protracker_irq_handler(void);
void stop_protracker_module(void);

static uint8_t read_u8(uint32_t address)
{
    return *(const volatile uint8_t *)(uintptr_t)address;
}

static uint16_t read_le16(uint32_t address)
{
    return (uint16_t)(read_u8(address) | ((uint16_t)read_u8(address + 1u) << 8));
}

static uint32_t read_le32(uint32_t address)
{
    return (uint32_t)read_u8(address) |
           ((uint32_t)read_u8(address + 1u) << 8) |
           ((uint32_t)read_u8(address + 2u) << 16) |
           ((uint32_t)read_u8(address + 3u) << 24);
}

static uint16_t read_be16(uint32_t address)
{
    return (uint16_t)(((uint16_t)read_u8(address) << 8) | read_u8(address + 1u));
}

static void write_u32(uint8_t *p, unsigned offset, uint32_t value)
{
    memcpy(p + offset, &value, sizeof value);
}

static uint32_t state_u32(const uint8_t *p, unsigned offset)
{
    uint32_t value;
    memcpy(&value, p + offset, sizeof value);
    return value;
}

static uint16_t state_u16(const uint8_t *p, unsigned offset)
{
    uint16_t value;
    memcpy(&value, p + offset, sizeof value);
    return value;
}

static uint16_t io_in(uint16_t port)
{
    uint8_t value = (uint8_t)inp(port);
#ifdef KE_ORACLE
    oracle_trace_add('I', port, value, 1);
#endif
    return value;
}

static void io_out(uint16_t port, uint8_t value)
{
#ifdef KE_ORACLE
    oracle_trace_add('O', port, value, 1);
#endif
    (void)outp(port, value);
}

static int issue_interrupt(int interrupt_number, union REGS *registers, struct SREGS *segments)
{
#ifdef KE_ORACLE
    oracle_trace_add('N', (uint16_t)interrupt_number, registers->x.eax, 4);
#endif
    return int386x(interrupt_number, registers, registers, segments);
}

static uint16_t sample_volume_scale(unsigned volume, uint8_t sample)
{
    int16_t product = (int16_t)((int16_t)(int8_t)sample * (int16_t)volume);
    unsigned shift = module_channel_count >> MOD_MIX_GROUP_SHIFT;
    return (uint16_t)(product >> shift);
}

static int parse_mod_header(uint32_t image)
{
    uint32_t max_pattern = 0, pattern_bytes, sample_data, sample_header;
    unsigned i;
    module_channel_count = MOD_FOUR_CHANNELS;
    if (read_le32(image + MOD_TAG_OFFSET) == MOD_MAGIC_MK ||
        read_le32(image + MOD_TAG_OFFSET) == MOD_MAGIC_FLT4) {
        /* Recognized four-channel layouts. */
    } else {
        module_channel_count = MOD_EIGHT_CHANNELS;
        if (read_le32(image + MOD_TAG_OFFSET) != MOD_MAGIC_8CHN)
            return 1;                     /* carry set, but the count remains eight */
    }

    module_order_position = MOD_ORDER_POSITION_UNINITIALIZED;
    module_tick_counter = MOD_PATTERN_ROWS;
    module_ticks_per_row = MOD_DEFAULT_TICKS_PER_ROW;
    module_row_tick_countdown = 0;
    module_tempo_bpm = MOD_DEFAULT_TEMPO_BPM;
    module_song_length = read_u8(image + MOD_SONG_LENGTH_OFFSET);
    for (i = 0; i < MOD_PATTERN_COUNT; i++) {
        uint8_t order = read_u8(image + MOD_ORDER_TABLE_OFFSET + i);
        module_pattern_order_table[i] = order;
        if (order > max_pattern)
            max_pattern = order;
    }

    pattern_bytes = (uint32_t)module_channel_count * 0x100u;
    sample_data = image + MOD_PATTERN_DATA_OFFSET + (max_pattern + 1u) * pattern_bytes;
    for (i = 0; i <= max_pattern; i++)
        module_pattern_addresses[i] = image + MOD_PATTERN_DATA_OFFSET + i * pattern_bytes;

    sample_header = image + MOD_TITLE_BYTES;
    for (i = 1; i <= MOD_SAMPLE_COUNT; i++) {
        uint32_t length = (uint32_t)read_be16(sample_header + 22u) * 2u;
        uint32_t loop_length = (uint32_t)read_be16(sample_header + 28u) * 2u;
        uint32_t loop_start = (uint32_t)read_be16(sample_header + 26u) * 2u;
        uint32_t end;
        module_sample_volumes[i] = read_u8(sample_header + 25u);
        if (loop_length <= MOD_MIN_LOOP_BYTES) {
            loop_length = 0;
            loop_start = length;
        }
        end = loop_start + loop_length;
        module_sample_addresses[i] = sample_data;
        module_sample_loop_starts[i] = sample_data + loop_start;
        module_sample_loop_ends[i] = sample_data + end;
        sample_data += length;
        sample_header += MOD_SAMPLE_HEADER_BYTES;
    }
    return 0;
}

static void set_channel_volume(unsigned channel, uint8_t volume)
{
    channel_state(channel)[CHANNEL_VOLUME_OFFSET] = volume;
}

static void calculate_channel_sample_period(unsigned channel, uint16_t period)
{
    if (period != 0)
        write_u32(channel_state(channel), SAMPLE_PERIOD_OFFSET,
                  tracker_sample_period_scale / period);
}

static void set_channel_sample_bounds(unsigned channel, uint32_t sample,
                                      uint32_t loop_start, uint32_t loop_end)
{
    uint8_t *state = channel_state(channel);
    write_u32(state, SAMPLE_POSITION_OFFSET, sample);
    write_u32(state, SAMPLE_LOOP_START_OFFSET, loop_start);
    write_u32(state, SAMPLE_LOOP_END_OFFSET, loop_end);
}

static void update_tick_period_for_tempo(uint8_t tempo)
{
    uint32_t dividend = (uint32_t)module_sample_rate * MOD_TICK_TIME_NUMERATOR;
    uint16_t divisor = (uint16_t)((uint16_t)tempo << 8);
    samples_per_tracker_tick = (uint16_t)(dividend / divisor);
}

static void apply_channel_tick_effect(unsigned channel)
{
    uint8_t *event = module_channel_row_events + channel * 6u;
    uint16_t effect = state_u16(event, ROW_EFFECT_OFFSET);
    if ((uint8_t)(effect >> 8) == MOD_EFFECT_VOLUME_SLIDE) {
        uint8_t parameter = (uint8_t)effect;
        uint8_t volume = event[ROW_VOLUME_OFFSET];
        if (parameter & 0xf0u) {
            unsigned raised = (unsigned)volume + (parameter >> 4);
            volume = (uint8_t)(raised <= MOD_MAX_VOLUME ? raised : MOD_MAX_VOLUME);
        } else if (volume < parameter) {
            volume = 0;
        } else {
            volume = (uint8_t)(volume - parameter);
        }
        event[ROW_VOLUME_OFFSET] = volume;
        set_channel_volume(channel, volume);
    }
}

static void decode_channel_pattern_event(unsigned channel, uint32_t event_address)
{
    uint8_t *event = module_channel_row_events + channel * 6u;
    uint8_t instrument_effect = read_u8(event_address + 2u);
    uint8_t sample_number = (uint8_t)((instrument_effect >> 4) |
                                      (read_u8(event_address) & 0xf0u));
    uint16_t period;
    uint16_t effect;
    if (sample_number != 0) {
        event[ROW_SAMPLE_NUMBER_OFFSET] = sample_number;
        event[ROW_VOLUME_OFFSET] = module_sample_volumes[sample_number];
        set_channel_volume(channel, event[ROW_VOLUME_OFFSET]);
    }
    period = (uint16_t)(((uint16_t)read_u8(event_address) << 8) |
                        read_u8(event_address + 1u));
    period &= MOD_PERIOD_MASK;
    if (period != 0) {
        uint8_t current_sample = event[ROW_SAMPLE_NUMBER_OFFSET];
        event[0] = (uint8_t)period;
        event[1] = (uint8_t)(period >> 8);
        calculate_channel_sample_period(channel, period);
        set_channel_sample_bounds(channel, module_sample_addresses[current_sample],
                                  module_sample_loop_starts[current_sample],
                                  module_sample_loop_ends[current_sample]);
    }
    effect = (uint16_t)(((uint16_t)(instrument_effect & 0x0fu) << 8) |
                        read_u8(event_address + 3u));
    memcpy(event + ROW_EFFECT_OFFSET, &effect, sizeof effect);
    switch ((uint8_t)(effect >> 8)) {
    case MOD_EFFECT_SET_VOLUME:
        set_channel_volume(channel, (uint8_t)effect);
        break;
    case MOD_EFFECT_SET_SPEED_OR_TEMPO: {
        uint8_t value = (uint8_t)effect;
        if (value == 0)
            break;
        if (value < MOD_TEMPO_THRESHOLD_BPM) {
            module_ticks_per_row = value;
            module_row_tick_countdown = value;
        } else {
            module_tempo_bpm = value;
            update_tick_period_for_tempo(value);
        }
        break;
    }
    case MOD_EFFECT_POSITION_JUMP:
        module_order_position = (uint8_t)((uint8_t)effect - 1u);
        module_tick_counter = MOD_PATTERN_ROWS;
        break;
    case MOD_EFFECT_PATTERN_BREAK:
        module_tick_counter = MOD_PATTERN_ROWS;
        break;
    case MOD_EFFECT_SAMPLE_OFFSET: {
        uint8_t current_sample = event[ROW_SAMPLE_NUMBER_OFFSET];
        uint32_t sample = module_sample_addresses[current_sample] +
                          ((uint32_t)(uint8_t)effect << 8);
        set_channel_sample_bounds(channel, sample,
                                  module_sample_loop_starts[current_sample],
                                  module_sample_loop_ends[current_sample]);
        break;
    }
    default:
        break;
    }
}

static void advance_protracker_tick(void)
{
    module_row_tick_countdown = (uint8_t)(module_row_tick_countdown - 1u);
    if ((int8_t)module_row_tick_countdown > 0) {
        unsigned channel;
        for (channel = 0; channel < module_channel_count; channel++)
            apply_channel_tick_effect(channel);
        return;
    }

    module_row_tick_countdown = module_ticks_per_row;
    module_tick_counter = (uint8_t)(module_tick_counter + 1u);
    if (module_tick_counter >= MOD_PATTERN_ROWS) {
        uint8_t order;
        module_tick_counter = 0;
        order = (uint8_t)(module_order_position + 1u);
        if (order >= module_song_length)
            order = 0;
        module_order_position = order;
        order = module_pattern_order_table[order];
        module_current_pattern_row = module_pattern_addresses[order];
    }

    {
        uint32_t row = module_current_pattern_row;
        unsigned channel;
        for (channel = 0; channel < module_channel_count; channel++) {
            decode_channel_pattern_event(channel, row);
            row += 4u;
        }
        module_current_pattern_row = row;
    }
}

static int dpmi_interrupt(union REGS *r, struct SREGS *s)
{
    return issue_interrupt(DPMI_SERVICES_INTERRUPT, r, s);
}

static int dos_interrupt(union REGS *r, struct SREGS *s)
{
    return issue_interrupt(DOS_SERVICES_INTERRUPT, r, s);
}

static int allocate_tracker_memory(void)
{
    union REGS r;
    struct SREGS s;
    uint32_t base, table, buffer;
    uint16_t negative_base;
    uint16_t segment_linear;
    uint8_t *mix_table;
    uint8_t *audio;
    struct SREGS current_segments;
    unsigned volume, sample;
    memset(&r, 0, sizeof r);
    memset(&s, 0, sizeof s);
    module_audio_buffer_half_offset = 0;
    samples_until_next_tracker_tick = 0;
    samples_per_tracker_tick = 0;
    tracker_dos_memory_selector = 0;
    r.w.ax = DPMI_ALLOCATE_DOS_MEMORY;
    r.w.bx = TRACKER_DOS_MEMORY_PARAGRAPHS;
    (void)dpmi_interrupt(&r, &s);
    if (r.x.cflag & 1u)
        return 1;
    tracker_dos_memory_selector = r.w.dx;
    base = (uint32_t)r.w.ax << 4;
    table = base + TRACKER_MIX_TABLE_CLEAR_BYTES;
    buffer = base;
    segment_linear = (uint16_t)((uint16_t)r.w.ax << 4);
    negative_base = (uint16_t)(0u - segment_linear);
    if (negative_base < TRACKER_MIX_TABLE_CLEAR_BYTES) {
        table = base;
        buffer += TRACKER_SAMPLE_AREA_OFFSET;
    }
    table = (table + TRACKER_MEMORY_ALIGN_BIAS) & TRACKER_MEMORY_ALIGN_MASK;
    module_volume_mix_table = table;
    module_audio_buffer = buffer;
    module_audio_buffer_bytes = TRACKER_DMA_BUFFER_BYTES;
    segread(&current_segments);
    module_saved_es_segment = current_segments.es;
    audio = (uint8_t *)(uintptr_t)module_audio_buffer;
    memset(audio, PCM_UNSIGNED_SILENCE, module_audio_buffer_bytes);
    mix_table = (uint8_t *)(uintptr_t)module_volume_mix_table;
    for (volume = 0; volume <= MOD_MAX_VOLUME; volume++) {
        for (sample = 0; sample < 256; sample++) {
            uint16_t scaled = sample_volume_scale(volume, (uint8_t)sample);
            mix_table[volume * 256u + sample] = (uint8_t)(scaled >> 8);
        }
    }
    return 0;
}

static void free_tracker_memory(void)
{
    union REGS r;
    struct SREGS s;
    if (tracker_dos_memory_selector == 0)
        return;
    memset(&r, 0, sizeof r);
    memset(&s, 0, sizeof s);
    r.w.ax = DPMI_FREE_DOS_MEMORY;
    r.w.dx = tracker_dos_memory_selector;
    (void)dpmi_interrupt(&r, &s);
    tracker_dos_memory_selector = 0;
}

static void mix_channel_samples(unsigned channel, uint8_t *output, unsigned count)
{
    uint8_t *state = channel_state(channel);
    uint32_t position = state_u32(state, SAMPLE_POSITION_OFFSET);
    uint32_t fraction = state_u32(state, SAMPLE_FRACTION_OFFSET);
    uint32_t loop_start = state_u32(state, SAMPLE_LOOP_START_OFFSET);
    uint32_t loop_end = state_u32(state, SAMPLE_LOOP_END_OFFSET);
    uint32_t loop_length = loop_end - loop_start;
    uint32_t period = state_u32(state, SAMPLE_PERIOD_OFFSET);
    uint64_t fixed_step = (uint64_t)period << 16;
    uint32_t step_fraction = (uint32_t)fixed_step;
    uint32_t step_integer = (uint32_t)(fixed_step >> 32);
    uint32_t volume_base = module_volume_mix_table +
                           ((uint32_t)state[CHANNEL_VOLUME_OFFSET] << 8);
    unsigned group, sample_in_group;
    if ((count & MOD_MIX_GROUP_MASK) != 0)
        return;                             /* original's lookup alignment loop never exits */
    for (group = 0; group < count; group += 8u) {
        if (position >= loop_end) {
            position -= loop_length;
            if (position >= loop_end)
                break;                      /* one wrap only, as the original branch does */
        }
        /* QUIRK: the original checks the loop boundary once per unrolled eight-sample group. */
        for (sample_in_group = 0; sample_in_group < 8u; sample_in_group++) {
            uint32_t old_fraction;
            uint8_t sample = read_u8(position);
            old_fraction = fraction;
            fraction += step_fraction;
            position += step_integer + (fraction < old_fraction ? 1u : 0u);
            output[group + sample_in_group] =
                (uint8_t)(output[group + sample_in_group] + read_u8(volume_base + sample));
        }
    }
    write_u32(state, SAMPLE_POSITION_OFFSET, position);
    write_u32(state, SAMPLE_FRACTION_OFFSET, fraction);
}

static void render_sample_buffer(void)
{
    uint16_t half = (uint16_t)(module_audio_buffer_bytes >> 1);
    uint16_t offset = module_audio_buffer_half_offset;
    uint8_t *output = (uint8_t *)(uintptr_t)(module_audio_buffer + offset);
    unsigned remaining = half;
    module_audio_buffer_remaining_bytes = half;
    module_audio_buffer_half_offset = (uint16_t)(offset ^ half);
    {
        struct SREGS segments;
        segread(&segments);
        module_saved_es_segment = segments.es;
    }
    memset(output, PCM_UNSIGNED_SILENCE, half);
    while (remaining != 0) {
        uint16_t amount;
        uint16_t rounded;
        uint16_t row_samples;
        unsigned channel;
        if ((int16_t)samples_until_next_tracker_tick <= 0) {
            advance_protracker_tick();
            samples_until_next_tracker_tick = (uint16_t)(samples_until_next_tracker_tick +
                                                          samples_per_tracker_tick);
        }
        row_samples = samples_until_next_tracker_tick;
        rounded = (uint16_t)((row_samples + TRACKER_RENDER_CHUNK_MASK) &
                             TRACKER_RENDER_CHUNK_BOUNDARY);
        if ((int16_t)module_audio_buffer_remaining_bytes <= (int16_t)rounded)
            amount = module_audio_buffer_remaining_bytes;
        else
            amount = rounded;
        module_audio_buffer_remaining_bytes = (uint16_t)(module_audio_buffer_remaining_bytes - amount);
        samples_until_next_tracker_tick = (uint16_t)(samples_until_next_tracker_tick - amount);
        for (channel = 0; channel < module_channel_count; channel++)
            mix_channel_samples(channel, output, amount);
        output += amount;
        remaining = module_audio_buffer_remaining_bytes;
    }
}

static void write_sound_blaster_dsp_byte(uint8_t value)
{
    uint16_t port = (uint16_t)(module_sound_io_base + SB_DSP_WRITE_DATA_OFFSET);
    unsigned count;
    for (count = TRACKER_MAX_POLL_COUNT; count != 0; count--)
        if ((io_in(port) & SB_DSP_BUSY_MASK) == 0)
            break;
    io_out(port, value);                    /* QUIRK: even a timed-out poll writes the byte */
}

static int reset_sound_blaster_dsp(void)
{
    uint16_t port = (uint16_t)(module_sound_io_base + SB_DSP_RESET_OFFSET);
    uint8_t response = 0;
    unsigned count;
    io_out(port, 1);
    (void)io_in(port); (void)io_in(port); (void)io_in(port); (void)io_in(port);
    io_out(port, 0);
    port = (uint16_t)(port + SB_DSP_RESET_TO_STATUS_DELTA);
    for (count = TRACKER_MAX_POLL_COUNT; count != 0; count--) {
        response = (uint8_t)(io_in(port) & SB_DSP_BUSY_MASK);
        if (response != 0)
            break;
    }
    port = (uint16_t)(port - SB_DSP_STATUS_TO_READ_DATA_DELTA);
    response = (uint8_t)io_in(port);
    return response != SB_DSP_RESET_ACK;    /* carry set unless the response is 0AAh */
}

static int start_sound_blaster_playback(void)
{
    uint32_t dividend;
    uint16_t quotient, time_constant;
    if (reset_sound_blaster_dsp())
        return 1;
    write_sound_blaster_dsp_byte(SB_DSP_CMD_SPEAKER_ON);
    write_sound_blaster_dsp_byte(SB_DSP_CMD_SET_TIME_CONSTANT);
    dividend = 1000u * 1000u;
    quotient = (uint16_t)(dividend / module_sample_rate);
    time_constant = (uint16_t)(0u - quotient);
    write_sound_blaster_dsp_byte((uint8_t)time_constant);
    write_sound_blaster_dsp_byte(SB_DSP_CMD_SINGLE_CYCLE_DMA);
    quotient = (uint16_t)(module_audio_buffer_bytes >> 1);
    quotient = (uint16_t)(quotient - 1u);
    write_sound_blaster_dsp_byte((uint8_t)quotient);
    write_sound_blaster_dsp_byte((uint8_t)(quotient >> 8));
    return 0;
}

static void stop_sound_blaster_playback(void)
{
    (void)reset_sound_blaster_dsp();
    write_sound_blaster_dsp_byte(SB_DSP_CMD_SPEAKER_OFF);
}

static void mask_dma_channel(void)
{
    io_out(DMA_CHANNEL_MASK_PORT,
           (uint8_t)(module_sound_dma_channel | DMA_CHANNEL_MASK_BIT));
}

static void program_dma_audio_buffer(void)
{
    uint8_t channel = module_sound_dma_channel;
    uint8_t al = (uint8_t)(channel | DMA_CHANNEL_MASK_BIT);
    uint16_t dx;
    uint16_t count;
    uint32_t eax = module_audio_buffer;
    uint32_t edx;
    io_out(DMA_CHANNEL_MASK_PORT, al);
    io_out(DMA_CLEAR_FLIP_FLOP_PORT, al);
    io_out(DMA_MODE_PORT, (uint8_t)(channel | SB_DSP_DMA_SINGLE_CYCLE_MODE));
    dx = (uint16_t)((uint16_t)channel * 2u);
    io_out(dx, (uint8_t)eax);
    io_out(dx, (uint8_t)(eax >> 8));
    dx = (uint16_t)(dx + 1u);
    count = (uint16_t)(module_audio_buffer_bytes - 1u);
    io_out(dx, (uint8_t)count);
    io_out(dx, (uint8_t)(count >> 8));
    edx = DMA_PAGE_PORTS_PACKED >> ((channel << MOD_MIX_GROUP_SHIFT) & 31u);
    dx = (uint16_t)edx;
    dx &= 0x00ffu;
    eax >>= TRACKER_FIXED_POINT_SHIFT;
    io_out(dx, (uint8_t)eax);
    io_out(DMA_CHANNEL_MASK_PORT, channel);
}

static uint8_t dos_vector_number(uint8_t irq)
{
    uint8_t al = irq;
    if (al >= 8u)
        al = (uint8_t)(al + DOS_IRQ_SLAVE_VECTOR_BASE);
    return (uint8_t)(al + DOS_IRQ_MASTER_VECTOR_BASE);
}

static void int21_set_vector(uint8_t vector, uint16_t selector, uint32_t offset)
{
    union REGS r;
    struct SREGS s;
    memset(&r, 0, sizeof r);
    memset(&s, 0, sizeof s);
    r.w.ax = (uint16_t)((DOS_SET_INTERRUPT_VECTOR << 8) | vector);
    r.x.edx = offset;
    s.ds = selector;
    (void)dos_interrupt(&r, &s);
}

static void int21_get_vector(uint8_t vector, uint16_t *selector, uint32_t *offset)
{
    union REGS r;
    struct SREGS s;
    memset(&r, 0, sizeof r);
    memset(&s, 0, sizeof s);
    r.w.ax = (uint16_t)((DOS_GET_INTERRUPT_VECTOR << 8) | vector);
    (void)dos_interrupt(&r, &s);
    *selector = s.es;
    *offset = r.x.ebx;
}

/* Restore the saved vector and mask the IRQ. The source calls this "install". */
static void install_protracker_irq(void)
{
    uint8_t slave_mask = (uint8_t)io_in(PIC_SLAVE_MASK_PORT);
    uint8_t master_mask;
    uint8_t irq = module_sound_irq_number;
    uint16_t mask = (uint16_t)(PIC_IRQ_BIT_BASE << (irq & 31u));
    uint16_t ax;
    master_mask = (uint8_t)io_in(PIC_MASTER_MASK_PORT);
    ax = (uint16_t)(master_mask | ((uint16_t)slave_mask << 8));
    ax |= mask;
    io_out(PIC_MASTER_MASK_PORT, (uint8_t)ax);
    io_out(PIC_SLAVE_MASK_PORT, (uint8_t)(ax >> 8));
    if ((module_saved_irq_vector_segment | module_saved_irq_vector_offset) != 0)
        int21_set_vector(dos_vector_number(irq), module_saved_irq_vector_segment,
                         module_saved_irq_vector_offset);
    module_saved_irq_vector_offset = 0;
    module_saved_irq_vector_segment = 0;
}

/* Save the current vector and install the tracker ISR; the source calls this "uninstall". */
static void uninstall_protracker_irq(void)
{
    uint8_t irq = module_sound_irq_number;
    uint16_t saved_selector;
    uint16_t mask = (uint16_t)(PIC_IRQ_BIT_BASE << (irq & 31u));
    uint32_t saved_offset;
    uint16_t ax;
    struct SREGS current_segments;
    int21_get_vector(dos_vector_number(irq), &saved_selector, &saved_offset);
    module_saved_irq_vector_offset = saved_offset;
    module_saved_irq_vector_segment = saved_selector;
    int21_set_vector(dos_vector_number(irq), KE_FLAT_SELECTOR,
                     (uint32_t)(uintptr_t)protracker_irq_handler);
    /* The original restores EAX from its saved DS value before this mask update. */
    segread(&current_segments);
    (void)io_in(PIC_SLAVE_MASK_PORT);
    ax = (uint16_t)(((uint16_t)(current_segments.ds >> 8) << 8) |
                    (uint8_t)io_in(PIC_MASTER_MASK_PORT));
    ax &= (uint16_t)~mask;
    io_out(PIC_MASTER_MASK_PORT, (uint8_t)ax);
    io_out(PIC_SLAVE_MASK_PORT, (uint8_t)(ax >> 8));
}

static void advance_and_mix_irq_buffer(void)
{
    render_sample_buffer();
}

int load_protracker_module(int file_image, int sample_rate, int dsp_base, int irq, int dma_channel)
{
    uint32_t image = (uint32_t)(uintptr_t)file_image;
    uint32_t clock_scale;
    module_sound_io_base = (uint16_t)dsp_base;
    module_sound_irq_number = (uint8_t)irq;
    module_sound_dma_channel = (uint8_t)dma_channel;
    module_sample_rate = (uint16_t)sample_rate;
    clock_scale = (uint32_t)(((uint64_t)MOD_SAMPLE_CLOCK_HZ << TRACKER_FIXED_POINT_SHIFT) /
                             module_sample_rate);
    tracker_sample_period_scale = clock_scale;
    module_player_error_code = PLAYER_ERROR_BAD_MOD;
    if (parse_mod_header(image))
        return (int)module_player_error_code;
    module_player_error_code = PLAYER_ERROR_DPMI_ALLOC;
    if (allocate_tracker_memory())
        return (int)module_player_error_code;
    update_tick_period_for_tempo(module_tempo_bpm);
    program_dma_audio_buffer();
    uninstall_protracker_irq();
    module_player_error_code = 0;
    if (start_sound_blaster_playback()) {
        module_player_error_code = PLAYER_ERROR_DSP_START;
        stop_protracker_module();
    }
    return (int)module_player_error_code;
}

void stop_protracker_module(void)
{
    stop_sound_blaster_playback();
    install_protracker_irq();
    mask_dma_channel();
    free_tracker_memory();
}

void protracker_irq_handler(void)
{
    uint16_t write_port = (uint16_t)(module_sound_io_base + SB_DSP_WRITE_DATA_OFFSET);
    (void)io_in((uint16_t)(module_sound_io_base + SB_DSP_READ_STATUS_OFFSET));
    write_sound_blaster_dsp_byte(SB_DSP_CMD_SINGLE_CYCLE_DMA);
    {
        uint16_t half_count = (uint16_t)(module_audio_buffer_bytes >> 1);
        uint16_t count = (uint16_t)(half_count - 1u);
        /* Keep the three per-command wait loops and their poll order. */
        unsigned poll;
        for (poll = TRACKER_MAX_POLL_COUNT; poll != 0; poll--)
            if ((io_in(write_port) & SB_DSP_BUSY_MASK) == 0)
                break;
        io_out(write_port, (uint8_t)count);
        for (poll = TRACKER_MAX_POLL_COUNT; poll != 0; poll--)
            if ((io_in(write_port) & SB_DSP_BUSY_MASK) == 0)
                break;
        io_out(write_port, (uint8_t)(count >> 8));
    }
    if ((int8_t)module_sound_irq_number >= 8)
        io_out(PIC_SLAVE_EOI_PORT, PIC_END_OF_INTERRUPT_COMMAND);
    io_out(PIC_MASTER_EOI_PORT, PIC_END_OF_INTERRUPT_COMMAND);
    /* Original executes STI before rendering the next DMA half-buffer. */
#ifdef KE_ORACLE
    oracle_trace_add('S', 0, 0, 0);
#endif
    vhw_enter();
    vcpu_sti();
    vhw_leave();
    advance_and_mix_irq_buffer();
}
