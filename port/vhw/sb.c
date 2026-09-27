/* sb.c - Sound Blaster 2.0 / Pro DSP at 220h, IRQ 7, DMA 1 (attached with KE_SB=1),
 * feeding an SDL3 audio stream.
 *
 * DSP: reset (226h) -> AAh; commands 10h direct DAC, 14h/1Ch single/auto-init 8-bit
 * output, 24h/2Ch 8-bit input, 40h time constant, 48h block size, 80h timed IRQ, D0h/D4h
 * pause/continue, D1h/D3h speaker, DAh exit auto-init, E1h version (3.02 = SB Pro), F2h
 * force IRQ. Mixer (224h/225h) registers are stored and read back (SB Pro detection).
 * Playback: the SDL audio thread pulls unsigned 8-bit mono at 1e6/(256-tc) Hz from the
 * programmed DMA channel; at the end of each DSP block it raises the SB IRQ, whose handler
 * (the game's) acknowledges by reading 22Eh and programs the next block. The consumer
 * (SDL device) is the clock, as the DAC was on the card.
 *
 * WORK PACKAGE "audio": stereo/high-speed modes if used, underrun policy, latency, verify
 * with the m_11258/m_11494 translations and the ProTracker player (m_11530).
 */
#include <string.h>
#include <windows.h>
#include <SDL3/SDL.h>
#include "vhw.h"
#include "../include/ke_port.h"

#define SB_BASE 0x220
#define SB_IRQ 7
#define SB_DMA 1

static CRITICAL_SECTION sb_lock;
static uint8_t out_fifo[16];
static int out_count;
static uint8_t cmd, cmd_args[4];
static int cmd_need, cmd_have;
static uint8_t reset_latch;
static uint8_t time_constant = 0xa6;       /* ~11 kHz */
static uint8_t direct_sample = 0x80;
static uint32_t block_len = 0x800, block_left;
static uint32_t timed_irq_samples;
static int dma_active, dma_auto, dma_input, dma_paused, high_speed, speaker, direct_active;
static int lock_initialized;
static int underrun_reported;
static uint8_t irq_pending;
static uint8_t mixer_index, mixer[256];
static SDL_AudioStream *stream;
static int stream_rate;

static void mixer_reset_state(void)
{
    memset(mixer, 0, sizeof mixer);
    mixer[0x02] = 0x44;                      /* CT1345 legacy master volume: level 4 */
    mixer[0x04] = 0xcc;                      /* CT1345 voice L/R: default level 12 */
    mixer[0x22] = 0xcc;                      /* CT1345 master L/R: default level 12 */
}

static uint16_t mixer_voice_gain(void)
{
    /* CT1345 volume levels are 4 dB steps: level 15 is 0 dB, level 0 is -60 dB. */
    static const uint16_t gain_q8[16] = {
        0, 0, 1, 1, 2, 3, 4, 6, 10, 16, 26, 41, 64, 102, 162, 256
    };
    uint8_t voice = mixer[0x04], master = mixer[0x22];
    uint16_t voice_gain = (uint16_t)((gain_q8[voice >> 4] + gain_q8[voice & 0x0f]) / 2);
    uint16_t master_gain = (uint16_t)((gain_q8[master >> 4] + gain_q8[master & 0x0f]) / 2);
    return (uint16_t)((voice_gain * master_gain + 128) >> 8);
}

static void apply_mixer_gain(uint8_t *samples, int count, uint16_t gain)
{
    int i;
    for (i = 0; i < count; i++) {
        int centered = (int)samples[i] - 128;
        int scaled = (centered * gain) / 256;
        if (scaled < -128) scaled = -128;
        if (scaled > 127) scaled = 127;
        samples[i] = (uint8_t)(scaled + 128);
    }
}

static void fifo_push(uint8_t v)
{
    if (out_count < (int)sizeof out_fifo)
        out_fifo[out_count++] = v;
}

static void raise_irq(void)
{
    irq_pending = 1;
    vpic_raise_irq(SB_IRQ);
}

static int rate_from_tc(uint8_t tc) { return 1000000 / (256 - tc); }

