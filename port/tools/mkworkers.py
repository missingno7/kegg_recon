"""Supervisor helper: create one git worktree + branch per port work package and write its worker prompt.

    python port/tools/mkworkers.py PKG[+PKG] ...      e.g. A1+A4 A3 V1

Worktree D:/kgw/<name> on branch port/<name> from the current portable-sdl3 HEAD; assets/ copied (gitignored);
prompt at <worktree>/WORKER_PROMPT.md (not committed).
"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = Path("D:/kgw")

PROMPT = """Repo worktree: {wt} (git branch `port/{name}`, created from `portable-sdl3`). Project: SDL3 Windows port of the
1994 DOS game Krypton Egg. The historical byte-exact source is frozen at tag `historical-exact-clean-v1`; this branch
runs that original code on a "virtual PC" (port/vhw) under SDL3, with the TASM modules translated to C and verified
against the original machine code executed natively (port/oracle).

Read docs/port/architecture.md, then docs/port/workpackages.md: "Common rules" and your package section(s): {pkgs}.
You are worker "{name}". Work ONLY in this worktree ({wt}); own only the files your package(s) list; if you need a
change in a file another package owns, make the smallest compatible change and list it in your hand-off (the
supervisor merges all package branches). Build with the mingw32 toolchain:
    export PATH=/c/msys64/mingw32/bin:$PATH
    cmake -S port -B build/port -G Ninja && cmake --build build/port
    python port/tools/le_export.py            (once, for the oracle image)
Run every gate from "Common rules" before hand-off (build without new warnings, oracle tests, check_layouts, stub
generator check, smoke). Original data files are in assets/. Behaviour must match the original: translations are
literal and oracle-tested (randomized inputs + a mutation check); virtual hardware follows the real device semantics
the game relies on. Iterate autonomously; do not ask. Commit on your branch in small logical commits with plain messages. Do not push.
Final answer <=15 lines: what works (tests/gates results), files changed, src/ PORT: edits, cross-package changes,
open issues.
"""


def main(argv):
    BASE.mkdir(exist_ok=True)
    for spec in argv:
        name = spec.lower().replace("+", "-")
        wt = BASE / name
        if not wt.exists():
            subprocess.run(["git", "-C", str(ROOT), "worktree", "add", "-q", "-b", f"port/{name}", str(wt),
                            "portable-sdl3"], check=True)
        if not (wt / "assets").exists():
            shutil.copytree(ROOT / "assets", wt / "assets")
        pkgs = ", ".join(spec.split("+"))
        (wt / "WORKER_PROMPT.md").write_text(PROMPT.format(wt=wt.as_posix(), name=name, pkgs=pkgs), encoding="utf-8")
        print(name, wt)


if __name__ == "__main__":
    main(sys.argv[1:])
