"""Supervisor helper: merge work-package branches into portable-sdl3 and run every gate; undo the merge on failure.

    python port/tools/mergepkg.py port/p1 [port/a3 ...]
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENV = dict(os.environ, PATH="C:/msys64/mingw32/bin;" + os.environ["PATH"])
GATES = [
    ["C:/msys64/mingw32/bin/cmake.exe", "-S", "port", "-B", "build/port", "-G", "Ninja"],
    ["C:/msys64/mingw32/bin/cmake.exe", "--build", "build/port"],
    [sys.executable, "port/tools/le_export.py"],
    [str(ROOT / "build/port/oracle/ke_oracle.exe"), "build/port/oracle"],
    [sys.executable, "port/tools/check_layouts.py", "--data"],
    [sys.executable, "port/tools/gen_asm_stubs.py", "--check"],
    [sys.executable, "port/tools/smoke.py"],
]


def run(cmd):
    r = subprocess.run(cmd, cwd=ROOT, env=ENV, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip().splitlines()


def main(branches):
    for br in branches:
        head = run(["git", "rev-parse", "HEAD"])[1][0]
        rc, out = run(["git", "merge", "--no-ff", "--no-edit", br])
        if rc:
            run(["git", "merge", "--abort"])
            print(f"{br}: MERGE CONFLICT\n  " + "\n  ".join(out[-15:]))
            return 1
        for g in GATES:
            rc, out = run(g)
            if rc:
                run(["git", "reset", "--hard", head])
                print(f"{br}: GATE FAILED {' '.join(g)} -> merge undone\n  " + "\n  ".join(out[-15:]))
                return 1
        print(f"{br}: merged, all gates pass ({out[-1] if out else ''})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
