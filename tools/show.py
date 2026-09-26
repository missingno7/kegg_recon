"""Annotated disassembly of an original code range (worker context).

    python tools/show.py 0x7032            # function starting here (extent from manifest/inventory, else to next prologue)
    python tools/show.py 0x7032 0x704d     # explicit [start, end)
    python tools/show.py NAME              # manifest function by name

Marks LE-fixup operands with their target (obj:off, known symbol name), resolves call/jmp targets to
manifest names, and lists callers of the range.
"""
from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parent.parent / 'build' / 'pylib'))
import capstone  # noqa: E402  (vendored: pip install --no-user --target build/pylib capstone==5.0.7)

sys.path.insert(0, str(Path(__file__).resolve().parent))
import le as lemod  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
PROLOGUE = b"\x53\x56\x57\x55\x89\xe5"


def load():
    L = lemod.LE(ROOT / "assets" / "KE.EXE")
    code = L.object_bytes(L.objects[0])[:L.objects[0]["vsize"]]
    fix = {off: tgt for obj, off, typ, tgt in L.resolved_fixups() if obj == 1}
    man = json.loads((ROOT / "manifest.json").read_text())
    return L, code, fix, man


def names(man):
    n = {}
    for f in man.get("functions", []):
        if f.get("name"):
            n[f"1:{int(f['start'], 16):x}"] = f["name"]
    for k, v in man.get("symbols", {}).items():
        n[v] = k
    return n


def extent(code, man, start):
    for f in man.get("functions", []):
        if int(f["start"], 16) == start:
            return int(f["end"], 16)
    inv = ROOT / "build" / "inventory.json"
    if inv.exists():
        try:
            for e in json.loads(inv.read_text()).get("entries", []):
                if int(str(e["start"]), 16) == start:
                    return int(str(e["end"]), 16)
        except Exception:
            pass
    m = code.find(PROLOGUE, start + 1)
    return m if m != -1 else len(code)


def main(argv):
    L, code, fix, man = load()
    nm = names(man)
    if re.fullmatch(r"(0x)?[0-9a-fA-F]+", argv[1]):
        start = int(argv[1], 16)
    else:
        start = next(int(f["start"], 16) for f in man["functions"] if f.get("name") == argv[1])
    end = int(argv[2], 16) if len(argv) > 2 else extent(code, man, start)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    print(f"; obj1 {start:#x}..{end:#x} ({end - start} bytes) {nm.get(f'1:{start:x}', '')}")
    for ins in md.disasm(code[start:end], start):
        note = []
        for a in range(ins.address, ins.address + ins.size):
            if a in fix:
                t = fix[a]
                key = f"{t.get('obj')}:{t.get('off', 0):x}"
                note.append(f"fix->{key}" + (f" {nm[key]}" if key in nm else ""))
        if ins.mnemonic in ("call", "jmp") and ins.bytes[0] in (0xE8, 0xE9):
            tgt = ins.address + ins.size + struct.unpack_from("<i", ins.bytes, 1)[0]
            key = f"1:{tgt:x}"
            if key in nm:
                note.append(nm[key])
        print(f"{ins.address:05x}  {ins.bytes.hex():20} {ins.mnemonic:6} {ins.op_str:34} {' '.join(note)}")
    callers = [i for i in range(len(code) - 5) if code[i] == 0xE8
               and i + 5 + struct.unpack_from("<i", code, i + 1)[0] == start]
    ptrs = [a for a, t in fix.items() if t.get("obj") == 1 and t.get("off") == start]
    print(f"; callers: {' '.join(hex(c) for c in callers[:20])}{' ...' if len(callers) > 20 else ''}"
          f"  address-taken at: {' '.join(hex(p) for p in ptrs)}")


if __name__ == "__main__":
    main(sys.argv)
