#!/usr/bin/env python3
"""Capture timed reference screenshots from the pinned DOSBox-X game run.

    python port/tools/refshot.py [--keys "1500:39"]
        [--shots "1200=report,3500=title,6500=menu"]
        [--clicks "8000:150:82"] [--out docs/port/reference-captures]

The key syntax matches smoke.py: milliseconds from DOSBox-X window startup,
followed by a hexadecimal XT set-1 make code. The private AUTOEXEC launches
KE.EXE immediately after DOSBox-X starts and uses DOSBox-X's ADDKEY command to
queue guest keys; its F11+P host shortcut writes each PNG capture. A private
copy of assets/ is mounted so the original game files remain untouched.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets"

# XT set-1 make codes accepted by the DOSBox-X ADDKEY command. Extended keys
# are accepted where DOSBox-X names them; add additional codes only with a
# known mapping from a DOSBox-X keyboard event.
_KEY_NAMES = {
    0x01: "escape", 0x02: "1", 0x03: "2", 0x04: "3", 0x05: "4",
    0x06: "5", 0x07: "6", 0x08: "7", 0x09: "8", 0x0A: "9",
    0x0B: "0", 0x0C: "-", 0x0D: "=", 0x0E: "bs", 0x0F: "tab",
    0x10: "q", 0x11: "w", 0x12: "e", 0x13: "r", 0x14: "t",
    0x15: "y", 0x16: "u", 0x17: "i", 0x18: "o", 0x19: "p",
    0x1A: "[", 0x1B: "]", 0x1C: "enter", 0x1E: "a", 0x1F: "s",
    0x20: "d", 0x21: "f", 0x22: "g", 0x23: "h", 0x24: "j",
    0x25: "k", 0x26: "l", 0x27: ";", 0x28: "'", 0x29: "`",
    0x2B: "\\", 0x2C: "z", 0x2D: "x", 0x2E: "c", 0x2F: "v",
    0x30: "b", 0x31: "n", 0x32: "m", 0x33: ",", 0x34: ".",
    0x35: "/", 0x39: "space",
    0x47: "home", 0x48: "up", 0x49: "pgup", 0x4B: "left",
    0x4D: "right", 0x4F: "end", 0x50: "down", 0x51: "pgdown",
    0x52: "ins", 0x53: "del",
}
for _i in range(10):
    _KEY_NAMES[0x3B + _i] = f"f{_i + 1}"
_KEY_NAMES[0x57] = "f11"
_KEY_NAMES[0x58] = "f12"
_KEY_NAMES.update({0x100 | 0x47: "home", 0x100 | 0x48: "up",
                    0x100 | 0x49: "pgup", 0x100 | 0x4B: "left",
                    0x100 | 0x4D: "right", 0x100 | 0x4F: "end",
                    0x100 | 0x50: "down", 0x100 | 0x51: "pgdown",
                    0x100 | 0x52: "ins", 0x100 | 0x53: "del"})


def parse_keys(spec: str) -> list[tuple[int, int]]:
    result = []
    for entry in filter(None, (part.strip() for part in spec.split(","))):
        try:
            ms_text, code_text = entry.split(":", 1)
            ms, code = int(ms_text, 10), int(code_text, 16)
        except ValueError as exc:
            raise ValueError(f"invalid key item {entry!r}; expected ms:hex-scan") from exc
        if ms < 0 or not 0 <= code <= 0x1FF:
            raise ValueError(f"key item out of range: {entry!r}")
        if code not in _KEY_NAMES:
            raise ValueError(f"XT scan code {code:03X} has no DOSBox-X ADDKEY name")
        result.append((ms, code))
    if result != sorted(result):
        raise ValueError("key times must be nondecreasing")
    return result


def parse_shots(spec: str) -> list[tuple[int, str]]:
    result = []
    for entry in filter(None, (part.strip() for part in spec.split(","))):
        try:
            ms_text, name = entry.split("=", 1)
            ms, name = int(ms_text, 10), name.strip()
        except ValueError as exc:
            raise ValueError(f"invalid screenshot item {entry!r}; expected ms=name") from exc
        if ms < 0 or not name or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_" for c in name):
            raise ValueError(f"invalid screenshot item {entry!r}")
        result.append((ms, name))
    if not result:
        raise ValueError("at least one screenshot time is required")
    if result != sorted(result):
        raise ValueError("screenshot times must be nondecreasing")
    if len({name for _, name in result}) != len(result):
        raise ValueError("screenshot names must be unique")
    return result


def hotkey_dispatch_ms(requested_ms: int) -> int:
    """Account for the 60 ms F11 lead-in before DOSBox-X's second hotkey."""
    return max(0, requested_ms - 60)


