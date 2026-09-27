/* sb.c - Sound Blaster 2.0 / Pro DSP at 220h, IRQ 7, DMA 1 (on by default),
 * feeding an SDL3 audio stream.
 *
 * DSP: reset (226h) -> AAh; commands 10h direct DAC, 14h/1Ch single/auto-init 8-bit
 * output, 24h/2Ch 8-bit input, 40h time constant, 48h block size, 80h timed IRQ, D0h/D4h
 * pause/continue, D1h/D3h speaker, DAh exit auto-init, E1h version (2.01 = SB Pro), F2h
 * force IRQ. Mixer (224h/225h) registers are stored and read back (SB Pro detection).
 * Playback: the SDL audio thread pulls unsigned 8-bit mono at 1e6/(256-tc) Hz from the
 * programmed DMA channel; at the end of each DSP block it raises the SB IRQ, whose handler
 * (the game's) acknowledges by reading 22Eh and programs the next block. The consumer
 * (SDL device) is the clock, as the DAC was on the card.
 *
 * WAV dumps preserve the unsigned PCM bytes submitted to SDL; rate and block changes are
 * written to a sidecar because a WAV header can describe only one rate.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
static FILE *audio_dump;
static FILE *audio_dump_log;
static char audio_dump_path[MAX_PATH];
static char audio_dump_log_path[MAX_PATH * 2];
static uint64_t audio_sample_offset;
static uint32_t audio_dump_bytes;
static uint32_t audio_dump_rate = 7936;
static uint64_t dma_block_start_offset;
static unsigned dma_block_number;
static int audio_dump_failed;

static void mixer_reset_state(void)
{
    memset(mixer, 0, sizeof mixer);
    /* Unity voice/master gain preserves raw PCM bytes from the game's .DIG samples. */
    mixer[0x02] = 0xff;
    mixer[0x04] = 0xff;
    mixer[0x22] = 0xff;
}

static void dump_log(const char *format, ...)
{
    va_list args;
    if (!audio_dump_log)
        return;
    va_start(args, format);
    vfprintf(audio_dump_log, format, args);
    va_end(args);
    fputc('\n', audio_dump_log);
}

