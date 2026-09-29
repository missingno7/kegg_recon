# Android port architecture

Branch `android` (from `portable-sdl3`). The Android app runs the same thing the Windows
SDL3 port runs - the ORIGINAL game code (`src/*.c`, only the `PORT:` lines listed in
`docs/port/src-delta.md`) plus the literal C translations of the TASM modules (`port/asm`)
on the virtual PC (`port/vhw`) - under SDL3 on 64-bit Android (arm64-v8a, and x86_64 for the
emulator). Status markers as in AGENTS.md: **PROVEN** (a tool or test named here shows it),
**STRONG**, **HYPOTHESIS**.

The rule of `docs/port/architecture.md` applies unchanged: no game logic, constant, timing or
control-flow change to compensate for the host. Everything Android-specific is port code.

## 1. What the game reacts to (input analysis)

Evidence: source reading of `src/` (file:line below), done for this phase. Input is sampled
only inside `wait_for_tick` (t06.c:228-241): bit 0 keyboard (`dispatch_keyboard` +
`poll_keyboard`), bit 1 mouse (`update_mouse`, t17_mouse.c:132-163, INT 33h function 03,
position halved, 4-sample average for the racket). Menu, gameplay and boss fight call
`wait_for_tick(3)`; high-score name entry uses keyboard only (u_00708.c:2292).

| Screen (VGA scan-out) | Left button | Right button | Keys | Ends on |
|---|---|---|---|---|
| Intro title (320x400) u_00708.c:1725-1776 | any new press skips | same | Space, Esc skip; S sound | input / music end |
| Main menu (320x240) u_00708.c:1885-2004 | new press of any button selects the item under the pointer (hotspots u_07bd5.c:178-183: X 91..222; Y 70-93 play, 111-134 hall of fame, 151-174 instructions, 191-214 order info) | same | Esc = quit to DOS; Space = restart-code entry; LCtrl+click = mouse sensitivity | selection |
| Click at exactly (0,0) on the menu | quits (u_00708.c:1990-1993) | same | - | - |
| Restart code (on the menu) u_00708.c:2016-2081 | - | - | Space opens, hex digits by ASCII (keypad digits: the game's table is AZERTY), Backspace, Enter | Enter |
| Level fly-in / "LEVEL xx" (320x200) u_02f0c.c:337-374 | any new press skips | same | Esc, Space | input / timeout |
| Gameplay (320x200) u_00010.c:681-746 | launch caught ball (u_08585.c:287-292) | fire racket gun while that spell is active (u_0608a.c:281-288) | P pause; Esc = give up the round (costs life balance, u_00010.c:734-752); RShift+Bksp hold-pause; cheats; S | - |
| Pause (P) t16_keyboard.c:331-340 | mouse not polled | - | only P resumes | P |
| Boss fight (320x200) u_06b02.c:291-356, 484-500 | single shot | spread shot | Esc forfeits | boss dead / 100 s |
| Game over / hall of fame (320x200) u_00708.c:2136-2314 | new press continues | same | name: letters/digits by ASCII, Backspace, Enter/Esc | input / timeout |
| Instructions, ending pages (320x200) | new press continues | same | Space, Esc | input / timeout |
| Order info (320x240) u_00708.c:1806-1883 | left only exits | - | P = print order form (refused: no DOS programs) | input |

Mouse ranges (INT 33h functions 7/8, driver units = 2x game pixels, t17_mouse.c:196-231):
menu X 0..319 Y 0..196; gameplay Y locked at 188 (u_04cf2.c:529-536; 24..188 with the
shrink spell), X within the racket limits; boss X locked at 4, Y 16..149 (u_06b02.c:433-441).
The racket follows `mouse_x/y_recent_average` (u_04cf2.c:405-408). Joystick: never polled
by the frozen game (STRONG, docs/port/building.md).

## 2. Touch UX

Landscape only (`sensorLandscape`), immersive full screen, the 4:3 game image centred at the
largest size that fits (non-integer scale on Android; nearest filtering), so a phone has
pillarbox areas left and right. Overlay controls are drawn by the presentation layer with
SDL (never into the VGA image) and are hit-tested in window coordinates.

**Gameplay** - derived only from port-side state: the scan-out is 320x200 AND the virtual
mouse driver's ranges are the play ranges (vertical maximum 376 = racket row 188, or the
horizontal range locked = boss). After a game ends the ranges stay until the menu resets
them, so the game-over screens count as gameplay too; their "any button" waits accept the
overlay buttons.
- Any finger that is not on an overlay button is the pointer finger: the racket goes where
  it touches, through the one canonical viewport mapping (`ke_present_map_mouse` ->
  `vmouse_set_absolute_position`, NM1's native-mouse seam; touches outside the image clamp
  to its nearest edge). The game's own `update_mouse()` smoothing stays the consumer.
