# SDL3 port architecture (P0 foundation)

Branch `portable-sdl3`, started from tag `historical-exact-clean-v1` (the byte-exact
reconstruction, docs/freeze.md). Status markers follow AGENTS.md: **PROVEN** (checked by a
tool or test named here), **STRONG** (consistent evidence), **HYPOTHESIS**.

## Principle

The historical `src/*.c` stay in place and are compiled as they are; the port delta in
`src/` is minimal and every changed line carries a `PORT:` comment
(`git diff historical-exact-clean-v1 -- src` is the complete list: 6 files, 11 lines). The branch's `src/` therefore no longer rebuilds KE.EXE with Watcom; the tag remains the historical oracle.
Everything platform specific lives in `port/`:

| path | content |
|---|---|
| `port/include/watcom/` | Watcom compat headers force-/include-d into the historical units only |
| `port/vhw/` | the small "virtual PC" the unchanged game runs on |
| `port/host/` | SDL3 main thread, game thread, logging, presentation, input, clib bridges |
| `port/asm/` | C translations of the TASM modules (one file per `asm/*.asm`, owns it) |
| `port/stubs/` | generated link stubs for every TASM public not yet translated |
| `port/oracle/` | native differential oracle: original KE.EXE machine code vs translations |
| `port/tools/` | generators and checks (stubs, data packing, layouts, LE export, smoke) |

Rather than rewriting hardware-facing game code, the port emulates the machine that code
talks to. The 44 C functions `docs/porting.json` classes REIMPLEMENT (timer, PIC/IRQ install,
DPMI, VGA registers, SB detection, keyboard LEDs...) therefore run unmodified on `port/vhw`;
only the 44 assembly routines are translated (to C, instruction-order faithful).

## Build and run

```
set PATH=C:\msys64\mingw32\bin;%PATH%          (bash: export PATH=/c/msys64/mingw32/bin:$PATH)
cmake -S port -B build/port -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/port
build/port/ke_sdl3.exe assets                  # data directory = assets/ (KE_* files)
python port/tools/smoke.py                     # scripted start/idle/quit + report
python port/tools/le_export.py                 # once: build/port/oracle/ke_image.bin
build/port/oracle/ke_oracle.exe build/port/oracle   # differential + vhw tests
python port/tools/check_layouts.py --data      # struct + _DATA layout vs the original
python port/tools/gen_asm_stubs.py --check     # generated stubs up to date
```

32-bit only (i686 gcc 16.2 from MSYS2 mingw32; SDL3 3.4.16 i686 at
`C:/tools/sdl3-3.4.16-i686`, `-DKE_SDL3_DIR=` to override): the game keeps pointers in `int`.

Runtime environment: `KE_IRQ=async|sync`, `KE_WINDOWS=1` (answer as a Windows DOS box),
`KE_SB=1` (attach Sound Blaster 220h/IRQ7/DMA1), `KE_JOY=1` (gameport), `KE_SCALE`,
`KE_ASPECT=0|1`, `KE_LOG_LEVEL=0..4`, `KE_LOG=path`, `KE_DOSENV="A=b;C=d"`, and for scripted
runs `KE_AUTOKEYS="ms:scan,..."`, `KE_SCREENSHOT="ms:file.bmp"`, `KE_EXIT_AFTER_MS`.
The log (`ke_sdl3.log` next to the exe) lists every stub reached; at quit it prints the game
thread's call chain (`smoke.py` resolves it with addr2line).

## Inventory that drove the design (PROVEN by grep/disassembly of the frozen tree)