static void dsp_command_complete(void)
{
    switch (cmd) {
    case 0x10:
        direct_sample = cmd_args[0];
        direct_active = 1;
        break;                                          /* direct DAC sample */
    case 0x14:
        block_len = (uint32_t)(cmd_args[0] | (cmd_args[1] << 8)) + 1;
        block_left = block_len;
        dma_active = 1;
        dma_auto = dma_input = dma_paused = high_speed = 0;
        break;
    case 0x1c:
    case 0x90:
        block_left = block_len;
        dma_active = 1;
        dma_auto = 1;
        dma_input = dma_paused = 0;
        high_speed = (cmd == 0x90);
        break;
    case 0x91:                                      /* high-speed single-cycle output */
        block_left = block_len;
        dma_active = 1;
        dma_auto = dma_input = dma_paused = 0;
        high_speed = 1;
        break;
    case 0x24: {                                    /* single-cycle input; startup DMA probe */
        int terminal = 0;
        uint32_t input_len = (uint32_t)(cmd_args[0] | (cmd_args[1] << 8)) + 1;
        int moved = input_len > 0x10000u ? 0x10000 : (int)input_len;
        (void)vdma_write(SB_DMA, 0x80, moved, &terminal);
        dma_active = dma_input = 0;
        if (terminal)
            raise_irq();
        break;
    }
    case 0x2c: case 0x98: case 0x99:               /* auto/single-cycle digitized input */
        block_left = block_len;
        dma_active = dma_input = 1;
        dma_auto = (cmd != 0x99);
        dma_paused = 0;
        high_speed = (cmd == 0x98 || cmd == 0x99);
        break;
    case 0x40: time_constant = cmd_args[0]; break;
    case 0x48: block_len = (uint32_t)(cmd_args[0] | (cmd_args[1] << 8)) + 1; break;
    case 0x80:
        timed_irq_samples = (uint32_t)(cmd_args[0] | (cmd_args[1] << 8)) + 1;
        break;
    case 0xd0: dma_paused = 1; break;
    case 0xd4: dma_paused = 0; break;
    case 0xd1: speaker = 1; break;
    case 0xd3: speaker = 0; break;
    case 0xda: dma_auto = 0; break;
    case 0xe1: fifo_push(3); fifo_push(2); break;
    case 0xe0: fifo_push((uint8_t)~cmd_args[0]); break;
    case 0xf2: raise_irq(); break;
    default:
        ke_log_once("sb.cmd", KE_LOG_DEBUG, "sb", "DSP command %02Xh ignored", cmd);
        break;
    }
}

static int args_for(uint8_t c)
{
    switch (c) {
    case 0x10: case 0x40: case 0xe0: return 1;
    case 0x14: case 0x24: case 0x48: case 0x80: return 2;
    default: return 0;
    }
}

static uint32_t sb_in(void *ctx, uint16_t port, int size)
{
    uint32_t v = 0xff;
    (void)ctx; (void)size;
    EnterCriticalSection(&sb_lock);
    switch (port - SB_BASE) {
    case 0x5: v = mixer[mixer_index]; break;
    case 0xa:
        if (out_count) {
            v = out_fifo[0];
            memmove(out_fifo, out_fifo + 1, (size_t)--out_count);
        }
        break;
    case 0xc: v = 0x7f; break;                          /* write buffer ready */
    case 0xe:
        v = out_count ? 0xff : 0x7f;
        if (irq_pending) {                              /* acknowledge 8-bit IRQ */
            irq_pending = 0;
            vpic_lower_irq(SB_IRQ);
        }
        break;
    case 0xf:                                          /* 16-bit IRQ acknowledge on SB Pro */
        v = out_count ? 0xff : 0x7f;
        if (irq_pending) {
            irq_pending = 0;
            vpic_lower_irq(SB_IRQ);
        }
        break;
    default: break;
    }
    LeaveCriticalSection(&sb_lock);
    return v;
}

static void sb_out(void *ctx, uint16_t port, uint32_t value, int size)
{
    uint8_t v = (uint8_t)value;
    (void)ctx; (void)size;
    EnterCriticalSection(&sb_lock);
    switch (port - SB_BASE) {
    case 0x4: mixer_index = v; break;
    case 0x5:
        if (mixer_index == 0)
            mixer_reset_state();
        else
            mixer[mixer_index] = v;
        break;
    case 0x6:
        if (v & 1)
            reset_latch = 1;
        else if (reset_latch) {
            reset_latch = 0;
            out_count = 0;
            cmd_need = cmd_have = 0;
            dma_active = 0;
            dma_auto = dma_input = dma_paused = high_speed = 0;
            timed_irq_samples = 0;
            time_constant = 0xa6;
            block_len = 0x800;
            speaker = 0;
            direct_active = 0;
            direct_sample = 0x80;
            if (irq_pending) {
                irq_pending = 0;
                vpic_lower_irq(SB_IRQ);
            }
            fifo_push(0xaa);
        }
        break;
    case 0xc:
        if (high_speed) {
            /* Creative's high-speed modes accept no commands before DSP reset. */
        } else if (cmd_need > cmd_have) {
            cmd_args[cmd_have++] = v;
            if (cmd_have == cmd_need)
                dsp_command_complete();
        } else {
            cmd = v;
            cmd_need = args_for(v);
            cmd_have = 0;
            if (!cmd_need)
                dsp_command_complete();
        }
        break;
    default: break;
    }
    LeaveCriticalSection(&sb_lock);
}

