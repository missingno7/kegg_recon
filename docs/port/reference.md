# DOSBox-X screen references

The captures in [reference-captures](reference-captures/) come from the original
`assets/KE.EXE` and `assets/DOS4GW.EXE`, launched from a temporary writable copy
of `assets/`. `refshot.py` checks the pinned DOSBox-X executable through
`tools/dosbox.py`, uses DOSBox-X's native F11+P PNG capture, and removes the
scratch copy after the run. Key times use smoke's `ms:hex-scan-code` syntax;
they are scheduled with DOSBox-X `ADDKEY`. Shot/click times are measured from
the DOSBox-X window starting; the AUTOEXEC launches KE.EXE immediately after
startup. Host startup adds a small timing offset.

## What the original shows

**PROVEN by the linked DOSBox-X captures:**

- At 1200 ms, the VGA text report lists the detected 80486, DOS 5, VGA, XMS,
  mouse, Sound Blaster Pro at 220h / IRQ 7 / DMA 1, DSP 4.05, and VBL/key
  managers. See [startup-report.png](reference-captures/startup-report.png).
- Space at 1500 ms passes the startup wait. The title image fades in by 3500 ms;
  the stable main menu at 6500 ms is a blue rippled field, metallic "KRYPTON
  EGG" lettering, a central egg, and the cyan "THE ULTIMATE BREAKOUT" caption.
  See [startup-title.png](reference-captures/startup-title.png) and
  [main-menu.png](reference-captures/main-menu.png). The captured artwork has
  no horizontal yellow striping.
- **PROVEN by source and captures:** the title menu has no keyboard start key.
  Its first mouse hotspot uses strict bounds x>91, x<222, y>70, y<93; a click
  at 150,82 selects `run_gameplay_session`. A
  second click advances the level-ready screen. Captures show the transition to
  LEVEL 01 and the first active frame with the blue checkerboard, brick layout,
  score HUD, racket, and ball: [game-entry.png](reference-captures/game-entry.png),
  [level-01.png](reference-captures/level-01.png),
  [first-gameplay.png](reference-captures/first-gameplay.png).

## Port comparison

The paired port frame is [port-menu.bmp](reference-captures/port-menu.bmp),
captured by `smoke.py --ms 7000 --shot ...` at 6500 ms. It shows a blue/black
texture with yellow horizontal striping, unlike the original title/menu artwork.
`framediff.py` normalizes both images to 320x200 with nearest-neighbor sampling;
auto mode handles raw 320x200, 4:3 aspect-corrected captures, and 320x400
scanouts. At threshold 0, this pair differs at **99.57% of pixels** (mean
absolute channel error 50). The [diff image](reference-captures/port-menu-vs-original.png)
marks every differing pixel red/yellow. This is a proven visual mismatch. The
port smoke log at shutdown places the game thread in `wait_menu_select`, so
this capture may still be at the startup wait rather than the later menu loop;
the screen state should be checked alongside the pixel diff.

**LIMITATION:** `refshot.py` measures from DOSBox-X window startup; the AUTOEXEC
starts KE.EXE immediately afterward, with a small uncalibrated offset. The
port screenshot is the raw VGA scanout, while DOSBox-X captures the displayed
surface. `framediff.py` rescales both complete frames to 320x200, so its
percentage is a screen-level baseline rather than a synchronized frame oracle.

The 720x400 text startup report is evidence of the original startup state, but
is not a VGA raster comparison input. Graphics captures are 640x400 or 640x480;
the current port `smoke.py --shot` frame is the raw 320x400 scanout.

## Reproduce

```powershell
python port/tools/refshot.py --keys "1500:39" `
  --shots "1200=startup-report,3500=startup-title,6500=main-menu" `
  --out docs/port/reference-captures

python port/tools/refshot.py --keys "1500:39" `
  --clicks "8000:150:82,11000:150:82" `
  --shots "10000=game-entry,12500=level-01,16000=first-gameplay" `
  --out docs/port/reference-captures

python port/tools/smoke.py --ms 7000 --shot build/port/port-menu.bmp
python port/tools/framediff.py `
  --reference docs/port/reference-captures/main-menu.png `
  --port build/port/port-menu.bmp `
  --diff docs/port/reference-captures/port-menu-vs-original.png
```
