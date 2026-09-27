"""Compiler launcher for the historical units: byte-packed, declaration-ordered data.

Watcom 10.0 places a unit's _DATA/_BSS variables back to back with no alignment padding,
and the game relies on it: several records are declared as consecutive globals and then
accessed through one struct pointer (e.g. the 57-byte interrupt records key_irq/tmr_rec
in src/u_0d4ba.c, docs/port/architecture.md "Data layout"). gcc aligns every global to
its type. This launcher (CMake C_COMPILER_LAUNCHER of target ke_game) compiles to
assembly, deletes the alignment directives inside .data/.bss, and assembles:

    python gcc_pack_data.py <gcc> <args ... -o OUT.obj -c SRC.c>

Together with -fno-toplevel-reorder (source order) and -fno-zero-initialized-in-bss
(`= 0` variables stay in .data next to their neighbours, as in Watcom's _DATA) the unit's
data layout equals the original's (checked by port/tools/check_layouts.py --data).
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

DATA_SECTIONS = re.compile(r"^\s*\.(data|bss)\b|^\s*\.section\s+\.(data|bss)")
OTHER_SECTION = re.compile(r"^\s*\.(text|section)\b")
ALIGN = re.compile(r"^\s*\.(align|p2align|balign)\b")


def pack(asm: str) -> str:
    out, in_data = [], False
    for line in asm.splitlines():
        if DATA_SECTIONS.match(line):
            in_data = True
        elif OTHER_SECTION.match(line):
            in_data = False
        if in_data and ALIGN.match(line):
            continue
        out.append(line)
    return "\n".join(out) + "\n"


def main(argv):
    cmd = list(argv)
    if "-c" not in cmd or "-o" not in cmd:
        return subprocess.call(cmd)
    out = Path(cmd[cmd.index("-o") + 1])
    asm = out.with_suffix(".packed.s")
    s_cmd = [a for a in cmd]
    s_cmd[s_cmd.index("-c")] = "-S"
    s_cmd[s_cmd.index("-o") + 1] = str(asm)
    rc = subprocess.call(s_cmd)
    if rc:
        return rc
    asm.write_text(pack(asm.read_text()))
    return subprocess.call([cmd[0], "-c", str(asm), "-o", str(out)])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