/* SDL audio thread: produce `additional` bytes of U8 mono at the current DSP rate. */
static void SDLCALL sb_audio_callback(void *userdata, SDL_AudioStream *s, int additional, int total)
{
    uint8_t buf[4096];
    (void)userdata; (void)total;
    while (additional > 0) {
        int want = additional < (int)sizeof buf ? additional : (int)sizeof buf;
        int n = 0, rate, starved;
        uint8_t fill = 0x80;
        uint32_t diagnostic_left;
        uint16_t gain;
        memset(buf, 0x80, (size_t)want);
        EnterCriticalSection(&sb_lock);
        rate = rate_from_tc(time_constant);
        if (rate != stream_rate) {
            SDL_AudioSpec src = {SDL_AUDIO_U8, 1, rate};
            SDL_SetAudioStreamFormat(s, &src, NULL);
            stream_rate = rate;
        }
        gain = mixer_voice_gain();
        while (n < want && dma_active && !dma_paused) {
            int tc = 0, chunk = want - n, got;
            if ((uint32_t)chunk > block_left)
                chunk = (int)block_left;
            got = dma_input ? vdma_write(SB_DMA, 0x80, chunk, &tc)
                            : vdma_read(SB_DMA, buf + n, chunk, &tc);
            if (got > 0 && got < chunk)
                ke_log_once("sb.dma.short-read", KE_LOG_WARN, "sb",
                            "DMA delivered %d of %d requested bytes (TC=%d, DSP block left=%u)",
                            got, chunk, tc, block_left);
            if (got <= 0)
                break;
            if (!dma_input && !speaker)
                memset(buf + n, 0x80, (size_t)got);
            n += got;
            block_left -= (uint32_t)got;
            if (block_left == 0) {
                raise_irq();
                if (dma_auto)
                    block_left = block_len;
                else {
                    dma_active = 0;
                    dma_input = high_speed = 0;
                }
            }
        }
        if (timed_irq_samples) {
            if ((uint32_t)want >= timed_irq_samples) {
                timed_irq_samples = 0;
                raise_irq();
            } else {
                timed_irq_samples -= (uint32_t)want;
            }
        }
        if (direct_active && !dma_active && speaker)
            fill = direct_sample;
        starved = !dma_input && n < want && dma_active && !dma_paused;
        diagnostic_left = block_left;
        LeaveCriticalSection(&sb_lock);
        if (n < want) {
            memset(buf + n, fill, (size_t)(want - n));
        }
        apply_mixer_gain(buf, want, gain);
        if (starved && !underrun_reported) {
            underrun_reported = 1;
            ke_log_once("sb.underrun", KE_LOG_WARN, "sb",
                        "audio DMA starved (DSP block left=%u); outputting unsigned silence until the next block",
                        diagnostic_left);
        }
        SDL_PutAudioStreamData(s, buf, want);
        additional -= want;
    }
}

void vsb_init(void)
{
    SDL_AudioSpec spec;
    InitializeCriticalSection(&sb_lock);
    lock_initialized = 1;
    if (!ke_config.sound_blaster) {
        ke_log(KE_LOG_INFO, "sb", "no Sound Blaster attached (KE_SB=1 attaches one at 220h/IRQ7/DMA1)");
        return;
    }
    mixer_reset_state();
    vhw_register_ports(SB_BASE, SB_BASE + 0xf, sb_in, sb_out, NULL, "sound-blaster");
    stream_rate = rate_from_tc(time_constant);
    spec.format = SDL_AUDIO_U8;
    spec.channels = 1;
    spec.freq = stream_rate;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, sb_audio_callback, NULL);
    if (!stream) {
        ke_log(KE_LOG_WARN, "sb", "SDL audio unavailable: %s (card stays silent)", SDL_GetError());
        return;
    }
    SDL_SetAudioStreamGain(stream, (float)ke_config_volume() / 100.0f);
    SDL_ResumeAudioStreamDevice(stream);
    ke_log(KE_LOG_INFO, "sb", "Sound Blaster Pro (DSP 3.02) at 220h IRQ %d DMA %d", SB_IRQ, SB_DMA);
}

void vsb_shutdown(void)
{
    if (stream) {
        SDL_DestroyAudioStream(stream);
        stream = NULL;
    }
    if (lock_initialized) {
        DeleteCriticalSection(&sb_lock);
        lock_initialized = 0;
    }
}