| kind | occurrences |
|---|---|
| I/O ports (C) | PIT 40h/43h, PIC 20h/21h/A1h, KBC 60h/64h, gameport 201h, SB base+4/+5 (mixer test), VGA 3C0h-3CFh, 3D4h/3D5h, 3DAh, DAC 3C7h-3C9h |
| I/O ports (asm) | PIT, VGA sequencer/GC/CRTC/DAC, SB DSP (+6,+A,+C,+E), 8237 DMA (0Ah-0Ch, channel and page ports), PIC EOI (`asm_inventory.py`: 158 IN/OUT) |
| INT services | 10h: 00h, 0Fh, 1A00h; 21h: 25h, 30h, 35h, 3567h, 48h; 2Fh: 1687h, 4300h, 4310h; 31h: 0100h, 0101h, 0200h, 0201h, 0204h, 0205h, 0400h, 0500h; 33h: 00h, 03h, 04h, 07h, 08h, 1Ah, 1Bh, 24h; 67h: DEh (VCPI), 46h; Windows is detected with `getenv("WINDIR")` |
| absolute addresses < 64 KiB | 417h, 41Ah, 41Ch (BIOS keyboard, t16), 19Ch (INT 67h vector, u_0d4ba), CCh (INT 33h vector, t17) |
| DOS memory | DPMI 0100h blocks dereferenced at `segment << 4` (IRQ templates copied there, keyboard flag buffer, DMA/tracker buffers) |
| VGA memory | `0xA0000` only as a value passed to asm (`frame_buffer_base`, `clear_video_bytes_entry`); all direct accesses are in the asm renderers |
| interrupt handlers | `keyboard_interrupt_handler` (C, IRQ1), `sound_test_irq_handler` (C, SB IRQ probe), `pit_channel0_interrupt` (asm, IRQ0, waits for retrace inside the ISR), `sound_blaster_irq_handler` (asm); installed through INT 21h/25h and DPMI 0205h; USE16 real-mode templates (irq.asm) copied to DOS memory |
| CLI/STI | t06 timer reset/start, t15 joystick timing loop, t16 LED update, u_0dfc3 register programming, t19 exit path; asm m_09f64, m_11258, m_11530, m_137a8 |
| busy waits | `wait_for_vsync` (3DAh), `wait_for_keyboard_controller` (64h), pause/prompt loops via `poll_keyboard` (60h), `measure_joystick_axes` (201h with IF=0), and **`wait_for_tick` spinning on `retrace_count5` / `*timer_sync_flag_ptr` with no I/O at all** when the PIT handler is installed (t06.c) |

## Decisions

### Interrupt delivery: asynchronous with a synchronous fallback (option a, hybrid)

Evidence: with the timer installed, `wait_for_tick` spins on plain memory updated only by the
IRQ0 handler, so delivery at yield points (option b) would hang there unless the source were
changed. Handlers also run at arbitrary points of the historical frame loop. The chosen model
(`port/vhw/pic.c`, `cpu.c`):

- Devices raise IRQs from their own threads (PIT thread, SDL audio callback, SDL main thread
  for the keyboard). The 8259 pair keeps IRR/ISR/IMR, priorities, EOI, cascade.
- The IRQ thread suspends the game thread (`SuspendThread` + `GetThreadContext`) and calls the
  handler while the game is frozen at an instruction boundary, only if IF=1, no handler is
  running, the game thread is not inside a vhw service (`vhw_game_depth`) and its EIP is in
  the exe's `.text` (not inside a CRT/SDL/Windows DLL that may hold locks). Otherwise the IRQ
  stays pending; the IRQ thread retries in 20 us steps for 2 ms.
- Every vhw entry (IN/OUT, INT, CLI/STI, BIOS console) is bracketed by `vhw_enter/leave`;
  `vhw_leave` and STI deliver pending interrupts synchronously on the game thread, exactly
  where a CPU would take them after the instruction.
- CLI/STI (`_disable/_enable`, and the translations) set the virtual IF; an interrupt entry
  clears IF and restores it on return. No nested handlers (one at a time).
- `exit()` inside a handler running on the IRQ thread redirects the frozen game thread into
  `ke_exit` (SetThreadContext) and abandons the handler.
- `KE_IRQ=sync` disables the IRQ thread (debugging); the timer-installed path then hangs by
  design.

PROVEN by `ke_oracle` test "vhw async IRQ0 into a busy-wait, CLI/STI": a handler installed
via DPMI 0205h interrupts a pure-memory spin at the programmed 1000 Hz (200 IRQs in 200 ms),
nothing is delivered during a 50 ms CLI, the pending IRQ is taken at STI. Handlers run on
another thread, so they get their own x87/TLS state (more robust than DOS). CPU cost: the
historical busy waits spin as they did; `inp(3DAh)` far from the retrace sleeps instead.

### Memory

