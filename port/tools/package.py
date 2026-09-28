"""Build and assemble the Windows SDL3 release directory without game data files.

    python port/tools/package.py
    python port/tools/package.py --build-dir build/port-release --dist-dir dist
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SDL_DEFAULT = Path("C:/tools/sdl3-3.4.16-i686")
SYSTEM_DLLS = {
    "advapi32.dll", "bcrypt.dll", "cfgmgr32.dll", "combase.dll", "comdlg32.dll",
    "crypt32.dll", "d3d11.dll", "d3d12.dll", "dinput8.dll", "dwmapi.dll", "dxgi.dll",
    "gdi32.dll", "hid.dll", "imm32.dll", "kernel32.dll", "mmdevapi.dll", "msvcrt.dll",
    "ntdll.dll", "ole32.dll", "oleaut32.dll", "powrprof.dll", "setupapi.dll", "shell32.dll",
    "shcore.dll", "ucrtbase.dll", "user32.dll", "version.dll", "winmm.dll",
    "ws2_32.dll", "xinput1_4.dll", "xinput9_1_0.dll",
}

README = """Krypton Egg SDL3
================

Run ke_sdl3.exe. Put the original game data files beside the executable or in an
assets subfolder. The program also searches ./assets when launched from a working
directory that contains it. You may point to another folder with --asset-dir PATH
or the asset_dir setting in ke_sdl3.ini. KE.EXE is not needed at runtime.

Required original data files
----------------------------
KE_ALL.PAL, KE_BRICK.BOB, KE_DIGIT.BOB, KE_END.DIG, KE_FILL.BOB, KE_FONT.BOB,
KE_GO.DIG, KE_INFOS.DIG, KE_LDCWC.TAB, KE_LVL.DIG, KE_MAIN.DIG, KE_MENU.BOB,
KE_MENU.DIG, KE_MENU.GIF, KE_MONST.BOB, KE_MONST.GIF, KE_NMY.BOB, KE_ORDER.GIF,
KE_PAUSE.DIG, KE_RACK.BOB, KE_SCORE.DIG, KE_SCORE.GIF, KE_SPELL.BOB, KE_TIT.DIG,
KE_TIT.GIF.

KE_PUB.DIG and KE_PUB1.GIF, KE_PUB2.GIF, KE_PUB3.GIF are optional publisher pages.
KE_SCORE.LST is optional initial high-score data; saved scores go in the user profile.

Controls
--------
The game receives the original keyboard controls. In the default faithful mouse mode,
click in the window to capture it; press Escape or switch focus to release it. Native
mouse mode leaves the desktop cursor visible and maps its position through the displayed
game viewport. Focus loss releases held buttons. F11 or Alt+Enter toggles fullscreen.
Enable SDL gamepad mapping with `joystick=on` or `[input] joystick = true` in
`krypton-egg.ini`. The left stick feeds the emulated 201h axes, South/A is button 1,
and East/B is button 2. Leave the stick centered during startup detection/calibration.
Detection, calibration, and T15 direction/fire bits are verified without physical hardware;
the frozen game does not poll its joystick hook during play, so racket control remains on
the original keyboard/mouse path.

Options
-------
Use --scale N (1..8), --fullscreen/--windowed, --integer-scaling/
--no-integer-scaling, --aspect 4:3|square, --audio on|off, --joystick on|off,
--mouse-mode faithful|native, --volume N (0..100), and --asset-dir PATH. On first run,
the program creates `%APPDATA%/Krypton Egg/krypton-egg.ini` with documented defaults.
The file includes video, Sound Blaster audio, volume, joystick, mouse mode, asset path,
IRQ host mode, Windows compatibility, and log level. `KE_CONFIG_DIR` selects another
config directory. The legacy `ke_sdl3.ini` beside the executable is still read and has
priority over the per-user file. Precedence is compiled defaults, per-user config,
legacy INI, `KE_*` environment variables, then command-line options. Native absolute
mouse input bypasses mickey acceleration and leaves the original smoothing active;
`mouse_sensitivity` is reserved and ignored. Sound Blaster audio is enabled by default;
set sound_blaster=false, --audio off, KE_SB=0,
or KE_AUDIO=off to disable it. Set KE_AUDIO_DUMP=path.wav to capture the samples
submitted to SDL as unsigned 8-bit mono WAV; path.wav.dsp.log records DSP commands,
effective rates, and DMA block offsets. Rate changes are listed because WAV has one
header sample rate.

