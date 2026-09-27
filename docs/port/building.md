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
message box naming the missing files. The optional `ke_sdl3.ini` beside the executable
can set `asset_dir`, `window_scale`, `fullscreen`, `integer_scaling`, `aspect` (`4:3`
or `square`), `audio`, `joystick`, and `volume` (0..100). Command-line options override
INI and environment values:

```sh
build/port/ke_sdl3.exe [DATA_DIR] [--scale 1..8] [--fullscreen|--windowed]
  [--integer-scaling|--no-integer-scaling] [--aspect 4:3|square]
  [--audio on|off] [--joystick on|off] [--volume 0..100] [--asset-dir DIR]
```

F11 and Alt+Enter toggle fullscreen. Click the window to capture the mouse; Escape or
focus loss releases it. The original keyboard controls are sent to the game, and an SDL
gamepad is exposed as a virtual gameport when joystick support is enabled.

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
