/* Differential tests for the ProTracker parser, DPMI IRQ setup, and mixer. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"
#include "../include/watcom/i86.h"

extern int load_protracker_module(int image, int sample_rate, int dsp_base, int irq, int dma);
extern void stop_protracker_module(void);
extern void protracker_irq_handler(void);
extern uint16_t module_sound_io_base, module_sample_rate, module_channel_count;
extern uint8_t module_sound_irq_number, module_sound_dma_channel;
extern uint32_t module_saved_irq_vector_offset, module_volume_mix_table, module_audio_buffer;
extern uint16_t module_saved_irq_vector_segment, tracker_dos_memory_selector;
extern uint16_t module_audio_buffer_bytes, module_audio_buffer_half_offset;
extern uint16_t module_audio_buffer_remaining_bytes, samples_until_next_tracker_tick;
extern uint16_t samples_per_tracker_tick;
extern uint32_t tracker_sample_period_scale, module_current_pattern_row;
extern uint8_t module_order_position, module_song_length, module_tick_counter;
extern uint8_t module_ticks_per_row, module_row_tick_countdown, module_tempo_bpm;
extern uint32_t module_player_error_code, module_saved_es_segment;
extern uint8_t module_channel_0_state[22], module_channel_1_state[22];
extern uint8_t module_channel_2_state[22], module_channel_3_state[22];
extern uint8_t module_channel_4_state[22], module_channel_5_state[22];
extern uint8_t module_channel_6_state[22], module_channel_7_state[22];
extern uint8_t module_pattern_order_table[128], module_channel_row_events[48];
extern uint32_t module_pattern_addresses[128], module_sample_addresses[32];
extern uint32_t module_sample_loop_starts[32], module_sample_loop_ends[32];
extern uint8_t module_sample_volumes[32];

#define TEST_DSP_BASE 0x220u
#define TEST_IRQ 7u
#define TEST_DMA 1u
#define MOD_IMAGE_BYTES 8192u
#define MOD_MIX_TABLE_BYTES (65u * 256u)
#define MOD_DMA_BUFFER_BYTES 0x120u
#define MAX_TRACE 8192

typedef struct TrackerSnapshot {
    uint16_t io_base, sample_rate, channel_count, saved_segment, selector;
    uint16_t audio_bytes, audio_half, audio_remaining, samples_until, samples_per_tick;
    uint8_t irq, dma, order_position, song_length, tick_counter;
    uint8_t ticks_per_row, row_tick_countdown, tempo;
    uint32_t saved_offset, volume_table, audio_buffer, sample_scale, current_row;
    uint32_t player_error, saved_es;
    uint8_t channel_states[8][22];
    uint8_t order_table[128], row_events[48], sample_volumes[32];
    uint32_t pattern_addresses[128], sample_addresses[32];
    uint32_t sample_loop_starts[32], sample_loop_ends[32];
    uint8_t mix_table[MOD_MIX_TABLE_BYTES];
    uint8_t audio[MOD_DMA_BUFFER_BYTES];
} TrackerSnapshot;

typedef struct ModFixture {
    uint8_t bytes[MOD_IMAGE_BYTES];
    uint32_t size;
} ModFixture;

typedef struct TrackerFrame {
    uint16_t audio_half, audio_remaining;
    uint32_t audio_address;
    uint16_t samples_until, samples_per_tick;
    uint8_t tick_counter, row_tick_countdown, tempo;
    uint32_t current_row;
    uint8_t channel_states[8][22];
    uint8_t row_events[48];
    uint8_t audio[MOD_DMA_BUFFER_BYTES];
} TrackerFrame;

static OracleEvent traces[2][MAX_TRACE];
static int trace_counts[2];
static uint32_t random_state;
static uint8_t dsp_reset_ack = 0xaa;
static int reported_native_selector_context;

static void get_native_ds_es(uint16_t *ds, uint16_t *es)
{
    __asm__ volatile("movw %%ds, %0\n\tmovw %%es, %1" : "=r"(*ds), "=r"(*es));
}

static uint32_t random_u32(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static uint32_t fake_in(void *ctx, uint16_t port, int size)
{
    uint32_t value;
    (void)ctx;
    (void)size;
    if (port == TEST_DSP_BASE + 0x06u)
        value = 0;
    else if (port == TEST_DSP_BASE + 0x0au)
        value = dsp_reset_ack;
    else if (port == TEST_DSP_BASE + 0x0cu)
        value = 0;
    else if (port == TEST_DSP_BASE + 0x0eu)
        value = 0x80;
    else if (port == 0x21u)
        value = 0xb8;
    else if (port == 0xa1u)
        value = 0x9d;
    else
        value = 0xff;
    return value;
}

static void fake_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    (void)ctx;
    (void)size;
}

static uint32_t fake_oracle_in(uint16_t port, int size)
{
    return fake_in(NULL, port, size);
}

static void fake_oracle_out(uint16_t port, uint32_t value, int size)
{
    fake_out(NULL, port, value, size);
}

static void setup_tracker_ports(void)
{
    vhw_set_port_override(fake_in, fake_out, NULL);
    oracle_set_port_hooks(fake_oracle_in, fake_oracle_out);
}

static void clear_tracker_ports(void)
{
    vhw_set_port_override(NULL, NULL, NULL);
    oracle_set_port_hooks(NULL, NULL);
}

static int prepare_tracker_environment(void)
{
    setup_tracker_ports();
    if (lowmem_init() != 0) {
        clear_tracker_ports();
        return -1;
    }
    return 0;
}

static void set_event(uint8_t *event, unsigned sample, unsigned period,
                      unsigned effect, unsigned parameter)
{
    event[0] = (uint8_t)(((sample & 0xf0u) | ((period >> 8) & 0x0fu)));
    event[1] = (uint8_t)period;
    event[2] = (uint8_t)(((sample & 0x0fu) << 4) | (effect & 0x0fu));
    event[3] = (uint8_t)parameter;
}

static void make_fixture(ModFixture *fixture, unsigned channels, uint32_t seed,
                         int position_jump, int pattern_break)
{
    static const uint16_t periods[] = { 428, 404, 381, 360, 339, 320, 303, 285 };
    uint32_t pattern_size = channels * 256u;
    uint32_t sample_offset = 0x43cu + pattern_size;
    uint8_t *pattern;
    unsigned i, row, channel, sample;
    random_state = seed;
    memset(fixture, 0, sizeof *fixture);
    fixture->size = sample_offset + 3u * 512u;
    fixture->bytes[0x3b6] = 1;
    fixture->bytes[0x3b8] = 0;
    memcpy(fixture->bytes + 0x438, channels == 4 ? "M.K." : "8CHN", 4);
    for (sample = 1; sample <= 3; sample++) {
        uint8_t *header = fixture->bytes + 0x14u + (sample - 1u) * 0x1eu;
        write_be16(header + 22, 256);
        header[25] = (uint8_t)(24u + sample * 12u);
        write_be16(header + 26, 0);
        write_be16(header + 28, 256);
    }
    for (i = 0; i < 3u * 512u; i++)
        fixture->bytes[sample_offset + i] = (uint8_t)(random_u32() >> 24);
    pattern = fixture->bytes + 0x43c;
    for (row = 0; row < 32; row++) {
        for (channel = 0; channel < channels; channel++) {
            if ((random_u32() & 3u) == 0) {
                unsigned selected_sample = 1u + random_u32() % 3u;
                unsigned period = periods[random_u32() % (sizeof periods / sizeof periods[0])];
                unsigned effect = (unsigned[]){0, 0, 0x0c, 0x0a, 0x09}[random_u32() % 5u];
                unsigned parameter = effect == 0x0c ? random_u32() % 65u :
                                    effect == 0x0a ? 1u + random_u32() % 8u :
                                    effect == 0x09 ? random_u32() % 2u : 0u;
                uint8_t *event = pattern + (row * channels + channel) * 4u;
                set_event(event, selected_sample, period, effect, parameter);
            }
        }
    }
    set_event(pattern, 1, 428, 0, 0);
    set_event(pattern + (2u * channels) * 4u, 0, 0, 0x0f, 3);
    set_event(pattern + (3u * channels) * 4u, 0, 0, 0x0f, 150);
    set_event(pattern + (4u * channels) * 4u, 0, 0, 0x0a, 5);
    set_event(pattern + (5u * channels) * 4u, 0, 0, 0x0c, 40);
    set_event(pattern + (6u * channels) * 4u, 1, 404, 0x09, 1);
    if (position_jump)
        set_event(pattern + (8u * channels) * 4u, 0, 0, 0x0b, 0);
    if (pattern_break)
        set_event(pattern + (8u * channels) * 4u, 0, 0, 0x0d, 0);
}

static void clear_original_module_data(void)
{
#define CLEAR_SYMBOL(name, bytes) memset(oracle_sym(#name), 0, (bytes))
    CLEAR_SYMBOL(module_sound_io_base, 2);
    CLEAR_SYMBOL(module_sound_irq_number, 1);
    CLEAR_SYMBOL(module_sound_dma_channel, 1);
    CLEAR_SYMBOL(module_sample_rate, 2);
    CLEAR_SYMBOL(module_saved_irq_vector_offset, 4);
    CLEAR_SYMBOL(module_saved_irq_vector_segment, 2);
    CLEAR_SYMBOL(tracker_dos_memory_selector, 2);
    CLEAR_SYMBOL(module_volume_mix_table, 4);
    CLEAR_SYMBOL(module_audio_buffer, 4);
    CLEAR_SYMBOL(module_audio_buffer_bytes, 2);
    CLEAR_SYMBOL(module_audio_buffer_half_offset, 2);
    CLEAR_SYMBOL(module_audio_buffer_remaining_bytes, 2);
    CLEAR_SYMBOL(samples_until_next_tracker_tick, 2);
    CLEAR_SYMBOL(samples_per_tracker_tick, 2);
    CLEAR_SYMBOL(tracker_sample_period_scale, 4);
    CLEAR_SYMBOL(module_channel_count, 2);
    CLEAR_SYMBOL(module_channel_0_state, 8 * 22); /* Eight adjacent 16h-byte records. */
    CLEAR_SYMBOL(module_order_position, 1);
    CLEAR_SYMBOL(module_song_length, 1);
    CLEAR_SYMBOL(module_tick_counter, 1);
    CLEAR_SYMBOL(module_pattern_order_table, 128);
    CLEAR_SYMBOL(module_current_pattern_row, 4);
    CLEAR_SYMBOL(module_ticks_per_row, 1);
    CLEAR_SYMBOL(module_row_tick_countdown, 1);
    CLEAR_SYMBOL(module_tempo_bpm, 1);
    CLEAR_SYMBOL(module_channel_row_events, 48);
    CLEAR_SYMBOL(module_pattern_addresses, 128 * 4);
    CLEAR_SYMBOL(module_sample_addresses, 32 * 4);
    CLEAR_SYMBOL(module_sample_loop_starts, 32 * 4);
    CLEAR_SYMBOL(module_sample_loop_ends, 32 * 4);
    CLEAR_SYMBOL(module_sample_volumes, 32);
    CLEAR_SYMBOL(module_player_error_code, 4);
    CLEAR_SYMBOL(module_saved_es_segment, 4);
