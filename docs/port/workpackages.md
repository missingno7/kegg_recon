# SDL3 port work packages (after P0)

Read docs/port/architecture.md first. Packages are independent unless a dependency is named;
each owns the files listed and must not edit files owned by another package (describe needed
changes in the hand-off instead, AGENTS.md rules apply). `src/` edits are allowed only as
minimal `PORT:`-marked lines and must be listed in the hand-off.

## Common rules

**Build / gates every package runs before hand-off**

```
cmake --build build/port                                  # no new warnings in port/ code
build/port/oracle/ke_oracle.exe build/port/oracle          # all tests pass
python port/tools/check_layouts.py --data                  # 0 mismatches
python port/tools/gen_asm_stubs.py --check                 # stubs regenerated
python port/tools/smoke.py                                 # SMOKE OK
```

**Translating an assembly module** (packages A*): create `port/asm/<module>.c` defining every
public code label *and* every public/internal data label of `asm/<module>.asm` (the generator
drops the module's stubs and data as soon as the file exists), then run
`python port/tools/gen_asm_stubs.py`. Style: literal, instruction-order preserving C
(the pilot `port/asm/m_137a8_13944.c` is the template): keep EQU names, keep register-level
quirks and mark them `QUIRK`, use signed/unsigned compares exactly as the jumps do, reach
video memory only through `vga_mem_read8/vga_mem_write8`, ports through `inp/outp/outpw`,
CLI/STI through `vcpu_cli/vcpu_sti` inside `vhw_enter/leave` (or `_disable/_enable`).
Entry points called from C keep the C prototypes the units declare (cdecl, -3s); internal
register-convention entries (jump tables between modules) become C functions whose interface
is documented at the top of the file. Tests: `port/oracle/test_<module>.c` with a
`register_<module>_tests()` added to `oracle_main.c`; compare against `oracle_sym(...)` of the
original with randomized inputs, port traces (`oracle_trace`) and virtual-PC state; include at
least one mutation check in the hand-off (break one constant, show the test fails).

## Packages

### A1 - asset checksum decoder (`asm/m_0982c_0995c.asm`) - priority 1
- Owns: `port/asm/m_0982c_0995c.c`, `port/oracle/test_m_0982c.c`.
- Publics: `decode_and_verify_asset` (installed as `decode_hook` by main), data
  `g_asset_checksum`, `g_asset_trailer_size`, `g_asset_encoding`.
- Accept: oracle on every `assets/KE_*` file loaded into RAM (identical output buffer,
  return value, globals) + random corruptions; smoke run no longer reports the stub.

### A2 - IFF/ILBM decoder (`m_0a284_0a51f`)
- Owns: `port/asm/m_0a284_0a51f.c`, `port/oracle/test_m_0a284.c`.
- Publics: `decode_iff_ilbm_image`, `find_iff_chunk` + `g_iff_*` data; externs
  `iff_width_pixels`, `iff_height_pixels`, `iff_output_byte_count`, `iff_decoded_pixel_count`.
- Accept: oracle on synthesized ILBM files (ByteRun1 / raw, odd widths, missing chunks ->
  same error codes) and on any IFF found in the assets.

### A3 - GIF LZW decoder (`m_11df8_12288`) - priority 1
- Owns: `port/asm/m_11df8_12288.c`, `port/oracle/test_m_11df8.c`.
- Publics: `decode_gif_image`, `decode_gif_image_entry` + data (`gif_decoded_image_state`...).
- Accept: oracle byte equality of pixels/palette/state on `KE_MENU.GIF`, `KE_MONST.GIF`,
  `KE_ORDER.GIF`, `KE_SCORE.GIF` and fuzzed truncations (same error paths).

### A4 - no-op hooks (`m_0a958_0a966`, `m_12680_12683`, `m_12970_12973`)
- Trivial; may be folded into A6. Accept: symbols defined, stubs gone.

### A5 - VGA register helpers and DAC (`m_13944_13a48`, `m_13a48_13a95`) - priority 1
- Owns: `port/asm/m_13944_13a48.c`, `port/asm/m_13a48_13a95.c`, their tests.
- Publics: `update_attr/crtc/seq/gc_register`, `set_seq_plane_mask`, `rotate_seq_plane_mask`,
  `set_gc_read_map`, `set_gc_mode` (+ `_entry` aliases), `write_dac_palette`, `copy_ds_to_es`,
  data `saved_ds`; they update the byte caches in `vga_state` (offsets 60h-62h).
- Accept: oracle I/O trace (port, value, order) and `vga_state` cache bytes identical for random
  arguments; `write_dac_palette` DAC contents identical in the virtual VGA. With A1+A3 the
  smoke screenshot shows the menu palette.

### A6 - sprite/BOB renderer cluster (`m_12a9c_12f30` + `m_12288`, `m_12684`, `m_12974`, A4 hooks)
- Owns: `port/asm/m_12a9c_12f30.c`, `m_12288_12680.c`, `m_12684_1296e.c`, `m_12974_12a9c.c`,
  (`m_12680_12683.c`, `m_12970_12973.c` if not done in A4), tests.
- Internal interface: the record-kind dispatch table of m_12a9c calls the kind-3/5/transparent
  renderers with register arguments - define one C signature for all of them in a shared
  header `port/asm/sprite_records.h` (owned here).
- Needs O1 (VGA memory hook) for direct oracle comparison; until then compare against the
  original with the page bases pointed at RAM where the code allows it.
- Accept: identical plane memory (all 4 planes) and I/O traces for random records, clipping at
  every edge, transparent/opaque, update-list overflow (error 407h).

### A7 - pixels, spans, fills (`m_13324_13712`, `m_13712_137a8`, `m_12f30_12f9c`, `m_12f9c_13324`)
- Owns those four `port/asm/*.c` + tests.
- Publics: `read/write_vga_pixel`, `fill_vga_span`, `fill_clipped_vga_rectangle`,
  `fill_planar_video_rows`, `copy_chunky_scanline_to_vga`, `copy_screen_span`,
  `copy_clipped_screen_rectangle` (+ `_entry`).
- Accept: as A6 (plane memory + traces), including unchained/chained layouts.

### A8 - PIT timing (`m_09f64_0a0d2`) - with T1
- Owns: `port/asm/m_09f64_0a0d2.c`, test.
- Publics: `measure_pit_channel0`, `set_pit_channel0_reload`, `pit_channel0_interrupt`, data
  `pit_sample_auxiliary`, `g_pit_elapsed_ticks`; calls `wait_for_vsync`, `process_timer_events`.
- Accept: `verify_timer()` passes on the virtual PC (samples within 2710h..61A8h and stable),
  the game installs its timer (log: "VBL manager installed"), `wait_for_tick` runs on the IRQ0
  path at the retrace rate (60/70 Hz measured over 10 s, +-0.5 %), traces match the original.

### A9 - Sound Blaster DSP + DMA (`m_11258_11494`, `m_11494_11530`) - with S1
- Owns: both `port/asm/*.c` + tests.
- Publics: see `gen_asm_stubs.py --list` (22 SB entries, 3 DMA entries, SB/DMA data).
- Accept: I/O traces identical to the original for reset, rate set, DMA start/stop, IRQ ack;
  with `KE_SB=1` detection reports "SOUNDBLASTER PRO detected (220h, 7, 1)".

### A10 - ProTracker player (`m_11530_11df8`) - after A9
- Owns: `port/asm/m_11530_11df8.c` + test. Largest module (0x8C8 bytes, 45 publics, INT 31h).
- Accept: oracle mixing of the game's modules for N ticks produces identical sample buffers
  and channel state; tempo/BPM changes; DPMI calls emulated identically.

### V1 - virtual VGA completion (`port/vhw/vga.c`)
- Owns: `port/vhw/vga.c`, `port/oracle/test_vga.c`.
- Work: start address latched at vertical retrace (page flipping without tearing), horizontal
  panning / line compare / max scan line, verify every mode of `video_mode_table`
  (320x200..360x480, chained/unchained), optional text mode rendering for the startup report,
  performance (fast paths for write mode 0 with all planes).
- Accept: unit tests per write/read mode against a reference table; scan-out of each game
  mode; with A5+A6+A7 the menu renders identically to a DOSBox-X capture of the original
  (pixel compare, palette compare).

### T1 - timer and interrupts (`port/vhw/pit.c`, `pic.c`, `cpu.c`)
- Owns those files + `port/oracle/test_vhw_irq.c`.
- Work: PIT modes 0/2/3 and read-back, latch semantics exercised by `read_pit_counter`,
  IRQ jitter measurement, optional nested interrupts if a handler executes STI, CPU usage of
  the `wait_for_tick` spin (e.g. SwitchToThread after N identical polls - measure first).
- Accept: existing IRQ test + new tests for latch/read order; game timer path stable (A8).

### K1 - keyboard (`port/vhw/kbd.c`, `port/host/input.c`)
- Work: complete XT set-1 table (E0 keys, E1 Pause, PrintScreen), typematic policy (SDL
  repeat vs PC typematic), LED command, BIOS INT 16h if needed, focus loss releases keys.
- Accept: test feeding scan codes through IRQ1 into the game's `update_key_state_from_scan_code`
  (bitmaps equal expected); pause (P), cheat code entry, Ctrl+Alt+Del-style exit chords work.

### M1 - mouse and joystick (`port/vhw/mouse.c`, `port/vhw/joy.c`, input part of `host/input.c`)
- Work: relative mouse mode / capture UX, MS driver acceleration (threshold), sensitivity
  functions; SDL gamepad -> gameport axes/buttons (count-based one-shot already in place),
  calibration flow of the game.
- Accept: racket control in play feels 1:1 at default sensitivity; `detect_joystick` finds the
  gameport with `KE_JOY=1`; calibration completes.

### S1 - audio output (`port/vhw/sb.c`, `port/vhw/dma.c`)
- Work: DSP command coverage used by A9/A10, auto-init vs single-cycle as the game uses it,
  underrun policy, latency, SB Pro mixer, speaker; `BLASTER` string.
- Accept: with A9+A10 music and effects play; DMA half-buffer IRQ cadence matches the
  programmed rate (+-1 %); no underruns in a 10-minute run.

### D1 - DPMI, memory and data layout (`port/vhw/intsvc.c`, `lowmem.c`, `port/tools/gcc_pack_data.py`)
- Work: order each unit's `.bss` like Watcom's `_BSS` (hash order; tools/bssorder.py on the
  frozen tree explains it) inside the launcher, or prove no code depends on BSS adjacency;
  audit every INT service against DOS/4GW behaviour (log shows none unimplemented at P0).
- Accept: `check_layouts.py --data` extended to `.bss`: 0 differing pairs, or a documented
  proof per differing pair that nothing overlays it.

### F1 - files and persistence (`port/host/clib.c` + new `port/host/files.c`)
- Work: data directory resolution (case-insensitive names), redirect writes (high scores
  `KE_SCORE.LST`) to a user directory while reading defaults from the data directory, audit
  text/binary `fopen` modes, `_splitpath` users, launch_print_order_form (spawn refused).
- Accept: high score written and re-read across runs; data directory read-only works.

### P1 - main loop and presentation (`port/host/main_sdl.c`, `present.c`, `gamethread.c`)
- Work: frame pacing (present on virtual retrace edges, not host vsync only), fullscreen
  toggle, integer/aspect scaling options, window title/icon, clean shutdown paths, optional
  GUI subsystem build with the startup report shown in the window.
- Accept: no tearing on page flips (with V1), steady 70/60 Hz presentation independent of
  monitor refresh, quit from any screen within 100 ms.

### O1 - oracle infrastructure (`port/oracle/oracle.c`, `oracle.h`)
- Work: VGA memory hook - reserve a no-access window where the original code expects video
  memory (e.g. relaunch/reserve A0000h..BFFFFh in the oracle process, or relocate the `0A0000h`
  immediates of the loaded image through a patch table), decode the faulting memory
  instructions used by the renderers (`mov`, `movzx`, `stos`, `movs`, `rep`, `and/or/xor
  mem`) and route them to `vga_mem_*`; fixture helpers to snapshot/compare the four planes,
  DAC and register files; a runner that executes original C routines (e.g. `set_display_mode`)
  on the virtual PC for whole-screen comparisons.
- Accept: `mov_mem`/`clear_video_bytes` video paths of the pilot module verified; A6/A7 can
  compare plane memory directly.

## Suggested order

A1, A3, A5 (menu visible) -> V1, A7, A6 (screens) -> K1, M1, P1 (playable) -> A8/T1 (original
timer path) -> A9, S1, A10 (sound) -> D1, F1, O1 in parallel throughout.