- Linear address == host address (the game stores pointers in `int`).
- Real-mode memory is identity mapped: DOS blocks are handed out at `segment << 4` in a
  deterministic free window `0xE0000..0x10FFFF` (DOS arena E000h-FEFFh, BIOS page with IRET
  bytes at F000:FF53 for default vectors, HMA). The exe has a fixed image base (0x400000, no
  ASLR: an ASLR'd 32-bit image was observed at 0x90000) and relaunches itself suspended to
  reserve the window before the child's loader runs (`host/main_sdl.c`); without the window
  the loader's own allocations occupy it. Consequence: the startup report shows 124 KiB of
  conventional memory instead of ~600 KiB (the game needs < 64 KiB) **STRONG**.
- Below 64 KiB Windows maps nothing: IVT + BIOS data area are a shadow array
  (`ke_lowmem_shadow`), reached by the 5 `KE_LOWMEM()` source edits and by vhw (vectors,
  tick count 46Ch, BIOS keyboard state).
- VGA memory (A0000h) is **not** host memory: 4 x 64 KiB planes in `vga.c`, reached through
  `vga_mem_read8/vga_mem_write8` by the asm translations. The window cannot be guarded (loader
  data may live at 0xA5000..), so a direct C dereference would corrupt silently: audited
  (inventory above: none in C).
- `malloc`/`fopen`/`printf` are the host CRT (same DOS-heritage semantics: text/binary modes,
  ^Z). `exit`/`atexit`/`getenv` are redirected (`watcom_compat.h`): atexit handlers run on the
  game thread, the game sees a virtual DOS environment (no host `WINDIR`, `BLASTER` when an SB
  is attached). `spawnlp` (print order form) is refused.

### Threads

Main thread: SDL3 window/renderer/event pump, presentation each host frame. Game thread:
historical `main()` (renamed `ke_game_main` by `-Dmain=`). Device threads: IRQ delivery,
PIT, SDL audio callback. Closing the window requests quit; the game thread unwinds at its next
vhw boundary through `ke_exit(0)` (atexit handlers restore text mode etc.), else it is
terminated after 3 s.

### Presentation

`vga_scanout` converts the CRTC-visible frame (start address, offset register, dword/byte
mode, chain-4, double scan, vertical display end) to 8-bit indices; `present.c` expands the
6-bit DAC palette to XRGB8888, uploads a streaming texture and draws it with nearest
filtering at the largest integer horizontal scale, 4:3 display aspect (`KE_ASPECT=0`: square
pixels, integer scale both axes). Text mode is not rendered (the startup report goes to the
console, CP437).

### Assembly

`port/tools/gen_asm_stubs.py` re-emits every TASM module's `_DATA` byte for byte (GNU as,
original label offsets, so adjacency and initial values are historical) and a logged stub
for every public code label (97 code stubs, 120 data symbols at P0). A module is owned by a
translation as soon as `port/asm/<module>.c` exists; the generator then skips it. The USE16
IRQ templates (LE object 2) are a 0x149-byte zero blob with their public offsets: real mode
never executes; C code only copies/patches them.

## Compile issue classes (all 49 units compile and link)

| # | issue | resolution |
|---|---|---|
| 1 | K&R/old-style declaration then promoted-type definition (`u_0acdd` subdivide_heightfield, `u_0e914` set_vga_horizontal_pan_register) - gcc error | `PORT:` prototype (same stack image: Watcom and cdecl push 4 bytes per argument) |
| 2 | conflicting block-scope `extern` types in one unit (`u_0dfc3` set_gc_*: short vs unsigned) - gcc error | `PORT:` one type |
| 3 | int <-> pointer assignments/arguments/returns (43 sites) | accepted (`-Wno-int-conversion` ...): identical on 32-bit |
| 4 | implicit declarations, implicit int, redeclared void (`u_00708` 6 functions), builtin mismatches (`u_0b1df` strlen/strcpy) | accepted warnings (cdecl, return values unused) |
| 5 | `__far`/`__near`/`__interrupt`/`_far`... | macros in `watcom_compat.h`; handlers are plain cdecl functions called by the virtual PIC |
| 6 | Watcom headers `i86.h`, `conio.h`, `process.h` | compat headers: REGS/SREGS layout = Watcom 386, `int386/int386x` -> vhw INT dispatcher, `FP_SEG` -> flat selector 0170h, `inp/outp/outpw/inpw`, `_disable/_enable` as real functions (units redeclare them) |
| 7 | `void main(void)` vs SDL main | `-Dmain=ke_game_main` for `u_13a95.c` |
| 8 | Watcom clib: `exit/atexit/getenv/kbhit/getch/spawnlp` need virtual-PC semantics | redirected / reimplemented in `port/host`, `port/vhw/kbd.c`; `itoa/ltoa/_rotl/_splitpath/_stricmp/strtoul` come from msvcrt (same behaviour) |
| 9 | absolute addresses < 64 KiB (5 sites) | `PORT: KE_LOWMEM()` |
| 10 | Watcom narrow comparison: `unsigned short == -1` compiled as `cmp word,-1` (true for FFFFh); ISO promotion makes it false (`t17` mouse reset) | `PORT:` cast; `-Wtype-limits` over all units finds no other site |
| 11 | char signedness, struct packing, bit-fields | `-funsigned-char`, `-fpack-struct=1` (Watcom 10.0 default /zp1), mingw default `-mms-bitfields` (AnimatedSprite = 32 B): PROVEN by `check_layouts.py` (74 struct definitions vs docs/types.md, 0 mismatches) |
| 12 | global data layout: records are declared as consecutive globals and used through one struct pointer (57-byte interrupt records `key_irq`/`tmr_rec`/... in `u_0d4ba`) | `-fno-toplevel-reorder -fno-zero-initialized-in-bss` + launcher `tools/gcc_pack_data.py` (drops data alignment): PROVEN, 224/224 adjacent initialized-data pairs equal the original spacing. `_BSS` order still differs (Watcom hash order, 34 of 163 pairs): see work package D1 |
| 13 | non-volatile globals shared with ISRs and spun on | historical units at `-O0` (Watcom `-d2` keeps them in memory) |
| 14 | 97 TASM code publics, 120 data publics | generated stubs/data (`gen_asm_stubs.py`) |
| 15 | low memory taken by the Windows loader / ASLR | fixed image base + suspended self-relaunch with reservation |

