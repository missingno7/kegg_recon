# Building, installing and testing the Android version

Design: `docs/android/architecture.md`. The app contains no game data; the user imports
their own copy on first run.

## Prerequisites (Windows host, as used for this branch)

| tool | version / location |
|---|---|
| Android SDK | `%LOCALAPPDATA%\Android\Sdk`: platform 35, build-tools 35, CMake 3.22.1, platform-tools, emulator |
| NDK | 28.2.13676358 (27.0.12077973 also works for C, but the build pins 28.2) |
| JDK | 17 or 21 for Gradle/AGP (`C:\Program Files\Java\jdk-21`); JDK 25 is too new for Gradle 8.12 |
| Gradle / AGP | 8.12 (wrapper, `android/gradlew.bat`) / 8.7.3 |
| SDL | 3.4.16 source tree, `C:\tools\download\sdl3\SDL3-3.4.16` (native build and its Java glue) |
| Python | 3.10+ on `PATH` (the ILP32 world build step; standard library only) |
| game image | `assets/KE.EXE` and `manifest.json`: the build reproduces the original CONST block from it, exactly as the Windows build does (never packaged separately) |

Paths can be overridden in `android/gradle.properties`, `~/.gradle/gradle.properties` or with
`-P`: `sdl3.dir=...`, `ke.python=...` (Python executable for CMake), `ke.abis=arm64-v8a,x86_64`.
`android/local.properties` holds `sdk.dir` (not committed).

## Build

```
cd android
set JAVA_HOME=C:\Program Files\Java\jdk-21
gradlew.bat assembleDebug                         (both ABIs; -Pke.abis=x86_64 for one)
gradlew.bat assembleRelease                       (signed with the debug key; replace for a store)
```
Output: `android/app/build/outputs/apk/debug/app-debug.apk` (arm64-v8a + x86_64,
`libSDL3.so`, `libmain.so`, `libkegame.so`). What happens (CMake, `android/app/jni/CMakeLists.txt`):
1. SDL3 is built from source (`libSDL3.so`, static libc++).
2. Every game unit, asm translation, the generated TASM `_DATA` and the world's own printf are
   built by `port/android/tools/ilp32_world.py` (ILP32 code generation, Windows-identical data
   layout) into `.cxx/.../<abi>/ilp32/*.o`; `check_ilp32_boundary.py` then fails the build if the
   world calls anything outside its 32-bit-scalar boundary.
3. `libkegame.so` = those objects + the LP64 virtual PC/host; `libmain.so` = the loader.

Layout proof after a build (compares with the Windows objects, so build `build/port` first):
```
python port/android/tools/check_ilp32_layout.py --abi arm64-v8a --ndk-bin <ndk>/toolchains/llvm/prebuilt/windows-x86_64/bin ^
       --android-objs android/app/.cxx/Debug/<id>/arm64-v8a/ilp32
```
Expected: `609 data symbols compared with the Windows layout, 0 differ` and `0 mismatches`.

## Install and first run

```
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```
Start "Krypton Egg". The first screen asks for the game files: **Choose folder** (the folder
with `KE_TIT.GIF`, `KE_MENU.GIF`, `KE_LDCWC.TAB`, ...; subfolders are searched, names may be
in any case) or **Choose ZIP**. Android does not allow picking the root of `Download`; pick a
folder inside it. The 25 required files (and the optional `KE_PUB*`, `KE_SCORE.LST`) are
copied to the app's private `files/game`; later starts go straight to the game. High scores
and `krypton-egg.ini` live in `files/Krypton Egg/`; the log is `files/Krypton Egg/ke_sdl3.log`
and logcat tag `KryptonEgg`. To re-import, clear the app's storage.

For the emulator, copy the data from the PC first, e.g.
```
adb shell mkdir -p /sdcard/Download/KryptonEgg
adb push assets/KE_*.* /sdcard/Download/KryptonEgg/      (MSYS: export MSYS_NO_PATHCONV=1)
```

## Controls