- Left pillarbox: big **L** button (left mouse button: launch ball / boss single shot).
  Right pillarbox: big **R** button (right mouse button: gun / boss spread shot) and a small
  **pause** button (sends P). Multi-touch: one thumb steers, the other presses a button.
  On a screen without room in the pillarboxes the buttons become translucent corner
  buttons over the image.

**Menus and other screens** - tap = move the pointer to the tap and press the LEFT button
while the finger is down (the menu, intro, order screen and every "press any button" wait
select by position + new press). A press is held until the game has read it through INT 33h
function 03 at least once (quick taps shorter than one game tick are not lost), at most
150 ms longer than the finger. Taps outside the image are ignored (they would otherwise
clamp to an edge; the menu quits on a click at exactly (0,0)). A small **keyboard** button
(top right) toggles the Android soft keyboard for the restart code and the high-score name:
typed characters are translated through the inverse of the game's own scan-code table
(`scan_code_to_ascii`, t16_keyboard.c:170, AZERTY) and paced one make/break per 2 game ticks
so the tick-sampled keyboard sees each press. Physical keyboards keep the Windows mapping.

**Android back** - during gameplay: P (pause / resume; the game's own pause screen). On every
other screen: Esc (skip / leave; on the main menu Esc quits the game and the activity
finishes - the natural Android "back from the root screen"). The game's gameplay Esc (give up
the round, costs a life's worth of balance) is deliberately not on Back.

## 3. 64-bit: the ILP32 game world

### Why not simply compile for LP64
Pointer *values* can be made lossless by keeping everything below 2 GB (the game converts
`int` to pointers, which sign-extends: 2 GB, not 4 GB, is the limit). Pointer *storage* cannot:
an LP64 audit of `src/` found about eleven gameplay-breaking sites (PROVEN by source reading,
this phase), e.g.
- globals defined `int` in one unit and declared as pointers in another
  (`current_ball_pointer` u_07f8a.c:124 vs 9 units, `falling_spell_cursor`, `palette_base`,
  `vga_buffer_base`, `file_buf_ptr` ...): an 8-byte store clobbers the next variable;
- records overlaid on pointer-bearing structs: the picture record at `picture_pixels8`
  (t08.c:29-97, written with fixed 4-byte fields by m_11df8/m_0a284), transition tracks read
  through `struct TransitionSpritePath` (u_082cc.c:116), the 57-byte interrupt records
  (u_0d4ba.c:94-126);
- fixed record sizes and strides (`memcpy(..., n * 0x20)` for AnimatedSprite, u_07bd5.c:404;
  FallingSpell 0x12, u_04066.c:234; `(prompt << 2)` pointer tables, u_00708.c:2068).
Fixing those would change game code beyond mechanical conversions, so the data model is kept.

### The two worlds
The game units, the asm translations and their generated `_DATA` form the **ILP32 world**,
compiled with a 32-bit-pointer code generator for the same CPU:

| ABI | ILP32 code generator | assembled as |
|---|---|---|
| arm64-v8a | clang `arm64_32-apple-watchos` (AArch64 instructions, ILP32) | `aarch64-linux-android` ELF |
| x86_64 | clang `x86_64-linux-gnux32` (x32) | `x86_64-linux-android` ELF |

`port/android/tools/ilp32_world.py` compiles each unit to assembly, rewrites it for the
64-bit ELF target (Mach-O relocation operators and sections -> ELF; a 32-bit GOT load
becomes direct PC-relative address materialization), restores declaration order (clang has
no `-fno-toplevel-reorder`), applies the Windows build's own layout step
(`port/tools/gcc_pack_data.pack`: packed `_DATA`/`_BSS`, the frozen Watcom BSS order, the
original CONST block from KE.EXE), and assembles. The virtual PC, the SDL host and the
platform layer are ordinary LP64 code (**LP64 world**) in the same shared object.

PROVEN (`port/android/tools/check_ilp32_layout.py`, both ABIs): all 609 data symbols of the
49 units sit at the same section offsets as in the Windows i686 objects (which
`check_layouts.py --data` proves against the original), every data byte outside relocation
sites is identical (including the 0x2452-byte original CONST block), the relocation sites
are the same, and 53 struct definitions match `docs/types.md`.

### Crossing the boundary
- 32-bit absolute data pointers (`.long sym`) cannot be dynamic relocations in an Android
  shared object. The tool zeroes each slot and lists {slot, target} as 64-bit pairs in
  section `ke32_relocs`; `ke32_apply_relocs()` stores the 32-bit values once before the game
  starts and refuses any target not below 2 GB.
- Calls from the ILP32 world to the host go only through functions whose parameters and
  results are scalars of at most 32 bits. C-library calls are renamed to `ke32_*` shims
  (`port/android/ilp32/ke32_shims.c`) that take pointers as `uint32_t`; FILE handles are
  low slots; `malloc` comes from the low arena. Variadic functions cannot cross (arm64_32
  passes variadic arguments on the stack in 4-byte slots): `printf`/`spawnlp` are compiled
  inside the ILP32 world (`ke32_clib.c`) and pass a finished buffer. The build fails if an
  ILP32 object references anything outside this allow-list.
- The ILP32 code generator assumes naturally aligned globals, but the Watcom layout packs them:
  AArch64 scaled `ldr/str [x, :lo12:sym]` accesses are rewritten to `add` + unscaled access
  through a temporary register the function does not use (x86 needs nothing).
- Calls from the host into the ILP32 world are `void (void)` (historical `main`, ISRs from
  the PIC vector table, atexit handlers).
- Floating point: the heightfield generator (u_0acdd.c) evaluates `int * float * float`
  and truncates. x87 (original, Windows port) keeps the intermediate in extended precision;
  the ILP32 world is compiled with `-ffp-eval-method=double`, which is exact for these
  operand sizes (products below 2^42), so truncation sees the same value.

### Memory below 2 GB
`libmain.so` (tiny, loaded by SDL's Java code) is a loader. Before anything else it reads
`/proc/self/maps`, picks a free window below 2 GB and reserves it (PROT_NONE):

| region | size | use |
|---|---|---|
| 0x000E0000..0x0010FFFF | 192 KiB | identity-mapped DOS memory, `lowmem.c` (fixed; Android `mmap_min_addr` is 32 or 64 KiB) |
| game image | reserved for `libkegame.so` | `android_dlopen_ext(ANDROID_DLEXT_RESERVED_ADDRESS)`: code, data, BSS, CONST |
| game heap | 64 MiB | `ke32_malloc` arena (the game allocates < 1 MiB) |
| game stack | 8 MiB | the game thread's stack (`pthread_attr_setstack`), and the ISRs that run on it |

`libkegame.so` holds the ILP32 world, the virtual PC and the host (everything the game can
point at, including `ke_lowmem_shadow`). It links `libSDL3.so` normally; SDL's own objects
may live anywhere. All symbols are hidden (`-fvisibility=hidden`) except the entry point, so
PC-relative references never need a GOT.

### Behavioural proof
`port/android/tools/lockstep64.py` runs the Android build and the Windows i686 port on the
same deterministic lockstep machine with the same inputs and compares every frame (clock,
PIC, PIT, VGA registers/DAC/planes, heap, BIOS data area, SB/DMA state, the game's globals with
pointers compared as tokens). PROVEN: NO DIVERGENCE in 6000 frames (clicks + mouse replay +
sound) for x86_64 and for arm64-v8a (the arm64 code run under the emulator's ARM translation,
inside the app). One known, excluded field: `saved_ds` (copy_ds_to_es stores the host DS
selector on i386, the virtual flat selector 0170h on 64-bit; never read by the game).

## 4. Platform abstraction

`port/platform/ke_platform.h` is the only OS seam of `port/vhw` and `port/host`
(recursive mutexes, atomics, auto-reset events, high-resolution waits, threads, thread
interruption, fixed mappings, fault handlers, paths). `platform_win32.c` contains the exact
Win32 calls the port made before (moved, not rewritten); `platform_posix.c` is the Android
version. PROVEN: after the split every `port/tools/mergepkg.py` gate passes (build, oracle
incl. the async-IRQ busy-wait test, layouts, stubs, smoke, both 1100-frame lockstep runs).

### Asynchronous interrupts on POSIX
Windows: the IRQ thread suspends the game thread, checks IF=1, `vhw_game_depth == 0`, no
handler running, EIP inside the executable's code or the game at the scheduler's safe wait,
then runs the handler on the IRQ thread. POSIX has no SuspendThread, so the IRQ thread sends
a real-time signal (SIGRTMIN+5) to the game thread; the handler reads the interrupted PC from
the signal context and applies the same checks (PC inside `libkegame.so`'s executable
segment), then runs the ISR on the game thread itself, on the low game stack, and reports
back through a semaphore. Consequences, all mirrored explicitly:
- "on the IRQ thread" decisions (VGA status polls inside IRQ0 must not sleep, `vhw_idle`
  inside a handler) use `vhw_on_irq_thread()`, which is also true while an asynchronous
  handler runs on the game thread;
- `exit()` inside such a handler longjmps to the game thread's exit directly (Windows
  redirects the suspended thread's context into `ke_exit`); the interrupt entry is undone
  first (IF=1, no ISR, IRQ0 clock scope closed) and the IRQ thread is released;
- the game thread's syscalls restart (`SA_RESTART`); the 100 us poll wait is simply cut short.
Mutexes are recursive on both hosts (CRITICAL_SECTION semantics).

## 5. Asset import (first run)

Game data is never shipped. The launcher activity checks the app's internal storage
(`files/game/`) for the 25 required files (`required_assets` in `port/host/config.c`). If any
is missing it shows a short explanation and two buttons: **Choose folder**
(`ACTION_OPEN_DOCUMENT_TREE`) and **Choose ZIP** (`ACTION_OPEN_DOCUMENT`). The selection is
scanned case-insensitively (any subfolder level for ZIPs), every `KE_*` data file (plus the
optional publisher pages and `KE_SCORE.LST`) is copied with its canonical upper-case name,
the required set is validated, and only then is the game started. Nothing outside the
selection is read and no permission beyond the picker's grant is needed. High scores are
written to the app's own storage (`files/Krypton Egg/ke_score.lst`), config to
`files/Krypton Egg/krypton-egg.ini`.

## 6. Lifecycle

- **Backgrounding**: on `SDL_EVENT_WILL_ENTER_BACKGROUND` (delivered synchronously from the
  Java UI thread) the host freezes virtual time (`ke_time_pause`: `ke_now_ns()` stands still)
  and asks the game thread to park at its next virtual-PC boundary (`ke_check_pause` in
  `vhw_leave`, the memory-poll yield and blocking BIOS waits - never inside a device lock or
  an ISR). The PIT sees no time pass, so no interrupts are generated; SDL pauses audio. On
  `DID_ENTER_FOREGROUND` time resumes from the frozen value and the game continues exactly
  where it was: no catch-up burst, no lost PIT edges.
- **Quit**: the game exits by itself (menu Esc) -> the game thread finishes -> the SDL main
  returns and the activity finishes. Android destroying the activity -> `ke_request_quit()`
  (the game thread unwinds at its next boundary).
- **Audio**: the virtual Sound Blaster feeds an SDL audio stream (AAudio/OpenSL ES) exactly
  as on Windows.

## 7. Build layout

```
android/                      Gradle project (wrapper), app module (Java launcher, SDLActivity subclass)
android/app/jni/CMakeLists.txt  SDL3 from source + libmain.so (loader) + libkegame.so
port/android/                 Android host code: loader, main, touch/overlay, import JNI, ILP32 world
port/platform/                platform layer (win32 / posix)
```
See `docs/android/building.md`.
