"""Smoke run of ke_sdl3.exe: start, answer the startup prompt, idle, quit, report.

    python port/tools/smoke.py [--ms 6000] [--keys "1500:39"] [--shot out.bmp] [--build build/port]

Runs the game with KE_AUTOKEYS (default: SPACE at 1.5 s for the startup report prompt) and
KE_EXIT_AFTER_MS, then prints: stubs reached, where the game thread was at quit (resolved
with addr2line) and faults. Exit 0 when the game thread unwound cleanly (exit code 0, no
fault). Needs the original data files in assets/.
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ms", type=int, default=6000)
    ap.add_argument("--keys", default="1500:39")
    ap.add_argument("--shot", default=None, help="save the frame shown at --ms - 500 as BMP")
    ap.add_argument("--build", default=str(ROOT / "build" / "port"))
    ap.add_argument("--env", action="append", default=[], help="extra NAME=value")
    a = ap.parse_args()
    exe = (Path(a.build) / "ke_sdl3.exe").resolve()
    env = dict(os.environ, KE_AUTOKEYS=a.keys, KE_EXIT_AFTER_MS=str(a.ms))
    if a.shot:
        env["KE_SCREENSHOT"] = f"{max(a.ms - 500, 0)}:{Path(a.shot).resolve()}"
    for kv in a.env:
        k, v = kv.split("=", 1)
        env[k] = v
    r = subprocess.run([str(exe), str(ROOT / "assets")], env=env, capture_output=True,
                       timeout=a.ms / 1000 + 30)
    out = r.stdout.decode("cp437", "replace") + r.stderr.decode("cp437", "replace")
    log = [l for l in out.splitlines() if l.startswith("[")]
    stubs = sorted({m.group(1) for l in log for m in [re.search(r"STUB (\w+) \(", l)] if m})
    faults = [l for l in log if "fault" in l and "ERROR" in l]
    bt = next((l for l in log if "game thread (" in l), None)
    print(f"exit code {r.returncode}; {len(stubs)} stubs reached: {', '.join(stubs)}")
    if bt:
        addrs = bt.split("addresses:")[1].split()
        res = subprocess.run(["addr2line", "-f", "-s", "-e", str(exe)] + addrs,
                             capture_output=True, text=True).stdout.splitlines()
        frames = [f"{res[i]} ({res[i + 1]})" for i in range(0, len(res) - 1, 2) if res[i] != "??"]
        print("game thread at quit: " + " <- ".join(frames))
    for f in faults:
        print(f)
    ok = r.returncode == 0 and not faults and any("game thread finished" in l for l in log)
    print("SMOKE OK" if ok else "SMOKE FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