Landscape, full screen. Gameplay: the racket follows your finger; **L** (left pillarbox) =
left mouse button (launch the ball; boss: single shot), **R** (right) = right button (gun;
boss: spread shot), **II** = the game's pause (P). Menus and other screens: tap = point and
left-click. The keyboard button (top right, outside gameplay) opens the soft keyboard for the
restart code (Space, then the code, Enter) and the high-score name. Android Back: pause/resume
during play, otherwise Esc (skip a page; on the main menu it quits). A mouse, keyboard or
gamepad connected to the device works as on the desktop port.

## 64-bit lockstep (development)

Compares the Android build with the Windows i686 port on the deterministic lockstep machine
(needs the Windows port built in `build/port` and a device/emulator):
```
gradlew.bat assembleDebug -Pke.lockstep=1 -Pke.abis=x86_64
python port/android/tools/lockstep64.py --frames 1100 --click-every 100:60:150:82:400
# arm64 code: install an arm64-only debug build and run the runner inside the app process
gradlew.bat assembleDebug -Pke.lockstep=1 -Pke.abis=arm64-v8a  &  adb install -r ...  &  adb root
python port/android/tools/lockstep64.py --in-app --abi arm64-v8a --frames 6000 --sound ^
       --click-every 100:60:150:82:400 --replay <pmrec json> --replay-offset 400
```
The report is `build/port/lockstep64*/report.txt`. The in-app mode needs the game data
imported (it reads `files/game`) and root (`adb root` on a userdebug/google_apis emulator).

## Test notes (this branch, emulator `KryptonEggApi36`: android-36 google_apis x86_64, Pixel 5 profile)

Screenshots: `build/android_shots/*.png` (`adb exec-out screencap -p`, not committed).

Verified (PROVEN on the emulator):
- Import: folder picker on `Download/KryptonEgg` with the files in a subfolder `DATA`, two of
  them renamed to lower/mixed case (`ke_tit.gif`, `Ke_Menu.Dig`): all 26 files copied with
  canonical names, validation passed, game started (`01_import` .. `05`).
- Loader: `libkegame.so` mapped at 0x0E000000, heap 0x11200000, stack 0x15400000; 268
  32-bit data pointers self-relocated; DOS memory E0000h..10FFFFh mapped.
- Startup report identical to the Windows port's, intro (320x400), hall of fame, main menu
  (320x240), level 1 (320x200) with the overlay (`06`..`10`); racket moved by a swipe, ball
  launched with L (`11`, `12`); pause button and Back pause/resume (`13`, `14`); Home + 10 s +
  return: log shows 0 ms of virtual time elapsed (`15`, `16`); Esc (hardware key) gives up
  rounds back to the menu, touch mode switches to menu controls (`17`); keyboard button,
  restart code typed `12AB` via the soft keyboard (`18`..`21`); menu tap on INFOS, Back to the
  next page (`41`, `43`); Back on the main menu quits (game exit code 0, activity finished).
- The same APK's arm64-v8a libraries run under the emulator's ARM translation
  (`libndk_translation`): title, menu, gameplay with touch (`30`, `31`).
- 64-bit lockstep against the Windows port: NO DIVERGENCE in 1100 frames (clicks) and in 6000
  frames (clicks + pmrec mouse replay + Sound Blaster, first DSP payloads equal) for both the
  x86_64 build (native) and the arm64-v8a build (translated) - every frame's clock, PIC, PIT,
  VGA registers, DAC and planes, heap, BIOS data area, DSP/DMA state and 56 KB of game globals.
- Windows: all `port/tools/mergepkg.py` gates pass after every phase.

Not verified without a real arm64 device:
- Real-time behaviour on an arm64 CPU. Under the emulator's ARM translation the game's startup
  VBL calibration fails ("VBL compatibility" line missing: the translated code is too slow for
  the calibration window), so it runs without its vsync-locked timer; on the native x86_64
  build and on Windows the calibration succeeds (`Synchro $41BD`). Deterministic behaviour of
  the arm64 code itself is proven by the lockstep run above.
- Audio output quality (the headless emulator has no audible output; the virtual SB stream
  opens at 44.1 kHz, one "IRQ7 did not re-arm within 5 ms" warning was logged under emulator load).
- 16 KB page-size devices (linked with max-page-size 16384, not run on such a kernel).
- Performance/battery; high-score name entry (needs a qualifying score; same text path as the
  restart code); gamepads/physical keyboards on Android.