def parse_clicks(spec: str) -> list[tuple[int, int, int]]:
    result = []
    for entry in filter(None, (part.strip() for part in spec.split(","))):
        try:
            ms_text, x_text, y_text = entry.split(":", 2)
            ms, x, y = int(ms_text, 10), int(x_text, 10), int(y_text, 10)
        except ValueError as exc:
            raise ValueError(f"invalid click item {entry!r}; expected ms:x:y") from exc
        if ms < 0 or not 0 <= x <= 319 or not 0 <= y <= 239:
            raise ValueError(f"click item out of range: {entry!r}")
        result.append((ms, x, y))
    if result != sorted(result):
        raise ValueError("click times must be nondecreasing")
    return result


def addkey_command(keys: list[tuple[int, int]]) -> str | None:
    if not keys:
        return None
    # ADDKEY's pNNN tokens accumulate millisecond delays from when the command
    # runs. The command is issued immediately before KE.EXE in AUTOEXEC.
    words = []
    prev = 0
    for ms, code in keys:
        words.extend((f"p{ms - prev}", _KEY_NAMES[code]))
        prev = ms
    return "ADDKEY " + " ".join(words)


def _dos_mount_path(path: Path) -> str:
    # DOSBox-X's config parser accepts quoted Windows host paths. Quotation
    # marks are rejected to avoid changing the generated autoexec command.
    value = str(path.resolve())
    if '"' in value or "\n" in value or "\r" in value:
        raise ValueError(f"unrepresentable host path: {value!r}")
    return f'"{value}"'


def _find_window(pid: int, marker: str, timeout: float = 20.0) -> int:
    if os.name != "nt":
        raise RuntimeError("timed DOSBox-X screenshot hotkeys currently require Windows")
    user32 = ctypes.windll.user32
    enum_cb_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    user32.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
    user32.GetWindowTextLengthW.argtypes = [ctypes.c_void_p]
    user32.GetWindowTextW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int]
    user32.EnumWindows.argtypes = [enum_cb_type, ctypes.c_void_p]
    found: list[int] = []

    @enum_cb_type
    def visit(hwnd, _param):
        owner = ctypes.c_ulong()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            n = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, buf, n + 1)
            if marker.casefold() in buf.value.casefold():
                found.append(int(hwnd))
                return False
        return True

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        user32.EnumWindows(visit, 0)
        if found:
            return found[0]
        time.sleep(0.1)
    raise RuntimeError(f"DOSBox-X window containing {marker!r} was not created")


def _key_event(vk: int, up: bool = False) -> None:
    flags = 0x0002 if up else 0  # KEYEVENTF_KEYUP
    ctypes.windll.user32.keybd_event(vk, 0, flags, 0)


def _post_key(hwnd: int, vk: int, up: bool = False) -> None:
    user32 = ctypes.windll.user32
    user32.SendMessageTimeoutW.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t,
                                           ctypes.c_ssize_t, ctypes.c_uint, ctypes.c_uint,
                                           ctypes.POINTER(ctypes.c_size_t)]
    scan = {0x7A: 0x57, 0x50: 0x19, 0x11: 0x1D, 0x78: 0x43}.get(vk, 0)
    lparam = 1 | (scan << 16)
    message = 0x0100  # WM_KEYDOWN
    if up:
        message = 0x0101  # WM_KEYUP
        lparam |= 0xC0000000
    result = ctypes.c_size_t()
    if not user32.SendMessageTimeoutW(hwnd, message, vk, lparam, 0x0002, 1000, ctypes.byref(result)):
        raise RuntimeError("DOSBox-X SDL window did not accept a keyboard message")