static void wav_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void wav_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void wav_header(FILE *file, uint32_t rate, uint32_t bytes)
{
    uint8_t h[44] = {0};
    memcpy(h, "RIFF", 4);
    wav_u32(h + 4, 36 + bytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    wav_u32(h + 16, 16);
    wav_u16(h + 20, 1);
    wav_u16(h + 22, 1);
    wav_u32(h + 24, rate);
    wav_u32(h + 28, rate);
    wav_u16(h + 32, 1);
    wav_u16(h + 34, 8);
    memcpy(h + 36, "data", 4);
    wav_u32(h + 40, bytes);
    fwrite(h, 1, sizeof h, file);
}

static void audio_dump_open(void)
{
    const char *path = getenv("KE_AUDIO_DUMP");
    int n;
    if (!path || !*path)
        return;
    n = snprintf(audio_dump_path, sizeof audio_dump_path, "%s", path);
    if (n < 0 || n >= (int)sizeof audio_dump_path) {
        ke_log(KE_LOG_WARN, "sb", "KE_AUDIO_DUMP path is too long; audio dump disabled");
        return;
    }
    audio_dump = fopen(audio_dump_path, "wb+");
    if (!audio_dump) {
        ke_log(KE_LOG_WARN, "sb", "cannot open audio dump %s", audio_dump_path);
        return;
    }
    setvbuf(audio_dump, NULL, _IOFBF, 65536);
    wav_header(audio_dump, audio_dump_rate, 0);
    n = snprintf(audio_dump_log_path, sizeof audio_dump_log_path, "%s.dsp.log", audio_dump_path);
    if (n >= 0 && n < (int)sizeof audio_dump_log_path) {
        audio_dump_log = fopen(audio_dump_log_path, "w");
        if (audio_dump_log)
            setvbuf(audio_dump_log, NULL, _IOLBF, 0);
    }
    if (!audio_dump_log)
        ke_log(KE_LOG_WARN, "sb", "cannot open DSP log %s", audio_dump_log_path);
    else
        dump_log("AUDIO_DUMP wav=%s wav_header_rate=%u format=PCM_U8_MONO rate_changes=logged",
                 audio_dump_path, audio_dump_rate);
}

static void audio_dump_close(void)
{
    if (audio_dump) {
        if (!audio_dump_failed) {
            fflush(audio_dump);
            fseek(audio_dump, 0, SEEK_SET);
            wav_header(audio_dump, audio_dump_rate, audio_dump_bytes);
            fflush(audio_dump);
        }
        fclose(audio_dump);
        audio_dump = NULL;
    }
    if (audio_dump_log) {
        dump_log("AUDIO_DUMP_END samples=%llu bytes=%u wav_header_rate=%u failed=%d",
                 (unsigned long long)audio_sample_offset, audio_dump_bytes,
                 audio_dump_rate, audio_dump_failed);
        fclose(audio_dump_log);
        audio_dump_log = NULL;
    }
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

static void dump_dma_prefix(void)
{
    uint32_t linear;
    const uint8_t *p;
    if (!audio_dump_log)
        return;
    linear = vdma_current_linear(SB_DMA);
    if (linear < LOWMEM_BASE || linear + 16 > LOWMEM_END) {
        dump_log("DMA_MEMORY address=%08X prefix=outside-low-memory", linear);
        return;
    }
    p = (const uint8_t *)(uintptr_t)linear;
    dump_log("DMA_MEMORY address=%08X prefix=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
             linear, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
             p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}

static int update_stream_rate(SDL_AudioStream *s)
{
    int rate = rate_from_tc(time_constant);
    if (rate != stream_rate) {
        SDL_AudioSpec src = {SDL_AUDIO_U8, 1, rate};
        if (SDL_SetAudioStreamFormat(s, &src, NULL)) {
            stream_rate = rate;
            dump_log("STREAM_FORMAT offset=%llu rate=%d tc=%u",
                     (unsigned long long)audio_sample_offset, rate, time_constant);
        } else {
            ke_log_once("sb.stream-format", KE_LOG_WARN, "sb",
                        "SDL audio stream format change to %d Hz failed: %s",
                        rate, SDL_GetError());
        }
    }
    return rate;
}

/* A single-cycle 640-byte DSP transfer can end in the middle of SDL's request. Let the
 * independent PIC thread run the guest IRQ handler and re-arm DMA before filling the rest
 * of this output chunk; otherwise the unfilled tail becomes a repeatable silence gap. */
static int wait_for_dma_rearm(void)
{
    uint64_t deadline = ke_now_ns() + 5000000ull;
    do {
        int ready;
        EnterCriticalSection(&sb_lock);
        ready = dma_active && block_left && !dma_paused;
        LeaveCriticalSection(&sb_lock);
        if (ready)
            return 1;
        SwitchToThread();
    } while (ke_now_ns() < deadline);
    return 0;
}

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
    case 0xe1: fifo_push(2); fifo_push(1); break;
    case 0xe0: fifo_push((uint8_t)~cmd_args[0]); break;
    case 0xf2: raise_irq(); break;
    default:
        ke_log_once("sb.cmd", KE_LOG_DEBUG, "sb", "DSP command %02Xh ignored", cmd);
        break;
    }

    dump_log("DSP offset=%llu cmd=%02X args=%02X%02X%02X%02X tc=%u rate=%u block=%u mode=%s mixer_voice=%02X mixer_master=%02X gain_q8=%u",
             (unsigned long long)audio_sample_offset, cmd,
             cmd_args[0], cmd_args[1], cmd_args[2], cmd_args[3], time_constant,
             (unsigned)rate_from_tc(time_constant), (unsigned)block_len,
             dma_auto ? "auto" : "single", mixer[0x04], mixer[0x22],
             (unsigned)mixer_voice_gain());
    if (cmd == 0x14 || cmd == 0x1c || cmd == 0x90 || cmd == 0x91) {
        if (!dma_block_number)
            audio_dump_rate = (uint32_t)rate_from_tc(time_constant);
        dump_dma_prefix();
        dma_block_start_offset = audio_sample_offset;
        ++dma_block_number;
        dump_log("DMA_BLOCK_START block=%u offset=%llu length=%u rate=%u tc=%u",
                 dma_block_number, (unsigned long long)dma_block_start_offset,
                 (unsigned)block_len, (unsigned)rate_from_tc(time_constant), time_constant);
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
    case 0x4:
        mixer_index = v;
        dump_log("MIXER_SELECT offset=%llu reg=%02X",
                 (unsigned long long)audio_sample_offset, mixer_index);
        break;
    case 0x5:
        if (mixer_index == 0) {
            mixer_reset_state();
        } else {
            mixer[mixer_index] = v;
        }
        dump_log("MIXER_WRITE offset=%llu reg=%02X value=%02X voice=%02X master=%02X gain_q8=%u",
                 (unsigned long long)audio_sample_offset, mixer_index, v,
                 mixer[0x04], mixer[0x22], (unsigned)mixer_voice_gain());
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
            memset(cmd_args, 0, sizeof cmd_args);
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
        int n = 0, rate, starved, rearm_timeout = 0, committed = 0;
        uint8_t fill = 0x80;
        uint32_t diagnostic_left;
        uint16_t gain;
        uint64_t chunk_base;
        memset(buf, 0x80, (size_t)want);
        EnterCriticalSection(&sb_lock);
        chunk_base = audio_sample_offset;
        rate = update_stream_rate(s);
        gain = mixer_voice_gain();
        while (n < want) {
            if (dma_active && !dma_paused) {
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
                    dump_log("DMA_BLOCK_END block=%u start=%llu end=%llu length=%u rate=%u tc=%u",
                             dma_block_number, (unsigned long long)dma_block_start_offset,
                             (unsigned long long)(chunk_base + (uint32_t)n),
                             (unsigned)block_len, (unsigned)rate, time_constant);
                    raise_irq();
                    if (dma_auto) {
                        block_left = block_len;
                    } else {
                        dma_active = 0;
                        dma_input = high_speed = 0;
                        audio_sample_offset += (uint32_t)(n - committed);
                        committed = n;
                        LeaveCriticalSection(&sb_lock);
                        if (n < want && !wait_for_dma_rearm())
                            rearm_timeout = 1;
                        EnterCriticalSection(&sb_lock);
                        if (rearm_timeout)
                            break;
                        if (n < want) {
                            rate = update_stream_rate(s);
                            gain = mixer_voice_gain();
                        }
                    }
                }
            } else {
                break;
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
        audio_sample_offset += (uint32_t)(want - committed);
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
        if (rearm_timeout) {
            dump_log("DMA_REARM_TIMEOUT offset=%llu block=%u left=%u",
                     (unsigned long long)(chunk_base + (uint32_t)n),
                     dma_block_number, diagnostic_left);
            ke_log_once("sb.dma.rearm-timeout", KE_LOG_WARN, "sb",
                        "IRQ7 did not re-arm single-cycle DMA within 5 ms; inserting silence");
        }
        if (audio_dump && !audio_dump_failed) {
            size_t written = fwrite(buf, 1, (size_t)want, audio_dump);
            if (written != (size_t)want) {
                audio_dump_failed = 1;
                ke_log_once("sb.dump-write", KE_LOG_WARN, "sb", "audio dump write failed: %s",
                            audio_dump_path);
            } else {
                audio_dump_bytes += (uint32_t)written;
            }
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
    audio_dump_open();
    vhw_register_ports(SB_BASE, SB_BASE + 0xf, sb_in, sb_out, NULL, "sound-blaster");
    stream_rate = rate_from_tc(time_constant);
    spec.format = SDL_AUDIO_U8;
    spec.channels = 1;
    spec.freq = stream_rate;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, sb_audio_callback, NULL);
    if (!stream) {
        ke_log(KE_LOG_ERROR, "sb", "SDL_OpenAudioDeviceStream failed at %d Hz U8 mono: %s",
               spec.freq, SDL_GetError());
        return;
    }
    SDL_SetAudioStreamGain(stream, (float)ke_config_volume() / 100.0f);
    {
        SDL_AudioSpec src, dst;
        if (SDL_GetAudioStreamFormat(stream, &src, &dst))
            ke_log(KE_LOG_INFO, "sb", "SDL audio stream opened: source %d Hz U8 mono, device %d Hz/%d channels",
                   src.freq, dst.freq, dst.channels);
    }
    SDL_ResumeAudioStreamDevice(stream);
    ke_log(KE_LOG_INFO, "sb", "Sound Blaster Pro (DSP 2.01) at 220h IRQ %d DMA %d", SB_IRQ, SB_DMA);
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
    audio_dump_close();
}
