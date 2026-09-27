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


AUTO = {"port/oracle/oracle_main.c", "port/stubs/asm_data.S", "port/stubs/asm_stubs.c"}


def auto_resolve():
    """resolve conflicts limited to test registrations (union of both sides) and generated stubs (regenerate)"""
    import re
    rc, out = run(["git", "diff", "--name-only", "--diff-filter=U"])
    files = set(out)
    if not files or not files <= AUTO:
        return False
    if "port/oracle/oracle_main.c" in files:
        p = ROOT / "port/oracle/oracle_main.c"

        def union(m):
            a, b = m.group(1).splitlines(True), m.group(2).splitlines(True)
            return "".join(a + [l for l in b if l not in a])
        p.write_text(re.sub(r"<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n", union, p.read_text(), flags=re.S),
                     newline="\n")
    for f in files & {"port/stubs/asm_data.S", "port/stubs/asm_stubs.c"}:
        run(["git", "checkout", "--theirs", f])
    if run([sys.executable, "port/tools/gen_asm_stubs.py"])[0]:
        return False
    run(["git", "add", "-A", "port"])
    print("  auto-resolved: " + ", ".join(sorted(files)))
    return True


def main(branches):
    for br in branches:
        head = run(["git", "rev-parse", "HEAD"])[1][0]
        rc, out = run(["git", "merge", "--no-ff", "--no-edit", br])
        if rc and auto_resolve():
            run(["git", "commit", "-q", "--no-edit"])
            rc = 1 if (ROOT / ".git" / "MERGE_HEAD").exists() else 0
        if rc:
            run(["git", "merge", "--abort"])
            print(f"{br}: MERGE CONFLICT\n  " + "\n  ".join(out[-15:]))
            return 1
        for g in GATES:
            rc, out = run(g)
            if rc:
                run(["git", "reset", "--hard", head])
                fails = [l for l in out if "FAIL" in l or "mismatch" in l.lower()]
                print(f"{br}: GATE FAILED {' '.join(g)} -> merge undone\n  " + "\n  ".join(fails[:20] + out[-8:]))
                return 1
        print(f"{br}: merged, all gates pass ({out[-1] if out else ''})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