#undef CLEAR_SYMBOL
}

static void clear_translation_module_data(void)
{
    module_sound_io_base = 0;
    module_sound_irq_number = module_sound_dma_channel = 0;
    module_sample_rate = module_channel_count = 0;
    module_saved_irq_vector_offset = 0;
    module_saved_irq_vector_segment = tracker_dos_memory_selector = 0;
    module_volume_mix_table = module_audio_buffer = 0;
    module_audio_buffer_bytes = module_audio_buffer_half_offset = 0;
    module_audio_buffer_remaining_bytes = 0;
    samples_until_next_tracker_tick = samples_per_tracker_tick = 0;
    tracker_sample_period_scale = module_current_pattern_row = 0;
    module_order_position = module_song_length = module_tick_counter = 0;
    module_ticks_per_row = module_row_tick_countdown = module_tempo_bpm = 0;
    module_player_error_code = module_saved_es_segment = 0;
    memset(module_channel_0_state, 0, sizeof module_channel_0_state);
    memset(module_channel_1_state, 0, sizeof module_channel_1_state);
    memset(module_channel_2_state, 0, sizeof module_channel_2_state);
    memset(module_channel_3_state, 0, sizeof module_channel_3_state);
    memset(module_channel_4_state, 0, sizeof module_channel_4_state);
    memset(module_channel_5_state, 0, sizeof module_channel_5_state);
    memset(module_channel_6_state, 0, sizeof module_channel_6_state);
    memset(module_channel_7_state, 0, sizeof module_channel_7_state);
    memset(module_pattern_order_table, 0, sizeof module_pattern_order_table);
    memset(module_channel_row_events, 0, sizeof module_channel_row_events);
    memset(module_pattern_addresses, 0, sizeof module_pattern_addresses);
    memset(module_sample_addresses, 0, sizeof module_sample_addresses);
    memset(module_sample_loop_starts, 0, sizeof module_sample_loop_starts);
    memset(module_sample_loop_ends, 0, sizeof module_sample_loop_ends);
    memset(module_sample_volumes, 0, sizeof module_sample_volumes);
}

