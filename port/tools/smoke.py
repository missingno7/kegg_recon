"""Scripted SDL port run, capture helper, and kegg_forged input replay converter.

Examples::

    python port/tools/smoke.py
    python port/tools/smoke.py --keys "1500:39" --clicks "8000:150:82,11000:150:82" \
        --shots "3500=startup-title,6500=main-menu,10000=game-entry"
    python port/tools/smoke.py --replay D:/Games/DOS/dos_recosystem/kegg_forged/artifacts/replay-inputs/pmrec_20260723_200732.input.json

Mouse coordinates are game-raster coordinates (320x240 menu maximum); injection maps them
to the DOS driver's 2:1 mouse coordinate space. Replays are input-only and discard PF snapshots,
checkpoints, digests, and machine-time data.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]


def parse_timed_codes(spec: str, what: str, max_code: int) -> list[tuple[int, int]]:
    result = []
    for entry in filter(None, (part.strip() for part in spec.split(","))):
        try:
            ms_text, code_text = entry.split(":", 1)
            ms, code = int(ms_text, 10), int(code_text, 16)
        except ValueError as exc:
            raise ValueError(f"invalid {what} item {entry!r}") from exc
        if ms < 0 or not 0 <= code <= max_code:
            raise ValueError(f"{what} item out of range: {entry!r}")
        result.append((ms, code))
    if result != sorted(result):
        raise ValueError(f"{what} times must be nondecreasing")
    return result


def parse_mouse(spec: str, what: str) -> list[tuple[int, int, int]]:
    result = []
    for entry in filter(None, (part.strip() for part in spec.split(","))):
        try:
            ms_text, x_text, y_text = entry.split(":", 2)
            ms, x, y = int(ms_text, 10), int(x_text, 10), int(y_text, 10)
        except ValueError as exc:
            raise ValueError(f"invalid {what} item {entry!r}; expected ms:x:y") from exc
        if ms < 0 or not 0 <= x <= 319 or not 0 <= y <= 239:
            raise ValueError(f"{what} item out of range: {entry!r} (expected x=0..319, y=0..239)")
        result.append((ms, x, y))
    if result != sorted(result):
        raise ValueError(f"{what} times must be nondecreasing")
    return result


def parse_shots(spec: str) -> list[tuple[int, str]]:
    result = []
    for entry in filter(None, (part.strip() for part in spec.split(","))):
        try:
            ms_text, name = entry.split("=", 1)
            ms, name = int(ms_text, 10), name.strip()
        except ValueError as exc:
            raise ValueError(f"invalid screenshot item {entry!r}; expected ms=name") from exc
        if ms < 0 or not name or not re.fullmatch(r"[A-Za-z0-9_-]+", name):
            raise ValueError(f"invalid screenshot item {entry!r}")
        result.append((ms, name))
    if result != sorted(result):
        raise ValueError("screenshot times must be nondecreasing")
    if len({name for _, name in result}) != len(result):
        raise ValueError("screenshot names must be unique")
    return result


def parse_frame_shots(spec: str) -> list[tuple[int, str]]:
    result = []
    for entry in filter(None, (part.strip() for part in spec.split(","))):
        try:
            frame_text, name = entry.split("=", 1)
            frame, name = int(frame_text, 10), name.strip()
        except ValueError as exc:
            raise ValueError(f"invalid frame screenshot item {entry!r}; expected frame=name") from exc
        if not 0 <= frame <= 0xffffffff or not name or not re.fullmatch(r"[A-Za-z0-9_-]+", name):
            raise ValueError(f"invalid frame screenshot item {entry!r}")
        result.append((frame, name))
    if not result:
        raise ValueError("at least one frame screenshot is required")
    if len(result) > 64:
        raise ValueError("at most 64 frame screenshots can be scheduled in one run")
    if result != sorted(result):
        raise ValueError("frame screenshot targets must be nondecreasing")
    if len({name for _, name in result}) != len(result):
        raise ValueError("frame screenshot names must be unique")
    return result


def _bounded_int(value: Any, low: int, high: int, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        raise ValueError(f"invalid {label}: {value!r}")
    return value


def convert_replay(source: Path, dest: Path) -> int:
    """Convert PF input channels to host input events; return source frame count."""
    data = json.loads(source.read_text(encoding="utf-8"))
    fmt = data.get("format")
    if fmt == "portforge-dos-input-script-v1":
        source_events = data.get("events", [])
        get_occurrence = lambda e: e.get("occurrence")
    elif fmt == "portforge-replay-v2":
        source_events = data.get("events", [])

        def get_occurrence(event: dict[str, Any]) -> Any:
            at = event.get("at", {})
            if at.get("phase") != "before" or at.get("point") != "kegg.frame-update":
                raise ValueError("replay contains an unsupported boundary; expected before kegg.frame-update")
            return at.get("occurrence")
    else:
        raise ValueError(f"unsupported replay format {fmt!r}")

    converted: list[tuple[int, int, str, tuple[int, ...]]] = []
    max_occurrence = -1
    for order, event in enumerate(source_events):
        channel = event.get("channel")
        if channel not in ("dos.mouse.normalized", "dos.keyboard.scancodes"):
            continue
        occurrence = _bounded_int(get_occurrence(event), 0, 1000000, "occurrence")
        max_occurrence = max(max_occurrence, occurrence)
        sequence = _bounded_int(event.get("sequence", order), 0, 100000000, "sequence")
        payload = event.get("payload")
        if channel == "dos.mouse.normalized":
            if not isinstance(payload, dict):
                raise ValueError(f"mouse payload at occurrence {occurrence} is not an object")
            u, v = payload.get("u"), payload.get("v")
            if (isinstance(u, bool) or not isinstance(u, (int, float)) or not math.isfinite(u) or
                    isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) or
                    not 0 <= u <= 1 or not 0 <= v <= 1):
                raise ValueError(f"normalized mouse coordinates out of range at occurrence {occurrence}")
            buttons = _bounded_int(payload.get("buttons"), 0, 7, "mouse buttons")
            # PF normalizes game-raster coordinates. The DOS mouse driver uses twice that range.
            x = round(float(u) * 319) * 2
            y = round(float(v) * 199) * 2
            converted.append((occurrence, sequence, "M", (x, y, buttons)))
        else:
            if not isinstance(payload, list) or not payload:
                raise ValueError(f"keyboard payload at occurrence {occurrence} is not a scancode list")
            for raw in payload:
                scan = _bounded_int(raw, 0, 255, "XT scancode")
                converted.append((occurrence, sequence, "K", (scan,)))

    converted.sort(key=lambda item: (item[0], item[1]))
    frames = max_occurrence + 1 if max_occurrence >= 0 else 0
    dest.parent.mkdir(parents=True, exist_ok=True)
    with dest.open("w", encoding="ascii", newline="\n") as out:
        out.write(f"KEPORTREPLAY 1 {len(converted)} {frames}\n")
        for occurrence, _sequence, kind, values in converted:
            out.write(f"{kind} {occurrence} " + " ".join(map(str, values)) + "\n")
    return frames


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ms", type=int, default=None, help="run duration; replay mode chooses a duration from its frame count")
    ap.add_argument("--keys", default="1500:39", help="comma-separated ms:hex XT scancode pulses")
    ap.add_argument("--mouse", default="", help="comma-separated ms:x:y absolute moves in the 320x240 menu raster")
    ap.add_argument("--clicks", default=None, help="comma-separated ms:x:y left clicks in the 320x240 menu raster")
    ap.add_argument("--shots", default="", help="comma-separated ms=name screenshots, compatible with refshot.py")
    ap.add_argument("--frame-shots", default="",
                    help="comma-separated retrace-count=name screenshots (absolute virtual VGA retrace count)")
    ap.add_argument("--shot", default=None, help="save one frame at --ms minus 500 ms (legacy smoke option)")
    ap.add_argument("--shot-dir", default=None, help="directory for --shots BMP files")
    ap.add_argument("--timer-diag", action="store_true",
                    help="report and validate the game's timer calibration at shutdown")
    ap.add_argument("--audio-dump", default=None,
                    help="record samples submitted to SDL as WAV (plus a .dsp.log sidecar)")
    ap.add_argument("--replay", default=None, help="input-only PF JSON replay or legacy input script to convert and run")
    ap.add_argument("--replay-start-ms", type=int, default=None, help="arm occurrence zero at this process time (default 11500 ms)")
    ap.add_argument("--build", default=str(ROOT / "build" / "port"))
    ap.add_argument("--env", action="append", default=[], help="extra NAME=value")
    a = ap.parse_args()

    if a.ms is not None and a.ms < 0:
        ap.error("--ms must be nonnegative")
    if a.replay_start_ms is not None and a.replay_start_ms < 0:
        ap.error("--replay-start-ms must be nonnegative")
    try:
        keys = parse_timed_codes(a.keys, "key", 0x1ff)
        mouse_moves = parse_mouse(a.mouse, "mouse move")
        clicks = parse_mouse(a.clicks or "", "click")
        shots = parse_shots(a.shots)
        frame_shots = parse_frame_shots(a.frame_shots) if a.frame_shots else []
    except ValueError as exc:
        ap.error(str(exc))
    if sum(bool(x) for x in (a.shot, shots, frame_shots)) > 1:
        ap.error("use only one of --shot, --shots, or --frame-shots")

    exe = (Path(a.build) / "ke_sdl3.exe").resolve()
    env = dict(os.environ)
    for name in ("KE_AUTOKEYS", "KE_AUTOMOUSE", "KE_AUTOCLICKS", "KE_SCREENSHOT",
                 "KE_SCREENSHOTS", "KE_FRAME_SHOTS", "KE_REPLAY", "KE_REPLAY_START_MS",
                 "KE_EXIT_AFTER_MS", "KE_TIMER_DIAG", "KE_AUDIO_DUMP"):
        env.pop(name, None)
    env["KE_AUTOKEYS"] = ",".join(f"{ms}:{code:x}" for ms, code in keys)
    if a.audio_dump:
        audio_path = Path(a.audio_dump).resolve()
        audio_path.parent.mkdir(parents=True, exist_ok=True)
        env["KE_AUDIO_DUMP"] = str(audio_path)
    if mouse_moves:
        env["KE_AUTOMOUSE"] = ",".join(f"{ms}:{x}:{y}" for ms, x, y in mouse_moves)

    replay_frames = 0
    if a.replay:
        source = Path(a.replay).resolve()
        replay_file = Path(a.build).resolve() / "replays" / (source.stem + ".kereplay")
        try:
            replay_frames = convert_replay(source, replay_file)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            print(f"smoke: replay conversion failed: {exc}", file=sys.stderr)
            return 2
        replay_start_ms = a.replay_start_ms if a.replay_start_ms is not None else 11500
        env["KE_REPLAY"] = str(replay_file)
        env["KE_REPLAY_START_MS"] = str(replay_start_ms)
        if a.clicks is None:
            clicks = [(8000, 150, 82), (11000, 150, 82)]
        print(f"converted {source.name}: {replay_frames} frame occurrences -> {replay_file}")
        print(f"replay begins at {replay_start_ms} ms after the menu-start clicks")
        if a.ms is None:
            a.ms = replay_start_ms + (replay_frames * 1000 // 55) + 2500

    if clicks:
        env["KE_AUTOCLICKS"] = ",".join(f"{ms}:{x}:{y}" for ms, x, y in clicks)
    if a.shot:
        if a.ms is None:
            ap.error("--shot needs --ms")
        shot_path = Path(a.shot).resolve()
        shot_path.parent.mkdir(parents=True, exist_ok=True)
        env["KE_SCREENSHOT"] = f"{max(a.ms - 500, 0)}:{shot_path}"
    elif shots:
        shot_dir = Path(a.shot_dir).resolve() if a.shot_dir else (Path(a.build).resolve() / "captures")
        shot_dir.mkdir(parents=True, exist_ok=True)
        entries = []
        for ms, name in shots:
            path = (shot_dir / f"{name}.bmp").resolve()
            entries.append(f"{ms}:{path}")
        env["KE_SCREENSHOTS"] = ",".join(entries)
    elif frame_shots:
        shot_dir = Path(a.shot_dir).resolve() if a.shot_dir else (Path(a.build).resolve() / "captures")
        shot_dir.mkdir(parents=True, exist_ok=True)
        entries = []
        for frame, name in frame_shots:
            path = (shot_dir / f"{name}.bmp").resolve()
            entries.append(f"{frame}:{path}")
        env["KE_FRAME_SHOTS"] = ",".join(entries)

    if a.ms is None:
        a.ms = 6000
    env["KE_EXIT_AFTER_MS"] = str(a.ms)
    if a.timer_diag:
        env["KE_TIMER_DIAG"] = "1"
    for kv in a.env:
        k, v = kv.split("=", 1)
        env[k] = v

    # Keep replay/smoke behavior independent of a user's persistent config.
    env["KE_MOUSE_MODE"] = "faithful"
    try:
        with tempfile.TemporaryDirectory(prefix="ke-smoke-config-") as config_dir:
            env["KE_CONFIG_DIR"] = config_dir
            r = subprocess.run([str(exe), str(ROOT / "assets")], env=env, capture_output=True,
                               timeout=a.ms / 1000 + 30)
    except subprocess.TimeoutExpired:
        print(f"SMOKE FAILED: timed out after {a.ms / 1000 + 30:.1f}s", file=sys.stderr)
        return 1
    out = r.stdout.decode("cp437", "replace") + r.stderr.decode("cp437", "replace")
    log = [line for line in out.splitlines() if line.startswith("[")]
    timer_diag = re.search(r"timer diagnostic: timer_ok=(-?\d+) timer_delta=(\d+) timer_enabled09=(-?\d+)", out)
    stubs = sorted({m.group(1) for line in log for m in [re.search(r"STUB (\w+) \(", line)] if m})
    faults = [line for line in log if "fault" in line and "ERROR" in line]
    bt = next((line for line in log if "game thread (" in line), None)
    print(f"exit code {r.returncode}; {len(stubs)} stubs reached: {', '.join(stubs)}")
    if a.audio_dump:
        audio_path = Path(a.audio_dump).resolve()
        print(f"audio dump: {audio_path} (+ {audio_path}.dsp.log)")
    if bt:
        addrs = bt.split("addresses:")[1].split()
        res = subprocess.run(["addr2line", "-f", "-s", "-e", str(exe)] + addrs,
                             capture_output=True, text=True).stdout.splitlines()
        frames = [f"{res[i]} ({res[i + 1]})" for i in range(0, len(res) - 1, 2) if res[i] != "??"]
        print("game thread at quit: " + " <- ".join(frames))
    for line in faults:
        print(line)
    notable = [line for line in log if "replay" in line.lower() or "AUTOCLICKS" in line or
               "AUTOMOUSE" in line or (not faults and re.search(
                   r"screenshot .+ \(target (?:\d+ ms|frame \d+|retrace \d+)", line))]
    if len(notable) > 24:
        selected = notable[:12] + notable[-12:]
        omitted = len(notable) - len(selected)
    else:
        selected, omitted = notable, 0
    for line in selected:
        print(line)
    if omitted:
        print(f"... {omitted} additional automation log lines omitted")
    timer_valid = True
    if a.timer_diag:
        if timer_diag:
            timer_ok, timer_delta, timer_enabled = map(int, timer_diag.groups())
            expected_period = min(abs(timer_delta - 17024), abs(timer_delta - 19904)) <= 512
            timer_valid = timer_ok == -1 and timer_enabled > 0 and expected_period
            print(f"timer diagnostic: timer_ok={timer_ok} timer_delta={timer_delta} "
                  f"timer_enabled09={timer_enabled} ({'valid' if timer_valid else 'INVALID'})")
        else:
            timer_valid = False
            print("timer diagnostic missing")
    ok = (r.returncode == 0 and not faults and any("game thread finished" in line for line in log)
          and timer_valid)
    print("SMOKE OK" if ok else "SMOKE FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