def _activate_window(hwnd: int) -> bool:
    """Bring the DOSBox SDL window forward so its host shortcut receives input."""
    user32 = ctypes.windll.user32
    user32.GetForegroundWindow.restype = ctypes.c_void_p
    user32.SetForegroundWindow.argtypes = [ctypes.c_void_p]
    if int(user32.GetForegroundWindow() or 0) == hwnd:
        return True
    user32.ShowWindow(hwnd, 5)  # SW_SHOW
    for _attempt in range(4):
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.08)
        if int(user32.GetForegroundWindow() or 0) == hwnd:
            return True
    return False


def _host_chord(hwnd: int, second_vk: int) -> None:
    """Send the Windows DOSBox-X host key (F11) plus a key to its SDL window."""
    user32 = ctypes.windll.user32
    use_global_input = _activate_window(hwnd)
    print(f"refshot input: DOSBox window {'focused' if use_global_input else 'not focused'}; "
          f"method={'keyboard injection' if use_global_input else 'window messages'}")
    send = _key_event if use_global_input else lambda vk, up=False: _post_key(hwnd, vk, up)
    send(0x7A)  # VK_F11, Windows DOSBox-X default host key
    time.sleep(0.06)
    send(second_vk)
    time.sleep(0.04)
    send(second_vk, True)
    time.sleep(0.04)
    send(0x7A, True)


def _exit_dosbox(hwnd: int) -> None:
    user32 = ctypes.windll.user32
    use_global_input = _activate_window(hwnd)
    send = _key_event if use_global_input else lambda vk, up=False: _post_key(hwnd, vk, up)
    send(0x11)  # VK_CONTROL
    send(0x78)  # VK_F9
    time.sleep(0.05)
    send(0x78, True)
    send(0x11, True)


def _click(hwnd: int, x: int, y: int) -> tuple[int, int]:
    """Click a VGA coordinate through the host mouse input path."""
    user32 = ctypes.windll.user32
    rect = (ctypes.c_long * 4)()
    user32.GetClientRect.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
        raise RuntimeError("could not query the DOSBox-X client size for a mouse click")
    width, height = rect[2] - rect[0], rect[3] - rect[1]
    px = max(0, min(width - 1, round((x + 0.5) * width / 320)))
    py = max(0, min(height - 1, round((y + 0.5) * height / 200)))
    class Point(ctypes.Structure):
        _fields_ = (("x", ctypes.c_long), ("y", ctypes.c_long))
    point = Point(px, py)
    user32.ClientToScreen.argtypes = [ctypes.c_void_p, ctypes.POINTER(Point)]
    if not user32.ClientToScreen(hwnd, ctypes.byref(point)):
        raise RuntimeError("could not map the DOSBox-X client click to desktop coordinates")
    user32.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
    user32.mouse_event.argtypes = [ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong,
                                   ctypes.c_ulong, ctypes.c_size_t]
    if not user32.SetCursorPos(point.x, point.y):
        raise RuntimeError("could not place the host pointer for a DOSBox-X click")
    user32.mouse_event(0x0002, 0, 0, 0, 0)  # MOUSEEVENTF_LEFTDOWN
    time.sleep(0.06)
    user32.mouse_event(0x0004, 0, 0, 0, 0)  # MOUSEEVENTF_LEFTUP
    return px, py


def _wait_capture(capture_dir: Path, before: set[Path], timeout: float) -> Path:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        candidates = [p for p in capture_dir.glob("*.png") if p not in before and p.is_file()]
        if candidates:
            latest = max(candidates, key=lambda p: p.stat().st_mtime_ns)
            size = latest.stat().st_size
            time.sleep(0.15)
            if latest.exists() and latest.stat().st_size == size and size > 128:
                return latest
        time.sleep(0.05)
    raise RuntimeError(f"DOSBox-X did not create a PNG screenshot under {capture_dir}")


def _copy_assets(destination: Path) -> None:
    if not (ASSETS / "KE.EXE").is_file() or not (ASSETS / "DOS4GW.EXE").is_file():
        raise FileNotFoundError("assets/ must contain the original KE.EXE and DOS4GW.EXE")
    shutil.copytree(ASSETS, destination, ignore=shutil.ignore_patterns("shot*.bmp", "*.tmp"))
    for name in ("KE.EXE", "DOS4GW.EXE"):
        src, copied = ASSETS / name, destination / name
        if hashlib.sha256(src.read_bytes()).digest() != hashlib.sha256(copied.read_bytes()).digest():
            raise RuntimeError(f"scratch copy verification failed for {name}")