## Where the game stops at P0 (smoke.py, PROVEN)

The startup report prints (CPU 80486 protected mode, DOS, VGA, XMS, DPMI 0.90, memory, MOUSE
6.26), the SB probe finds "a card at 210h" because the SB asm routines are stubs returning 0
and asks for SPACE (autokey), the keyboard manager installs (D4GW), mode 13h is set, assets
are loaded (decoders stubbed), and the game idles in the main menu:
`run_main_menu -> initialize_main_menu -> wait_menu_select -> wait_for_tick -> wait_for_vsync
-> inp(3DAh)`. The screen stays black because the palette, VGA register, fill, BOB and GIF
routines are stubs. 14 distinct stubs are reached: decode_and_verify_asset, decode_gif_image,
write_dac_palette, set_gc_mode, set_gc_read_map, set_seq_plane_mask, update_crtc_register,
fill_clipped_vga_rectangle, copy_chunky_scanline_to_vga, measure_pit_channel0,
set_pit_channel0_reload, reset_sound_blaster_dsp, write_sound_blaster_byte,
acknowledge_sound_blaster_irq (plus draw_bob_sprite_entry / copy_screen_span_entry with more
idle time). Closing the window unwinds cleanly (5 atexit handlers, text mode restored).

## Differential oracle (port/oracle)

`le_export.py` checks KE.EXE against manifest.json and exports its three LE objects plus the
6290 resolved fixup sites (6292 off32 + 4 sel16 records, cross-page duplicates merged).
`ke_oracle.exe` maps the objects at arbitrary host addresses, applies the fixups (off32:
target base + offset; sel16: the process DS), and calls original routines natively with the
Watcom `-3s` convention (= cdecl for int/pointer arguments; EBX/ESI/EDI/EBP preserved, DF
clear). A vectored exception handler emulates the instructions that fault in user mode -
IN/OUT (to the virtual PC), CLI/STI, INT n (to `vhw_int`), MOV Sreg - and records a trace.

Result (PROVEN): pilot translation `port/asm/m_137a8_13944.c` vs original bytes:
`mov_mem` 2000 random overlapping/forward/backward cases identical (including the backward
dword QUIRK that touches up to 3 bytes past the range), a one-constant mutant fails 5+ cases;
`clear_video_bytes` (RAM) identical; `probe_cpu_environment` identical generation/mode and
CLI,CLI,STI trace, IOPL differs by design (host user mode 0 vs emulated DOS/4GW 3,
HYPOTHESIS for the historical value).

Verifiability per module (`python port/tools/asm_inventory.py`):

| class | modules | how |
|---|---|---|
| pure memory | m_0982c (asset checksum), m_0a284 (IFF), m_11df8 (GIF), m_0a958, m_12680, m_12970 (no-op hooks) | direct native calls on RAM buffers |
| port I/O, CLI/STI, INT | m_09f64 (PIT), m_11258/m_11494 (SB/DMA), m_11530 (ProTracker: also INT 31h), m_13944/m_13a48 (VGA regs, DAC), m_12f30/m_12f9c (span copies: destination is an argument, RAM-backed tests possible) | VEH emulation available now; compare I/O traces and vhw state |
| direct video memory | m_12288, m_12684, m_12974, m_12a9c (sprite renderers, BOBs), m_13324, m_13712 (pixels, fills), m_137a8 video paths | need the VGA memory hook (work package O1): the A0000h window is not host memory |
| real mode | irq.asm | never executed; data only |

## Known limitations / open risks

- `_BSS` variables are in source order, the original used Watcom's hash order (34/163
  adjacent pairs differ); no BSS overlay is known yet (D1).
- Retrace is time-based and presentation is not yet latched to it (P1); the SB and gameport
  are off by default; text mode is not displayed.
- The keyboard translation is a table for the common keys; E1 Pause sequence simplified.
- The oracle cannot yet run routines that touch the VGA window.

## Known screen modes (ground truth from the game's author/players)
All screens are 256-colour VGA; several use unchained "mode X" layouts:
- intro: 320x400 (mode X, 400 lines)
- main menu: 320x240 (mode X)
- gameplay: 320x200
Presentation must scan these out at their native geometry (aspect 4:3 on screen). A 320x400 frame is expected
during the intro, not a doubled 320x200.