static void copy_original_symbol(void *destination, const char *name, size_t size)
{
    void *source = oracle_sym(name);
    if (!source) {
        fprintf(stderr, "    missing original symbol %s\n", name);
        memset(destination, 0, size);
        return;
    }
    memcpy(destination, source, size);
}

static void snapshot_original(TrackerSnapshot *s)
{
#define COPY(name, field) copy_original_symbol(&s->field, #name, sizeof s->field)
    memset(s, 0, sizeof *s);
    COPY(module_sound_io_base, io_base);
    COPY(module_sample_rate, sample_rate);
    COPY(module_channel_count, channel_count);
    COPY(module_saved_irq_vector_segment, saved_segment);
    COPY(tracker_dos_memory_selector, selector);
    COPY(module_audio_buffer_bytes, audio_bytes);
    COPY(module_audio_buffer_half_offset, audio_half);
    COPY(module_audio_buffer_remaining_bytes, audio_remaining);
    COPY(samples_until_next_tracker_tick, samples_until);
    COPY(samples_per_tracker_tick, samples_per_tick);
    COPY(module_sound_irq_number, irq);
    COPY(module_sound_dma_channel, dma);
    COPY(module_order_position, order_position);
    COPY(module_song_length, song_length);
    COPY(module_tick_counter, tick_counter);
    COPY(module_ticks_per_row, ticks_per_row);
    COPY(module_row_tick_countdown, row_tick_countdown);
    COPY(module_tempo_bpm, tempo);
    COPY(module_saved_irq_vector_offset, saved_offset);
    COPY(module_volume_mix_table, volume_table);
    COPY(module_audio_buffer, audio_buffer);
    COPY(tracker_sample_period_scale, sample_scale);
    COPY(module_current_pattern_row, current_row);
    COPY(module_player_error_code, player_error);
    COPY(module_saved_es_segment, saved_es);
    COPY(module_channel_0_state, channel_states);
    COPY(module_pattern_order_table, order_table);
    COPY(module_channel_row_events, row_events);
    COPY(module_pattern_addresses, pattern_addresses);
    COPY(module_sample_addresses, sample_addresses);
    COPY(module_sample_loop_starts, sample_loop_starts);
    COPY(module_sample_loop_ends, sample_loop_ends);
    COPY(module_sample_volumes, sample_volumes);
#undef COPY
    if (s->volume_table)
        memcpy(s->mix_table, (const void *)(uintptr_t)s->volume_table, sizeof s->mix_table);
    if (s->audio_buffer)
        memcpy(s->audio, (const void *)(uintptr_t)s->audio_buffer, sizeof s->audio);
}