def capture(out_dir: Path, keys: list[tuple[int, int]], clicks: list[tuple[int, int, int]],
            shots: list[tuple[int, str]], timeout: float, record_video: bool = False,
            video_start_ms: int = 0, video_stop_ms: int | None = None) -> list[Path]:
    # Import the repository's pinned runner only for its locked DOSBox-X binary
    # path/hash check; the compiler-oriented run_dosbox() is not suitable for a game.
    sys.path.insert(0, str(ROOT / "tools"))
    import dosbox as dosbox_runner  # type: ignore[import-not-found]

    dosbox_runner.check_runner()
    exe = dosbox_runner.DOSBOX_X.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    marker = f"KE Reference {os.getpid()}"
    captures: list[Path] = []

    with tempfile.TemporaryDirectory(prefix="ke-refshot-") as temp_name:
        temp = Path(temp_name)
        game = temp / "game"
        capture_dir = temp / "captures"
        capture_dir.mkdir()
        _copy_assets(game)
        autoexec = [f"mount C {_dos_mount_path(game)}", "C:", "CD " + chr(92)]
        queued = addkey_command(keys)
        if queued:
            autoexec.append(queued)
        autoexec.extend(("KE.EXE", "EXIT"))
        config = temp / "refshot.conf"
        config.write_text(
            "[sdl]\n"
            "output=surface\nfullscreen=false\nshowmenu=false\nwaitonerror=false\n"
            f"titlebar={marker}\nmapperfile={temp / 'refshot.map'}\n"
            "[cpu]\ncycles=auto\ncore=normal\n"
            "[mixer]\nnosound=true\n"
            "[dosbox]\nmachine=svga_s3\n"
            f"captures={capture_dir}\nshow recorded filename=false\n"
            "[autoexec]\n" + "\n".join(autoexec) + "\n",
            encoding="ascii",
        )
        output_log = temp / "dosbox-x.log"
        output_stream = output_log.open("wb")
        command = [str(exe), "-conf", str(config), "-fastlaunch", "-noconsole", "-keydbg"]
        proc = subprocess.Popen(command,
                                cwd=temp, stdout=output_stream, stderr=subprocess.STDOUT,
                                creationflags=0)
        try:
            hwnd = _find_window(proc.pid, marker)
            t0 = time.monotonic()
            events = [(ms, 1, "shot", name) for ms, name in shots]
            events.extend((ms, 0, "click", (x, y)) for ms, x, y in clicks)
            if record_video:
                events.append((video_start_ms, -1, "video_start", None))
                if video_stop_ms is not None:
                    events.append((video_stop_ms, 2, "video_stop", None))
            events.sort(key=lambda item: (item[0], item[1]))
            recording_video = False
            for ms, _priority, kind, payload in events:
                # F11 is held for 60 ms before the second hotkey. Schedule
                # host-hotkey triggers at the requested timestamp.
                dispatch_ms = hotkey_dispatch_ms(ms) if kind in (
                    "shot", "video_start", "video_stop") else ms
                due = t0 + dispatch_ms / 1000.0
                while time.monotonic() < due:
                    if proc.poll() is not None:
                        raise RuntimeError(f"DOSBox-X exited early with status {proc.returncode}")
                    time.sleep(min(0.02, due - time.monotonic()))
                if kind == "video_start":
                    _host_chord(hwnd, ord("I"))
                    recording_video = True
                    print(f"{ms:>6} ms  started DOSBox-X AVI capture")
                elif kind == "video_stop":
                    _host_chord(hwnd, ord("I"))
                    recording_video = False
                    print(f"{ms:>6} ms  stopped DOSBox-X AVI capture")
                elif kind == "click":
                    x, y = payload
                    _activate_window(hwnd)
                    dispatched_ms = (time.monotonic() - t0) * 1000.0
                    px, py = _click(hwnd, x, y)
                    print(f"{ms:>6} ms  left click at VGA ({x},{y}), window ({px},{py}); "
                          f"dispatched {dispatched_ms:.1f} ms")
                else:
                    name = payload
                    before = set(capture_dir.glob("*.png"))
                    trigger_start = time.monotonic()
                    _host_chord(hwnd, ord("P"))
                    captured = _wait_capture(capture_dir, before, timeout)
                    target = out_dir / f"{name}.png"
                    shutil.copy2(captured, target)
                    captures.append(target)
                    actual_ms = (trigger_start + 0.06 - t0) * 1000.0
                    print(f"{ms:>6} ms  {target}  {captured.stat().st_size} bytes; "
                          f"capture triggered near {actual_ms:.1f} ms")
            if recording_video:
                _host_chord(hwnd, ord("I"))
                recording_video = False
            if record_video:
                videos = sorted(capture_dir.glob("*.avi"), key=lambda p: p.stat().st_mtime_ns)
                if not videos:
                    raise RuntimeError("DOSBox-X did not create an AVI recording")
                sizes = {video: video.stat().st_size for video in videos}
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    time.sleep(0.15)
                    current = {video: video.stat().st_size for video in videos if video.exists()}
                    if len(current) == len(videos) and current == sizes:
                        break
                    sizes = current
                if any(not video.exists() or video.stat().st_size <= 128 for video in videos):
                    raise RuntimeError("DOSBox-X AVI recording was empty or not flushed")
                for index, video in enumerate(videos, 1):
                    target = (out_dir / "reference.avi" if len(videos) == 1 else
                              out_dir / f"reference-part-{index:03d}.avi")
                    shutil.copy2(video, target)
                    print(f"refshot: video segment {index}/{len(videos)} saved to {target} "
                          f"({video.stat().st_size} bytes)")
                if len(videos) > 1:
                    shutil.copy2(videos[-1], out_dir / "reference.avi")
            # Close through DOSBox-X's documented host shortcut.
            _exit_dosbox(hwnd)
            if proc.poll() is None:
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    # Some SDL1 builds swallow Ctrl+F9 while the game has the
                    # mouse captured. The captures are already flushed; stop
                    # the private emulator process deterministically.
                    proc.terminate()
                    proc.wait(timeout=5)
                    print("refshot: DOSBox-X needed the terminate fallback after capture")
            if proc.returncode not in (0, 1, -15, None):
                raise RuntimeError(f"DOSBox-X exited with status {proc.returncode}")
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
            output_stream.close()
            if proc.returncode not in (0, 1, -15, None) and output_log.exists():
                tail = output_log.read_text(encoding="latin-1", errors="replace").splitlines()[-20:]
                if tail:
                    print("\n".join(tail), file=sys.stderr)
    return captures


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--keys", default="1500:39",
                    help="comma-separated ms:hex XT scan codes, relative to DOSBox-X startup")
    ap.add_argument("--shots", default="1200=dos4gw-report,3500=title-transition,6500=main-menu",
                    help="comma-separated ms=name screenshot times")
    ap.add_argument("--clicks", default="",
                    help="optional comma-separated ms:x:y left clicks in 320x200 VGA coordinates")
    ap.add_argument("--out", default=str(ROOT / "docs" / "port" / "reference-captures"))
    ap.add_argument("--timeout", type=float, default=30.0,
                    help="maximum wait for a DOSBox screenshot file")
    ap.add_argument("--video", action="store_true",
                    help="record a DOSBox-X AVI alongside the scheduled screenshots")
    ap.add_argument("--video-start-ms", type=int, default=0,
                    help="start AVI recording at this event time (requires --video)")
    ap.add_argument("--video-stop-ms", type=int,
                    help="stop AVI recording at this event time; defaults to after the last event")
    args = ap.parse_args()
    try:
        keys = parse_keys(args.keys)
        clicks = parse_clicks(args.clicks)
        shots = parse_shots(args.shots)
        if args.video_start_ms < 0 or (args.video_stop_ms is not None and args.video_stop_ms < 0):
            raise ValueError("video event times must be nonnegative")
        if args.video_stop_ms is not None and args.video_stop_ms <= args.video_start_ms:
            raise ValueError("--video-stop-ms must be later than --video-start-ms")
        if not args.video and (args.video_start_ms != 0 or args.video_stop_ms is not None):
            raise ValueError("--video-start-ms and --video-stop-ms require --video")
        capture(Path(args.out).resolve(), keys, clicks, shots, args.timeout, args.video,
                args.video_start_ms, args.video_stop_ms)
    except (OSError, RuntimeError, ValueError, ImportError) as exc:
        print(f"refshot: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
