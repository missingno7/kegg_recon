# Lockstep state diff: original KE.EXE vs the port (work package L1)

Screen captures cannot say *why* a frame differs. The lockstep tool runs the **original
KE.EXE machine code** and the **port** on the same deterministic virtual PC, with the same
input schedule, and compares the complete machine state at every frame.

```
cmake --build build/port                      # builds build/port/oracle/ke_lockstep.exe
python port/tools/le_export.py                # once: ke_image.bin / ke_symbols.txt
python port/tools/lockstep.py --frames 1100 --click-every 100:60:150:82:400 \
    --replay D:/Games/DOS/dos_recosystem/kegg_forged/artifacts/replay-inputs/pmrec_20260723_200732.input.json \
    --replay-offset 400
```

Output (`build/port/lockstep/report.txt`): `NO DIVERGENCE in N frames`, or the first differing
frame with every differing item (machine clock, PIC, PIT, VGA registers/latches/DAC, 1 KiB VGA
plane blocks, heap, BIOS data area, DGROUP symbols with both values), then the first frame at
which each further item diverges. Options: `--event F:M:x:y:buttons` / `--event F:K:hexscan`
(frame-keyed input), `--click-every FIRST:STEP:X:Y[:END]`, `--idle-keys 39,b9` (keys fed to
blocking BIOS keyboard waits, default Space for the startup report), `--full-at a,b` (full VGA
planes in the dump), `--only port|orig`, `--skip-run` (re-compare existing dumps).

Diagnostics of the runner itself (`build/port/oracle/ke_lockstep.exe --mode port|orig ...`, see
the header of `port/oracle/lockstep.c`): `--io-trace FILE [--io-trace-first F --io-trace-frames L]`
writes every port access and INT service with the machine clock (diff the two files to find the
first differing I/O), `--vga-trace` adds every VGA memory write with its caller (port: C return
address, `addr2line`; original: EIP of the emulated instruction, subtract the object-1 base
printed at start), `--watch HEX [--watch-from F]` sets a hardware write watchpoint and prints
EIP and callers of every write.

## Design (PROVEN by the runs below)

- **One executable, two processes.** `ke_lockstep.exe` links the whole port plus the oracle.
  `--mode port` runs the port's `main`; `--mode orig` maps KE.EXE's LE objects
  (`oracle_load`: fixups, VGA operand relocation) and calls the original `main` (obj1 13A95h).
  Both processes relaunch themselves with the same reserved ranges (DOS memory E0000h..,
  legacy VGA window A0000h no-access, a fixed heap arena at 20000000h), so the layouts match.
- **Deterministic machine (`vhw_lockstep`, port/vhw/cpu.c).** No device threads. The clock
  advances only by emulated events: 1 us per port access, one memory-poll iteration of
  `wait_for_tick` jumps to the next PIT edge (>= 1 us), blocking BIOS waits by their timeout,
  the VGA "far from retrace" poll skip by its computed time. The PIT raises IRQ0 when the clock
  crosses its next channel-0 edge (`vpit_lockstep_update`). Interrupts are delivered
  synchronously where the port already takes them (`vhw_leave`, STI, poll yield); the replay
  pump runs every emulated millisecond on the same clock (`ke_replay_use_clock`).