static void snapshot_translation(TrackerSnapshot *s)
{
    memset(s, 0, sizeof *s);
    s->io_base = module_sound_io_base;
    s->sample_rate = module_sample_rate;
    s->channel_count = module_channel_count;
    s->saved_segment = module_saved_irq_vector_segment;
    s->selector = tracker_dos_memory_selector;
    s->audio_bytes = module_audio_buffer_bytes;
    s->audio_half = module_audio_buffer_half_offset;
    s->audio_remaining = module_audio_buffer_remaining_bytes;
    s->samples_until = samples_until_next_tracker_tick;
    s->samples_per_tick = samples_per_tracker_tick;
    s->irq = module_sound_irq_number;
    s->dma = module_sound_dma_channel;
    s->order_position = module_order_position;
    s->song_length = module_song_length;
    s->tick_counter = module_tick_counter;
    s->ticks_per_row = module_ticks_per_row;
    s->row_tick_countdown = module_row_tick_countdown;
    s->tempo = module_tempo_bpm;
    s->saved_offset = module_saved_irq_vector_offset;
    s->volume_table = module_volume_mix_table;
    s->audio_buffer = module_audio_buffer;
    s->sample_scale = tracker_sample_period_scale;
    s->current_row = module_current_pattern_row;
    s->player_error = module_player_error_code;
    s->saved_es = module_saved_es_segment;
    memcpy(s->channel_states[0], module_channel_0_state, 22);
    memcpy(s->channel_states[1], module_channel_1_state, 22);
    memcpy(s->channel_states[2], module_channel_2_state, 22);
    memcpy(s->channel_states[3], module_channel_3_state, 22);
    memcpy(s->channel_states[4], module_channel_4_state, 22);
    memcpy(s->channel_states[5], module_channel_5_state, 22);
    memcpy(s->channel_states[6], module_channel_6_state, 22);
    memcpy(s->channel_states[7], module_channel_7_state, 22);
    memcpy(s->order_table, module_pattern_order_table, sizeof s->order_table);
    memcpy(s->row_events, module_channel_row_events, sizeof s->row_events);
    memcpy(s->pattern_addresses, module_pattern_addresses, sizeof s->pattern_addresses);
    memcpy(s->sample_addresses, module_sample_addresses, sizeof s->sample_addresses);
    memcpy(s->sample_loop_starts, module_sample_loop_starts, sizeof s->sample_loop_starts);
    memcpy(s->sample_loop_ends, module_sample_loop_ends, sizeof s->sample_loop_ends);
    memcpy(s->sample_volumes, module_sample_volumes, sizeof s->sample_volumes);
    if (s->volume_table)
        memcpy(s->mix_table, (const void *)(uintptr_t)s->volume_table, sizeof s->mix_table);
    if (s->audio_buffer)
        memcpy(s->audio, (const void *)(uintptr_t)s->audio_buffer, sizeof s->audio);
}