The original game files are not included. SDL3 is distributed under its license
in LICENSE-SDL3.txt.
"""


def run(command: list[str], cwd: Path) -> None:
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, cwd=cwd, check=True)


def inside(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def dependencies(objdump: str, binary: Path) -> list[str]:
    result = subprocess.run([objdump, "-p", str(binary)], check=True, capture_output=True,
                            text=True, errors="replace")
    return re.findall(r"^\s*DLL Name:\s*(\S+)\s*$", result.stdout, re.MULTILINE | re.IGNORECASE)


def locate_dll(name: str, search_dirs: list[Path]) -> Path | None:
    for directory in search_dirs:
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build/port-release"))
    parser.add_argument("--dist-dir", type=Path, default=Path("dist"))
    parser.add_argument("--sdl3-dir", type=Path, default=SDL_DEFAULT)
    args = parser.parse_args()

    build_dir = (ROOT / args.build_dir).resolve() if not args.build_dir.is_absolute() else args.build_dir.resolve()
    dist_root = (ROOT / args.dist_dir).resolve() if not args.dist_dir.is_absolute() else args.dist_dir.resolve()
    sdl3_dir = args.sdl3_dir.resolve()
    package_dir = dist_root / "KryptonEgg-sdl3"
    if dist_root == ROOT or not inside(dist_root, ROOT) or package_dir.parent != dist_root:
        parser.error("dist directory must be inside the repository worktree")
    cmake = shutil.which("cmake")
    objdump = shutil.which("objdump")
    if not cmake or not objdump:
        parser.error("cmake, Ninja and objdump must be on PATH (add MSYS2 mingw32/bin first)")
    if not (sdl3_dir / "bin" / "SDL3.dll").is_file():
        parser.error(f"SDL3.dll not found under {sdl3_dir}; build/install SDL3 i686 first")

    run([cmake, "-S", str(ROOT / "port"), "-B", str(build_dir), "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", f"-DKE_SDL3_DIR={sdl3_dir.as_posix()}"], ROOT)
    run([cmake, "--build", str(build_dir), "--config", "Release"], ROOT)

    executable = build_dir / "ke_sdl3.exe"
    sdl_dll = sdl3_dir / "bin" / "SDL3.dll"
    if not executable.is_file():
        parser.error(f"built executable not found: {executable}")
    license_candidates = [sdl3_dir / "share" / "licenses" / "SDL3" / "LICENSE.txt",
                          sdl3_dir / "share" / "licenses" / "SDL3" / "LICENSE"]
    license_file = next((p for p in license_candidates if p.is_file()), None)
    if license_file is None:
        parser.error(f"SDL3 license was not found under {sdl3_dir}/share/licenses/SDL3")

    path_dirs = [Path(p) for p in os.environ.get("PATH", "").split(os.pathsep) if p]
    mingw_dir = Path(shutil.which("gcc")).resolve().parent if shutil.which("gcc") else None
    if mingw_dir:
        path_dirs.insert(0, mingw_dir)
    system32 = Path(os.environ.get("WINDIR", "C:/Windows")) / "System32"
    pending = [executable, sdl_dll]
    copied: dict[str, Path] = {}
    while pending:
        binary = pending.pop()
        for name in dependencies(objdump, binary):
            key = name.lower()
            if key == "sdl3.dll" or key in SYSTEM_DLLS or key.startswith(("api-ms-win-", "ext-ms-win-")):
                continue
            if (system32 / name).is_file():
                continue
            if key in copied:
                continue
            runtime = locate_dll(name, path_dirs)
            if runtime is None:
                parser.error(f"cannot locate non-system DLL dependency {name} required by {binary}")
            copied[key] = runtime
            pending.append(runtime)

    dist_root.mkdir(parents=True, exist_ok=True)
    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir()
    shutil.copy2(executable, package_dir / "ke_sdl3.exe")
    shutil.copy2(sdl_dll, package_dir / "SDL3.dll")
    shutil.copy2(license_file, package_dir / "LICENSE-SDL3.txt")
    (package_dir / "README-port.txt").write_text(README, encoding="utf-8", newline="\r\n")
    for name, source in sorted(copied.items()):
        shutil.copy2(source, package_dir / source.name)
    print(f"Packaged {package_dir}")
    print("Bundled mingw runtime DLLs: " + (", ".join(p.name for p in copied.values()) or "none"))
    print("Original game data was not copied.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode or 1)