- **Same devices, same inputs.** Both runs use the same vhw devices and `port/host/replay.c`
  (KEPORTREPLAY events keyed by the Nth `wait_for_tick` entry; kegg_forged JSON is converted by
  smoke.py's converter). The frame hook is the port's `--wrap=wait_for_tick`; in the original an
  INT3 at `wait_for_tick` (09D40h) calls the same hook (calls from `start_timer` are skipped in
  both, as ld's `--wrap` does not see calls inside t06.c).
- **Original environment.** Every Watcom clib entry the game code calls that talks to
  DOS/DOS4GW (malloc/free -> the fixed arena, fopen/fread/fwrite/fseek/ftell/fclose, printf,
  exit/atexit, getenv, kbhit/getch, int386/int386x, spawnlp; found by disassembling every
  manifest function for calls into the runtime range) jumps to the port's host implementation.
  All other code - game C, assembly, pure clib routines - is original machine code. The oracle
  VEH emulates IN/OUT, CLI/STI, INT n, VGA-window and real-mode low-memory accesses (IVT/BDA
  -> `ke_lowmem_shadow`, incl. CMP/TEST forms), PUSHFD/POPFD (INT3-trapped: virtual IF, IOPL 3,
  as DOS/4GW) and the timer poll (INT3 at 09DBCh -> `vhw_cpu_poll_yield`, the PORT: line of
  t06.c). Original handlers are called through an IRETD frame; pending IRQs after an emulated
  instruction are taken by redirecting the thread to a thunk (never inside the exception).
  `set_display_mode`'s `VGA_EXTENDED_MEMORY_BASE` 280000h (= A0000h*4, the chunky page origin)
  is relocated like the other A0000h operands (it wrote to the unguarded legacy window before).
- **State mapping.** DGROUP is compared in the original's layout: each port global of the
  historical units, asm translations and generated data is placed at its object-3 offset
  (manifest symbols; unnamed initialized globals by the nearest named one of the same object;
  internal TASM `_DATA` labels from `gen_asm_stubs.parse_module`; the CONST block by G2's
  `__ke_original_const3_*` labels). Pointers compare as tokens: object/symbol offsets, function
  names, VGA addresses (A0000h/280000h vs the relocated alias), heap arena addresses.
  `KNOWN_TABLES` (asm-internal jump tables with code addresses; never read by C) are skipped.

## Divergences found and fixed (PROVEN: first differing frame/I/O, fix, rerun identical)

| # | where | root cause | fix |
|---|---|---|---|
| L1 | first I/O difference, startup (`measure_pit_channel0`) | the translation re-read 3DAh to get "AL after wait_for_vsync": one extra I/O shifted the PIT latch (Synchro $427F vs $427D) and every later clock | `vhw_last_inp_value` = AL left by the last Watcom `inp()` (saved/restored across IRQs) |
| L2 | frame 242, menu, VGA planes (text sprites shifted by 1 byte in later rows) | kind-5 BOB: the original decrements EBX after each plane, so plane N advances rows by `(stride - (width - N)) >> 2`; the port used one value | per-plane row advance, `vga_row_advance` as original |
| L3 | frame 244, `sprite_command_cursor` | public cursor not advanced per command | updated each command |
| L4 | frame 246, VGA planes (background restore) | encoded kind-5 restore re-derived wrongly: it treated the pixel-run stream as data runs and ignored clipped top rows | literal translation of `kind5_restore_encoded_background` |
| L5 | frame 281, `sprite_clip_bottom` | clip globals stored before the fully-clipped test; left clip must skip the right test; 16-bit CX arithmetic | literal order and widths |
| L6 | frame 282, VGA planes (left-clipped kind 5) | `sar [sprite_clip_left],2` and the `inc [sprite_clip_left]` when the stream index wraps were missing | literal |
| L7 | right-clipped kind 5 (found by the added DGROUP coverage of asm labels) | re-derived path: wrong per-plane visible columns / globals | literal `draw_kind5_right_clipped` |
| L8 | `pit_sample_auxiliary` | the translation stored the sample; the original never writes it | local value |

Regression tests: `ke_oracle` "L1 ..." (full I/O sequence of `measure_pit_channel0` on the
lockstep clock), "L2/L5/L6/L7 ..." (kind-5 widths 5..19, every x phase, left/right/top/bottom
and full clipping: four planes + renderer globals), "L4 ..." (encoded background restore).
The pre-fix translations fail all three (mutation check); `lockstep.py` itself is the
end-to-end regression (NO DIVERGENCE expected).

**PROVEN root cause of the gameplay crash** (write to FF530000 in `update_game_balls`, R1): the
anti-cheat checksum in `update_racket_state` XORs the CONST string block; with gcc's literal
layout it was non-zero, the game set `current_ball_count = 5000`, `remove_game_ball` memmoved
far past `game_balls` and overwrote `image_buffer_cursor` with the XMS entry F000:FF53. G2's
CONST layout (`gcc_pack_data.py`, merged from portable-sdl3) fixes it; lockstep confirmed the
state before the crash frame was identical and, after the merge, no crash and no divergence.

## Results

- 6000 frames (startup report, title, menu, game entry, Level 01, ~90 s of gameplay with a
  click every 60 frames and the pmrec mouse replay from frame 400): **NO DIVERGENCE**
  (clock, PIC, PIT, VGA registers/DAC/planes, heap, BDA, 56 KB of mapped DGROUP).
- Wall-clock R1 captures after the fixes: first-gameplay 0.82% (was 15.38%), level-01 8.05%
  (was 26.16%; LEVEL text fly-in phase), game-entry sampled a different screen (host timing of
  the capture, not state: the frame-synchronized lockstep is identical).

## Limitations / open

- `ke_oracle` "vhw async IRQ0 into a busy-wait" (host-time async mode, not lockstep) is load
  sensitive on this machine: with the pre-L1 translation 2 of 4 runs failed (695/697 PIT
  edges for 701 retraces, limit 698), with L1 2 of 4 (696/696); the final gate run passed
  (700/701). PROVEN not a regression of L1; the async IRQ0-to-retrace margin belongs to T1/T2.

- Sound Blaster, DMA and gameport are off in lockstep runs (their device threads/timing are
  not clock-driven yet); audio-driven game state (sound-active flag) is therefore not covered.
- Not compared: stack, the original's clib data (the port uses the host CRT), BSS globals the
  manifest does not name, CONST bytes outside the game's block, the IVT, DOS memory blocks.
- IOPL reported by `probe_cpu_environment` is 3 in both runs by construction (HYPOTHESIS for
  the historical DOS/4GW value, as before).
- The frame key is the wrapped `wait_for_tick`; `start_timer`'s two internal calls are not
  frames in either run.