static void save_trace(unsigned side)
{
    int count = oracle_trace_count();
    if (count > MAX_TRACE)
        count = MAX_TRACE;
    trace_counts[side] = count;
    memcpy(traces[side], oracle_trace(), (size_t)count * sizeof traces[side][0]);
}

static void capture_original_frame(TrackerFrame *frame)
{
    memset(frame, 0, sizeof *frame);
    copy_original_symbol(&frame->audio_half, "module_audio_buffer_half_offset",
                         sizeof frame->audio_half);
    copy_original_symbol(&frame->audio_remaining, "module_audio_buffer_remaining_bytes",
                         sizeof frame->audio_remaining);
    copy_original_symbol(&frame->samples_until, "samples_until_next_tracker_tick",
                         sizeof frame->samples_until);
    copy_original_symbol(&frame->samples_per_tick, "samples_per_tracker_tick",
                         sizeof frame->samples_per_tick);
    copy_original_symbol(&frame->tick_counter, "module_tick_counter", sizeof frame->tick_counter);
    copy_original_symbol(&frame->row_tick_countdown, "module_row_tick_countdown",
                         sizeof frame->row_tick_countdown);
    copy_original_symbol(&frame->tempo, "module_tempo_bpm", sizeof frame->tempo);
    copy_original_symbol(&frame->current_row, "module_current_pattern_row",
                         sizeof frame->current_row);
    copy_original_symbol(frame->channel_states, "module_channel_0_state",
                         sizeof frame->channel_states);
    copy_original_symbol(frame->row_events, "module_channel_row_events",
                         sizeof frame->row_events);
    copy_original_symbol(&frame->audio_address, "module_audio_buffer", sizeof frame->audio_address);
    memcpy(frame->audio, (const void *)(uintptr_t)frame->audio_address, sizeof frame->audio);
}

