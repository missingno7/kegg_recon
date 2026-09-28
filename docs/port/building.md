# Building and running the SDL3 port

## Prerequisites

- Windows 10 or newer and MSYS2's 32-bit MinGW environment (i686-w64-mingw32 GCC).
- CMake and Ninja on `PATH`.
- SDL 3.4.16 built for i686 and installed at `C:/tools/sdl3-3.4.16-i686`.
- Python 3.10 or newer. The port tools use only the Python standard library.

The game stores pointers in `int`, so the SDL executable must be 32-bit. In an MSYS2
shell, `gcc -dumpmachine` should report `i686-w64-mingw32`. Put the 32-bit tools first:

```sh
export PATH=/c/msys64/mingw32/bin:$PATH
```

### Build SDL3 i686

Use the official SDL 3.4.16 source release. Its archive SHA-256 is
`7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68`.
Verify the downloaded archive before extracting it:

```sh
curl -L -o SDL3-3.4.16.tar.gz \
  https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
sha256sum SDL3-3.4.16.tar.gz
tar -xf SDL3-3.4.16.tar.gz
cmake -S SDL3-3.4.16 -B build/sdl3-i686 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=C:/tools/sdl3-3.4.16-i686 \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
cmake --build build/sdl3-i686
cmake --install build/sdl3-i686
```

The result provides `bin/SDL3.dll`, SDL3 headers, and the CMake package imported by
this port. The checksum identifies the source archive used for the pinned local SDL3
install; do not replace it with an SDL build for another architecture or version.

## Build the game

From the repository root, with the MSYS2 MinGW32 environment active:

```sh
cmake -S port -B build/port -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/port
```

The historical game units stay at `-O0`. Their globals are polled by legacy code and
their data layout is checked against the original image. The host port files use the
flags in `port/CMakeLists.txt`.

## Run

Copy the original game data files to the executable directory or its `assets/`
subdirectory. When launched from the repository root, the executable also finds
`./assets`. The runtime data set is:

```text
KE_ALL.PAL KE_BRICK.BOB KE_DIGIT.BOB KE_END.DIG KE_FILL.BOB KE_FONT.BOB KE_GO.DIG
KE_INFOS.DIG KE_LDCWC.TAB KE_LVL.DIG KE_MAIN.DIG KE_MENU.BOB KE_MENU.DIG KE_MENU.GIF
KE_MONST.BOB KE_MONST.GIF KE_NMY.BOB KE_ORDER.GIF KE_PAUSE.DIG KE_RACK.BOB KE_SCORE.DIG
KE_SCORE.GIF KE_SPELL.BOB KE_TIT.DIG KE_TIT.GIF
```

`KE_PUB.DIG` and `KE_PUB1.GIF`/`KE_PUB2.GIF`/`KE_PUB3.GIF` are optional publisher
pages. `KE_SCORE.LST` is optional initial high-score data; updated scores are stored in
the user's profile.

`KE.EXE` is not opened by the SDL port. If required files are missing, startup shows a
message box naming the missing files. On first run the port creates a readable
`%APPDATA%/Krypton Egg/krypton-egg.ini` with the current defaults, then loads it on
later runs. `KE_CONFIG_DIR` can point to another config directory, which is useful for
portable runs and automated checks. The older `ke_sdl3.ini` beside the executable is
still read and overrides the per-user file. Precedence is compiled defaults, the
per-user file, legacy `ke_sdl3.ini`, `KE_*` environment variables, then command-line
options:

```sh
build/port/ke_sdl3.exe [DATA_DIR] [--scale 1..8] [--fullscreen|--windowed]
  [--integer-scaling|--no-integer-scaling] [--aspect 4:3|square]
  [--audio on|off] [--joystick on|off] [--mouse-mode faithful|native]
  [--volume 0..100] [--asset-dir DIR]
```

The generated file has these defaults:

```ini
[video]
fullscreen = false
scale = 3
aspect = true
integer_scale = true

[audio]
sound_blaster = true
volume = 100

[input]
# SDL gamepad maps to 201h; the frozen game does not poll its joystick hook during play.
joystick = false
mouse_mode = faithful
mouse_sensitivity = 1.0  # reserved; absolute native mode ignores this

[paths]
asset_dir =  # blank selects the usual executable/assets search

[system]
irq = async
windows_host = false

[debug]
log_level = 2
```

