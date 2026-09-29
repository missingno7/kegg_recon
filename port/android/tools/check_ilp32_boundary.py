"""Check the call boundary of the ILP32 game world (docs/android/architecture.md, section 3).

    python check_ilp32_boundary.py --nm LLVM_NM --stamp OUT obj1.o obj2.o ...

Every symbol an ILP32-world object references must be defined by another ILP32-world object
or be one of the 64-bit entry points whose parameters and results are 32-bit scalars
(ilp32_world.RENAME targets and ilp32_world.DIRECT_OK). Every function the ILP32 world
defines must not also be a C-library name, so that no 64-bit caller in the same shared
object can bind to 32-bit code by accident. Writes OUT (a stamp) on success; exit 1 lists
the offending symbols.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ilp32_world  # noqa: E402

# C library / compiler runtime names a 64-bit caller might use: none may be defined by the
# 32-bit world (they are renamed to ke32_* instead).
LIBC_NAMES = {
    "malloc", "free", "calloc", "realloc", "memcpy", "memmove", "memset", "memcmp", "strcpy",
    "strncpy", "strcat", "strlen", "strcmp", "strncmp", "strchr", "strrchr", "strstr", "printf",
    "fprintf", "sprintf", "snprintf", "vsnprintf", "puts", "fopen", "fclose", "fread", "fwrite",
    "fseek", "ftell", "fflush", "abs", "atoi", "strtol", "strtoul", "exit", "atexit", "getenv",
    "qsort", "rand", "srand", "time", "clock", "sin", "cos", "sqrt", "floor", "ceil",
}


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--nm", required=True)
    ap.add_argument("--stamp", required=True)
    ap.add_argument("objects", nargs="+")
    args = ap.parse_args(argv)
    defined, undefined = {}, {}
    for obj in args.objects:
        text = subprocess.run([args.nm, obj], capture_output=True, text=True, check=True).stdout
        for line in text.splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[0] == "U":
                undefined.setdefault(parts[1], set()).add(Path(obj).name)
            elif len(parts) == 3 and parts[1] in "TDBRVW":
                defined[parts[2]] = Path(obj).name
    allowed = set(ilp32_world.RENAME.values()) | ilp32_world.DIRECT_OK
    bad = []
    for name, users in sorted(undefined.items()):
        if name in defined or name in allowed:
            continue
        bad.append(f"  {name} (referenced by {', '.join(sorted(users)[:4])})")
    clashes = sorted(n for n in defined if n in LIBC_NAMES)
    if bad or clashes:
        if bad:
            print("ILP32 world references symbols outside the 32-bit-scalar boundary:")
            print("\n".join(bad))
        if clashes:
            print("ILP32 world defines C library names: " + ", ".join(clashes))
        return 1
    Path(args.stamp).write_text(
        f"{len(args.objects)} ILP32 objects, {len(defined)} symbols defined, "
        f"{len([n for n in undefined if n not in defined])} external references, all allowed\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