static void capture_translation_frame(TrackerFrame *frame)
{
    memset(frame, 0, sizeof *frame);
    frame->audio_half = module_audio_buffer_half_offset;
    frame->audio_remaining = module_audio_buffer_remaining_bytes;
    frame->audio_address = module_audio_buffer;
    frame->samples_until = samples_until_next_tracker_tick;
    frame->samples_per_tick = samples_per_tracker_tick;
    frame->tick_counter = module_tick_counter;
    frame->row_tick_countdown = module_row_tick_countdown;
    frame->tempo = module_tempo_bpm;
    frame->current_row = module_current_pattern_row;
    memcpy(frame->channel_states[0], module_channel_0_state, 22);
    memcpy(frame->channel_states[1], module_channel_1_state, 22);
    memcpy(frame->channel_states[2], module_channel_2_state, 22);
    memcpy(frame->channel_states[3], module_channel_3_state, 22);
    memcpy(frame->channel_states[4], module_channel_4_state, 22);
    memcpy(frame->channel_states[5], module_channel_5_state, 22);
    memcpy(frame->channel_states[6], module_channel_6_state, 22);
    memcpy(frame->channel_states[7], module_channel_7_state, 22);
    memcpy(frame->row_events, module_channel_row_events, sizeof frame->row_events);
    memcpy(frame->audio, (const void *)(uintptr_t)module_audio_buffer, sizeof frame->audio);
}

static int traces_equal(uint16_t native_ds, int expect_selector_artifact)
{
    int i;
    int saw_selector_artifact = 0;
    if (trace_counts[0] != trace_counts[1]) {
        printf("    trace length differs: original %d translated %d\n", trace_counts[0],
               trace_counts[1]);
        return 0;
    }
    for (i = 0; i < trace_counts[0]; i++) {
        const OracleEvent *a = &traces[0][i], *b = &traces[1][i];
        uint32_t av = a->value, bv = b->value;
        if (a->kind == 'N') {               /* upper EAX is caller scratch across INT n */
            av &= 0xffffu;
            bv &= 0xffffu;
        }
        if (a->kind == 'O' && a->size == 1 && a->port == 0x00a1u &&
            b->kind == 'O' && b->size == 1 && b->port == 0x00a1u &&
            av == ((native_ds >> 8) & 0xffu) &&
            bv == ((KE_FLAT_SELECTOR >> 8) & 0xffu)) {
            /* The native INT oracle cannot replace DS; the C side uses virtual DS=0170h. */
            if (saw_selector_artifact) {
                printf("    repeated PIC A1 selector-context difference\n");
                return 0;
            }
            saw_selector_artifact = 1;
            continue;
        }
        if (a->kind != b->kind || a->size != b->size || a->port != b->port || av != bv) {
            printf("    trace[%d] differs: orig %c/%d/%04X/%08X trans %c/%d/%04X/%08X\n",
                   i, a->kind, a->size, a->port, a->value,
                   b->kind, b->size, b->port, b->value);
            return 0;
        }
    }
    if (saw_selector_artifact != expect_selector_artifact) {
        printf("    PIC A1 selector-context difference presence was %d; expected %d\n",
               saw_selector_artifact, expect_selector_artifact);
        return 0;
    }
    if (!reported_native_selector_context) {
        printf("    oracle selector context: native DS %04X; virtual DS %04X (PIC A1 differs)\n",
               native_ds, KE_FLAT_SELECTOR);
        reported_native_selector_context = 1;
    }
    return 1;
}

static int snapshots_equal(const TrackerSnapshot *a, const TrackerSnapshot *b,
                           uint16_t native_es)
{
#define CHECK_VALUE(field) do { if (a->field != b->field) { \
        printf("    tracker state differs: %s (original %08X translated %08X)\n", #field, \
               (unsigned)a->field, (unsigned)b->field); return 0; } } while (0)
