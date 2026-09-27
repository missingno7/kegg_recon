# Facts from the earlier `kegg_forged` port attempt

Source: `D:\Games\DOS\dos_recosystem\kegg_forged` (R), its `port_forge` engine (PF). A binary-translation/replay port
with an emulated DOS machine; its captured device states come from the real game's register writes. Treat as strong
external evidence, verify against our own oracles before relying on it. Paths below are relative to R.

## Video (from snapshot `pm_state.json` -> devices.vga, ~100 snapshots under `artifacts/**`)
| Screen | Mode | CRTC 00..18 | Misc | Notes |
|---|---|---|---|---|
| title/intro | 320x400 unchained | `5F 4F 50 82 54 80 BF 1F 00 40 00 00 00 00 00 31 9C 0E 8F 28 00 96 B9 E3 FF` | 63 | CR09=0x40: NO line repeat (bit 6 = line-compare bit 9); CR13=0x28 -> 80 bytes/plane row; CR14=00, CR17=E3 byte mode; VDE=0x18F (400 lines); VT 449 lines = 70.086 Hz; display_start 0; no split |
| gameplay | 320x200 unchained (NOT chained 13h) | same, CR09=0x41 | 63 | double scan; page flip display_start 0 <-> 0x4000 |
| menu | 320x240 unchained | `5F 4F 50 82 54 80 0B 3E 00 41 00 00 (4B/00) 00 00 31 EA 0C DF 28 00 E7 04 E3 FF` | E3 | VT 525 = 59.94 Hz; VDE 479 / 2 = 240; flip 0 <-> 0x4B00 |

Scan-out rule that produced clean frames (PF `src/devices/vga_seq.hpp:83-105`): width = (CR01+1)*4;
height = (VDE+1) / ((CR09 & 0x1F) + 1) (only low 5 bits; bit 6 is NOT double scan); stride = CR13*2 (byte mode);
pixel = plane[x&3][start + y*stride + (x>>2)]. Planar model: write modes 0/1 only (mode 1 = latch copy), bit mask FF,
map mask SEQ02, read map GC04. DAC 6->8 bit: (v<<2)|(v>>4); 3C8/3C9 auto-increment. Raster: 3DA bit3 = vretrace,
bit0 = display disabled, from CRTC timing (25.175 MHz, 800 dots x 449 lines, retrace lines 412-413). Toggling SEQ01
bit 5 (blank) must not reset raster timing (PF `docs/47-dos-machine-timeline-and-vga-raster.md:212-231`).
Reference images: `pf_pm_title.png` (title 640x480 4:3, no stripes), `pf_pm_screen.ppm` (320x400 raw),
`pf_pm_screen.png` (gameplay). Every snapshot: `pm_planes.bin` (4 planes x 64 KiB, plane-major) + `dac_hex` +
CRTC/SEQ/GC state -> a deterministic scan-out test for port/vhw/vga.c.

## Sound
Confirmed by the user (who knows the game): Krypton Egg has NO MOD music, only PCM - the ProTracker player in
asm/m_11530 is linked-in library code that the game does not use (worker A10 checks the call sites). The port's
audio fidelity rests entirely on the PCM path below.
- SB 2.0 class, 0x220 / IRQ7 / DMA1, DSP 2.01 (`game.json:31-46`). All sound incl. music = one 8-bit unsigned mono
  PCM stream; a new sound replaces the current one (PF `docs/16-kegg-native-audio.md:18-44`).
- DSP: reset base+6 1/0 wait 0xAA; D1/D3 speaker on/off, D0 halt; rate via 0x40, TC = 256 - 3906/((rate+127)>>8)
  -> 5000->5128, 5500->5376, 8000->7936, 15000->15151 Hz. Playback DSP 0x14 single-cycle, length 0x27F (640 bytes),
  re-issued every IRQ7; ISR acks base+0xE, may reprogram TC, EOI 0x20 (+0xA0 for IRQ>=8), refills. 8237 mode 0x48
  (single) or 0x58 (auto-init); logs show a 1280-byte ring = two 640-byte halves (`artifacts/sb_exact.log`).
- Game logic reads the sound-active flag (completion timing affects gameplay).
- Sample table: {ptr, len, rate} x 16 bytes. Music: title 85296 B @8000, menu 65832 @8000 (docs disagree: 5524 vs
  5376 Hz), level jingle 11004 @5500 — sizes = KE_TIT/KE_MENU/KE_LVL.DIG minus 8 (8-byte header).
- References: `artifacts/audio_dump/idNNN_rRATE_ADDR.raw` (16 effect sources, byte-exact), `artifacts/audio_events.txt`
  (frame, id, len, rate), `audio_trace.txt`, `sb_exact.log`.

## Timing
- vsync wait: poll 3DA bit3 clear then set. Calibration (IRQs/NMI masked): 2 retraces; PIT ch0 mode 2 count 0;
  delay loop; at retrace latch; period = 0x10000 - count; accepted if 10000..25000 and 4 samples within 512 ticks,
  else the vsync timer is disabled. PIT reload = period - 0x100. IRQ0 ISR: load 0xFFFF, busy-wait retrace, reload
  period-0x100, ++tick, run up to 5 timer records {callback, period, accumulator}, EOI -> a vsync-locked timer.
  Measured: 17024 ticks (70 Hz) -> reload ~16768; 19904 (60 Hz menu) -> 19648. PIT and 3DA must share one clock.

## Input / other
- INT 33h functions 0/3/4/7/8/0B/24 (fn 8 locks the paddle row); range default 0-639 x 0-199. Keyboard IRQ1 set-1.
  P = pause, Esc schedules a restart after 33 ticks. DPMI 0100 (DOS memory for DMA buffer), 0204/0205, INT 1Ah.
- `.GIF` assets have no GIF header (custom format). KE_LDCWC.TAB = 60 fixed-size level records, 18x16 board.

## Reusable oracles
- Replays `artifacts/replays-v2/*.pfreplay.json`: input channels dos.keyboard.scancodes (pulse) and
  dos.mouse.normalized {u,v,buttons} (hold) per frame-entry occurrence; checkpoints are PF-internal hashes (not
  reusable). Cold-start gameplay rec_20260823_145424 (16806 frames); password/level-21 rec_20260824_114945; simple
  script `artifacts/replay-inputs/pmrec_20260723_200732.input.json` (588 frames). Semantic checkpoints `recovery/*.json`.
- Snapshots (`pm_mem.bin`, `pm_planes.bin`, `pm_state.json`: CPU, VGA, DAC, PIT, PIC, SB) = per-state register and
  VRAM oracles.
