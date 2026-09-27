# E1 screen and replay conformance

## Scripted input and captures

`smoke.py` now accepts timed absolute mouse moves (`--mouse`), left clicks
(`--clicks`), and named screenshots (`--shots`) alongside `--keys`. Coordinates
use the 320-pixel-wide game raster (up to 240 rows in the menu mode); the host
scales them to the DOS mouse range.
Clicks hold the left button for 80 ms. Each schedule is measured from process
start and fires on the first host poll/frame at or after its timestamp. The
default smoke still presses Space at 1500 ms.

The R1 sequence was run with Space at 1500 ms, clicks at 8000 and 11000 ms at
150,82, and captures at 3500, 6500, 10000, 12500, and 14000 ms. Files are in
`build/port/conformance-captures/`. `framediff.py` normalizes to 320x200 and
reports these threshold-0 results:

| Reference | Port capture | Different pixels | Mean channel error | Result |
| --- | --- | ---: | ---: | --- |
| `startup-title.png` (3500 ms) | `startup-title.bmp` | 100.00% | 55 | Title artwork is present, but the animation/fade phase differs. |
| `main-menu.png` (6500 ms) | `main-menu.bmp` | 0.00% | 0 | Exact match after DAC expansion adjustment. |
| `game-entry.png` (10000 ms) | `game-entry.bmp` | 5.62% | 2 | Close; residual pixels differ. |
| `level-01.png` (12500 ms) | `level-01.bmp` | 26.16% | 23 | Port is still in a transition with visible sprite/scanout artifacts. |
| `first-gameplay.png` (16000 ms) | `first-gameplay.bmp` (14000 ms) | 15.38% | 12 | Nearest safe capture is early; it has not reached the referenced active frame. |

The port's 1200 ms screenshot target cannot capture the DOS text startup report:
the SDL presenter only saves VGA graphics scanout, so that scheduled shot is
deferred until a graphics frame exists. The reference startup report is text
mode and is not a valid raster comparison.

## G3 transition frame synchronization

`smoke.py --frame-shots N=name` now saves at the first presented virtual VGA
retrace at or after absolute retrace `N`; the log also reports the current
`wait_for_tick` entry count. This avoids assuming one fixed refresh rate across
mode changes. In the scripted port run, the 10,000 ms and 12,500 ms captures
were retraces 712 and 877 respectively; the latter mode runs at about 70 Hz,
while the 320x240 menu mode before it runs at about 60 Hz.

`refshot.py --video --video-start-ms A --video-stop-ms B` can record a short
DOSBox-X segment around a transition. Screenshots and hotkeys are timestamped
at their trigger, with the F11 lead-in compensated. The original reference
AVI's Level 01 screenshot is an exact match at frame 107; the post-click
reference image is frame 13. The 12.5 s port shot occurs 106 retraces after its
second click (retrace 771), so the frame-aligned port window was also checked
against the adjacent original AVI frames.

**PROVEN:** at the transition, the port image alternates substantially between
the two display pages: even retraces in 870..885 are 12.18–12.91% from their
nearest original frames, while odd retraces are 21.69–21.93%. The captured
320x200 VGA state uses CRTC offset 28h, byte mode, and an 80-byte plane
stride. At sampled captures, the SEQ01 screen blank was clear, with no panning
or split. Existing oracle fixtures cover game pages 0/1, scan-out, write modes,
and the retrace start latch.

**STRONG remaining lead:** the transition mismatch is not explained by the
scheduled screenshot time alone. The two VGA pages contain different
transition progress at adjacent retraces, which points to game-update/page
flip timing relative to the VGA retrace. No scan-out or palette change was
made; the exact page-update mismatch remains for the V1/game-loop owners to
resolve. G2's `update_game_balls` area was not changed.

**PROVEN blocker:** with the R1 clicks, the game faults as it enters the first
ball update: write to `FF530000` at EIP `0040F4B4` (`update_game_balls`,
`src/u_08585.c:537`). The same fault occurs without screenshot scheduling and
with synchronous IRQ delivery. A delayed second click at 13000 ms allowed a
16000 ms capture, but it remained transitional (20.46% mismatch). No game logic
or `src/` file was changed. The root cause remains unresolved; later active
gameplay cannot be certified until this fault is fixed in the port stack.

## kegg_forged input replay

`--replay` converts `dos.mouse.normalized` and
`dos.keyboard.scancodes` from `portforge-dos-input-script-v1` or
`portforge-replay-v2` JSON into a temporary `KEPORTREPLAY 1` text stream. The
linker wrapper applies events immediately before the corresponding
`wait_for_tick` call, keyed from occurrence zero. Mouse normalized coordinates
scale by two into DOS mouse coordinates and are clamped by the game's current
INT 33h ranges; keyboard values are XT set-1 scancodes.

**STRONG external input evidence:** the read-only
`pmrec_20260723_200732.input.json` stream contains 588 frame occurrences and
588 mouse events. It was armed at 11500 ms after the two R1 clicks. The port
reached occurrence 50/588, entered Level 01's ball update, then faulted at the
write above. This run did not complete and did not reach stable gameplay.

## Virtual hardware paths and gates

The title/menu, early transition, replay, and baseline smoke runs reported zero
distinct `STUB` entry points reached. No unimplemented virtual-hardware warning
was emitted before the gameplay fault. Reached paths and captures are logged by
the host; the smoke summary lists any distinct `STUB` names.

| Gate | Result |
| --- | --- |
| `cmake -S port -B build/port -G Ninja` + `cmake --build build/port` | Pass; no new warnings in changed port files. |
| `build/port/oracle/ke_oracle.exe build/port/oracle` | Pass, 37/37. |
| `python port/tools/check_layouts.py --data` | Pass, 0 mismatches. |
| `python port/tools/gen_asm_stubs.py --check` | Pass, stubs up to date. |
| `python port/tools/smoke.py` | Pass, exit 0 and 0 stubs reached. |
| R1 gameplay capture and replay | Incomplete; port faults during the first ball update as described above. |

The DAC adjustment in `port/host/present.c` masks expanded DAC channels to
their upper six-bit-aligned value, matching DOSBox-X's `(DAC << 2)` screenshot
conversion while leaving virtual VGA/oracle palette values unchanged.