#define CHECK_BYTES(field) do { if (memcmp(a->field, b->field, sizeof a->field) != 0) { \
        size_t diff; const uint8_t *ap = (const uint8_t *)a->field; \
        const uint8_t *bp = (const uint8_t *)b->field; \
        for (diff = 0; diff < sizeof a->field && ap[diff] == bp[diff]; diff++) {} \
        printf("    tracker state differs: %s[%u] original %02X translated %02X\n", \
               #field, (unsigned)diff, ap[diff], bp[diff]); return 0; } } while (0)
    CHECK_VALUE(io_base); CHECK_VALUE(sample_rate); CHECK_VALUE(channel_count);
    if (a->saved_segment != native_es || b->saved_segment != 0xf000u) {
        printf("    unexpected oracle/virtual saved ES (native %04X, translated %04X, host %04X)\n",
               a->saved_segment, b->saved_segment, native_es);
        return 0;
    }
    CHECK_VALUE(selector); CHECK_VALUE(audio_bytes);
    CHECK_VALUE(audio_half); CHECK_VALUE(audio_remaining); CHECK_VALUE(samples_until);
    CHECK_VALUE(samples_per_tick); CHECK_VALUE(irq); CHECK_VALUE(dma);
    CHECK_VALUE(order_position); CHECK_VALUE(song_length); CHECK_VALUE(tick_counter);
    CHECK_VALUE(ticks_per_row); CHECK_VALUE(row_tick_countdown); CHECK_VALUE(tempo);
    CHECK_VALUE(saved_offset); CHECK_VALUE(volume_table); CHECK_VALUE(audio_buffer);
    CHECK_VALUE(sample_scale); CHECK_VALUE(current_row); CHECK_VALUE(player_error);
    if (a->saved_es != native_es || b->saved_es != KE_FLAT_SELECTOR) {
        printf("    unexpected oracle/virtual saved ES (native %04X, translated %04X, host %04X)\n",
               (unsigned)a->saved_es, (unsigned)b->saved_es, native_es);
        return 0;
    }
    CHECK_BYTES(channel_states); CHECK_BYTES(order_table);
    CHECK_BYTES(row_events); CHECK_BYTES(sample_volumes); CHECK_BYTES(pattern_addresses);
    CHECK_BYTES(sample_addresses); CHECK_BYTES(sample_loop_starts);
    CHECK_BYTES(sample_loop_ends); CHECK_BYTES(mix_table); CHECK_BYTES(audio);
#undef CHECK_VALUE
#undef CHECK_BYTES
    return 1;
}

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

static void prepare_side(unsigned side, uint8_t vector)
{
    if (side == 0)
        clear_original_module_data();
    else
        clear_translation_module_data();
    vpic_set_pm_vector(vector, 0, 0);
}

static int run_fixture(unsigned channels, uint32_t seed, int position_jump, int pattern_break)
{
    ModFixture fixture;
    TrackerSnapshot original, translated;
    TrackerFrame original_frames[72], frame;
    uint32_t args[5];
    uint16_t native_ds, native_es;
    uint8_t vector = (uint8_t)(0x08u + TEST_IRQ);
    unsigned i;
    int failures = 0;
    get_native_ds_es(&native_ds, &native_es);
    make_fixture(&fixture, channels, seed, position_jump, pattern_break);
    args[0] = (uint32_t)(uintptr_t)fixture.bytes;
    args[1] = 11025;
    args[2] = TEST_DSP_BASE;
    args[3] = TEST_IRQ;
    args[4] = TEST_DMA;

    prepare_side(0, vector);
    dsp_reset_ack = 0xaa;
    oracle_trace_reset();
    if (oracle_call(oracle_sym("load_protracker_module"), 5, args) != 0) {
        printf("    original rejected synthesized %u-channel MOD\n", channels);
        return 1;
    }
    for (i = 0; i < 72; i++) {
        call_original_irq(oracle_sym("protracker_irq_handler"));
        capture_original_frame(&original_frames[i]);
    }
    snapshot_original(&original);
    oracle_call(oracle_sym("stop_protracker_module"), 0, NULL);
    save_trace(0);

    prepare_side(1, vector);
    dsp_reset_ack = 0xaa;
    oracle_trace_reset();
    if (load_protracker_module((int)(uintptr_t)fixture.bytes, 11025,
                               TEST_DSP_BASE, TEST_IRQ, TEST_DMA) != 0) {
        printf("    translation rejected synthesized %u-channel MOD\n", channels);
        return 1;
    }
    for (i = 0; i < 72; i++) {
        protracker_irq_handler();
        capture_translation_frame(&frame);
        if (memcmp(&original_frames[i], &frame, sizeof frame) != 0) {
            size_t diff;
            const uint8_t *a = (const uint8_t *)&original_frames[i];
            const uint8_t *b = (const uint8_t *)&frame;
            const char *area = "runtime state";
            size_t offset = 0;
            for (diff = 0; diff < sizeof frame && a[diff] == b[diff]; diff++) {}
            if (diff >= offsetof(TrackerFrame, audio)) {
                area = "audio"; offset = diff - offsetof(TrackerFrame, audio);
            } else if (diff >= offsetof(TrackerFrame, row_events)) {
                area = "row events"; offset = diff - offsetof(TrackerFrame, row_events);
            } else if (diff >= offsetof(TrackerFrame, channel_states)) {
                area = "channel state"; offset = diff - offsetof(TrackerFrame, channel_states);
            }
            printf("    IRQ frame %u differs in %s[%u] (original %02X translated %02X)\n",
                   i, area, (unsigned)offset, a[diff], b[diff]);
            failures++;
            break;
        }
    }
    snapshot_translation(&translated);
    stop_protracker_module();
    save_trace(1);

    if (!traces_equal(native_ds, 1))
        failures++;
    if (!snapshots_equal(&original, &translated, native_es))
        failures++;
    if (translated.tempo != 150 || translated.ticks_per_row != 3) {
        printf("    fixture did not exercise speed/tempo changes (tempo=%u ticks=%u)\n",
               translated.tempo, translated.ticks_per_row);
        failures++;
    }
    if (translated.selector == 0 || translated.audio_bytes != MOD_DMA_BUFFER_BYTES ||
        translated.channel_count != channels) {
        printf("    DPMI allocation or channel setup state differs\n");
        failures++;
    }
    return failures;
}

