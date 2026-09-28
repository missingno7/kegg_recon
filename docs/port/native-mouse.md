# NM1 native mouse seam analysis

## Existing path (PROVEN by source inspection)

- `port/host/input.c` currently turns captured SDL relative motion into
  `vmouse_motion_at(dx, dy, timestamp)`. `port/vhw/mouse.c` applies the Microsoft
  driver's sensitivity and time-based double-speed threshold, accumulates mickey
  counters for INT 33h function 0Bh, then advances/clamps its absolute position.
- `src/t17_mouse.c:update_mouse()` reads position and buttons with INT 33h function
  03 on each game update. It halves CX/DX, shifts its historical sample arrays, and
  computes its original 4-sample and 8-sample averages. This is the game-facing
  consumer and must remain unchanged. The inspected function does not call INT 33h
  function 0Bh, so native mode should set absolute position rather than synthesize
  mickeys.
- The historical mouse setup uses a 2:1 driver-coordinate scale in
  `t17_mouse.c` (`MOUSE_COORDINATE_SCALE = 2`). Setting the virtual driver's
  absolute position directly therefore takes logical VGA coordinates multiplied
  by two; the normal function 03 read and original smoothing remain in use.
- `port/host/present.c:ke_present_frame()` is the one place that knows the actual
  scan-out size, SDL renderer output size, aspect policy, integer-scaling policy,
  and centered destination rectangle. SDL mouse event positions are in window
  coordinates, while the renderer may target a different pixel size on high-DPI
  displays. Input must use the same destination calculation and window-to-output
  ratio as presentation.
- Faithful input captures the window on left-button down. Focus loss already
  releases relative mode and clears virtual mouse buttons.
- `port/host/config.c` currently reads the legacy `ke_sdl3.ini` beside the exe,
  then applies environment variables, then command-line arguments. Config loading
  happens before SDL initialization because audio and gamepad flags affect
  `SDL_Init`.

## Smallest implementation seam

Keep all game logic and `src/t17_mouse.c` unchanged. Add a virtual-mouse absolute
position setter that only updates/clamps the driver's position under its existing
lock; it must not alter mickey counters, acceleration timestamps, sensitivity, or
button state. Add a pure viewport helper used by both presentation and native input
to calculate the centered destination and map SDL window coordinates through the
renderer output scale to logical VGA coordinates. Clamp positions outside the
displayed destination to its nearest game edge. Add a small host mouse backend
that uses this shared transform and the absolute setter. `input.c` should only
dispatch absolute motion/button coordinates to it when `mouse_mode=native`; the
faithful branch remains byte-for-byte the existing relative path and retains its
capture behavior.

Use the writable per-user roaming configuration directory (`%APPDATA%\Krypton
Egg\krypton-egg.ini`) with a test-only/automation-friendly `KE_CONFIG_DIR` directory
override. Preserve `ke_sdl3.ini` beside the executable as a higher-priority legacy
file. The intended precedence is compiled defaults, generated/user config, legacy
exe-local INI, `KE_*` environment overrides, then CLI options. This keeps legacy
settings and existing scripts effective while making the new file the default
user-facing config. Native absolute input leaves function 0Bh counters untouched;
the historical game reads function 03, so its existing smoothing still filters
the absolute position once per game update.

## Files likely to cross package boundaries

`port/host/input.c` is also a joystick/input package seam; keep its change limited
to the mouse dispatch and capture conditions. `port/host/present.c` is presentation
owned; minimize its change to using the shared viewport helper and exposing the
current mapping. If package ownership conflicts arise during merge, the compatible
hook signatures will be `ke_present_map_mouse(...)` and
`vmouse_set_absolute_position(...)` declared in port headers.