Sound Blaster audio is on by default at 220h/IRQ7/DMA1. Disable it with
`sound_blaster=false` in the new config, `audio=off` in the legacy INI, `--audio off`,
`KE_SB=0`, or `KE_AUDIO=off`. `mouse_mode=faithful` keeps the existing captured,
relative DOS mouse emulation. `mouse_mode=native` keeps the desktop cursor visible and
maps its position through the displayed game viewport into the game's absolute INT 33h
position; clicks outside the viewport clamp to its nearest edge. The original game's
4/8-sample smoothing remains active. `mouse_sensitivity` is reserved and has no effect
on absolute native input. `KE_MOUSE_MODE=faithful|native` overrides the file. To record the
unsigned 8-bit mono samples submitted to SDL, set `KE_AUDIO_DUMP=path.wav`; the
port writes DSP commands, effective sample rates, and DMA block offsets to
`path.wav.dsp.log`. Since DSP time constants change the rate during play, the log
records each rate alongside the WAV's single header rate.

F11 and Alt+Enter toggle fullscreen. Click the window to capture the mouse; Escape or
focus loss releases it. The original keyboard controls are sent to the game. Enable joystick
support with `--joystick on`, `KE_JOY=1`, `joystick = true` under `[input]` in
`krypton-egg.ini`, or `joystick=on` in legacy `ke_sdl3.ini`; the first connected SDL gamepad
is exposed through the emulated 201h gameport. The left stick feeds
the X/Y gameport axes, South/A is joystick button 1, and East/B is button 2. Keep the stick
centered during startup detection so the game's neutral calibration can run. Detection,
calibration, and the T15 direction/fire bits are verified without physical hardware. The
frozen game currently does not poll its joystick hook in the main gameplay loop, so racket
movement still uses the original keyboard/mouse path; changing that would alter frozen game
behavior.

The SDL virtual joystick integration test runs without physical controller hardware:

```sh
build/port/oracle/ke_oracle.exe build/port/oracle joystick
```

For deterministic in-game input, `lockstep.py` accepts joystick snapshots alongside mouse
and keyboard events. Each `F:J:X:Y:B` uses frame `F`, post-dead-zone X/Y values from -1000 to
1000, and a four-bit South/East/West/North button mask:

```sh
python port/tools/lockstep.py --frames 500 --joystick --click-every 100:200:150:82:400 \
  --event 130:J:-900:0:0 --event 150:J:900:0:1 --event 155:J:900:0:0
```

The runner applies identical 201h inputs to the original code and port, compares each
frame's VGA and named game data, and writes the full dumps under `build/port/lockstep/`.

## Package

The package script builds a Release configuration and assembles
`dist/KryptonEgg-sdl3/` with the executable, SDL3, non-system MinGW runtime DLLs,
SDL3's license, and a run guide. It does not bundle game data. The historical game
units are still compiled with `-O0`; changing their optimization requires the complete
layout, oracle, smoke, and timer gates in `docs/port/workpackages.md`.

```sh
python port/tools/package.py
```

Use `--sdl3-dir`, `--build-dir`, or `--dist-dir` to override the default paths. The
script expects `cmake`, Ninja, and MinGW `objdump` on `PATH` so it can discover and
include any non-system DLL dependencies of the executable and SDL3.

## Note: packaged (MSIX) launchers virtualize AppData
If the game is started from inside a packaged Windows app (for example an app-store Python, or an agent running in
the Claude desktop app), Windows redirects its writes under `%APPDATA%` into that package's private folder
(`%LOCALAPPDATA%\Packages\<package>\LocalCache\Roaming\...`). The generated `krypton-egg.ini` and the high-score file
then exist only inside that package's view and are invisible to editors outside it. Launch the game normally
(Explorer, a regular terminal) once to create the real `%APPDATA%\Krypton Egg\krypton-egg.ini`, or set
`KE_CONFIG_DIR` to an explicit directory.