static int test_bad_module_signature(void)
{
    ModFixture fixture;
    uint32_t args[5];
    uint32_t original_result, translated_result;
    uint8_t vector = (uint8_t)(0x08u + TEST_IRQ);
    int failures = 0;
    memset(&fixture, 0, sizeof fixture);
    fixture.size = MOD_IMAGE_BYTES;
    memcpy(fixture.bytes + 0x438, "NOPE", 4);
    args[0] = (uint32_t)(uintptr_t)fixture.bytes;
    args[1] = 11025; args[2] = TEST_DSP_BASE; args[3] = TEST_IRQ; args[4] = TEST_DMA;
    prepare_side(0, vector);
    oracle_trace_reset();
    original_result = oracle_call(oracle_sym("load_protracker_module"), 5, args);
    save_trace(0);
    prepare_side(1, vector);
    oracle_trace_reset();
    translated_result = (uint32_t)load_protracker_module((int)(uintptr_t)fixture.bytes,
                                                          11025, TEST_DSP_BASE,
                                                          TEST_IRQ, TEST_DMA);
    save_trace(1);
    if (original_result != translated_result || original_result != 0x602u ||
        module_player_error_code != 0x602u || module_channel_count != 8) {
        printf("    invalid MOD signature result/state differs\n");
        failures++;
    }
    if (!traces_equal(0, 0))
        failures++;
    return failures;
}

static int test_randomized_tracker(void)
{
    static const struct {
        unsigned channels;
        uint32_t seed;
        int jump;
        int pattern_break;
    } fixtures[] = {
        { 4, 0x11530a10u, 0, 0 },
        { 8, 0x8a108c8u, 0, 0 },
        { 4, 0x4b4d4b4du, 1, 0 },
        { 8, 0xa10f00du, 0, 1 }
    };
    unsigned i;
    int failures = 0;
    if (prepare_tracker_environment() != 0)
        return 1;
    for (i = 0; i < sizeof fixtures / sizeof fixtures[0]; i++)
        failures += run_fixture(fixtures[i].channels, fixtures[i].seed,
                                fixtures[i].jump, fixtures[i].pattern_break);
    clear_tracker_ports();
    return failures;
}

void register_m_11530_tests(void)
{
    oracle_register("m_11530 rejects an unrecognized MOD signature", test_bad_module_signature);
    oracle_register("m_11530 randomized 4/8-channel ProTracker mixing and DPMI IRQ", test_randomized_tracker);
}
