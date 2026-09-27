"""Check that gcc lays out the shared game structs exactly as Watcom did (docs/types.md).

    python port/tools/check_layouts.py [--data] [build/port]

Reads DWARF of every compiled historical unit (objdump --dwarf=info) and compares, for each
struct catalogued in docs/types.md, the byte size and every listed member offset. The
catalogue layouts are PROVEN by the byte-exact rebuild, so any difference is a port bug in
the compile flags (packing, bit-field allocation, enum size). --data also compares, per
unit, the spacing of consecutive initialized globals with the original object 3 addresses
(manifest.json symbols): record overlays need Watcom's packed _DATA order. Exit 1 on a
mismatch.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def catalogue():
    text = (ROOT / "docs" / "types.md").read_text(encoding="utf-8")
    out, cur = {}, None
    for line in text.splitlines():
        m = re.match(r"^## (\w+)\s*$", line)
        if m:
            cur = m.group(1)
            out[cur] = {"size": None, "members": {}}
            continue
        if not cur:
            continue
        m = re.match(r"^- \*\*Size:\*\* (\d+) bytes", line)
        if m:
            out[cur]["size"] = int(m.group(1))
        m = re.match(r"^\| (0x[0-9a-f]+) \| [^|]+ \| `(\w+)` \|", line)
        if m:
            out[cur]["members"][m.group(2)] = int(m.group(1), 16)
    return {k: v for k, v in out.items() if v["size"] is not None}


def dwarf_structs(obj: Path, objdump: str):
    text = subprocess.run([objdump, "--dwarf=info", str(obj)], capture_output=True, text=True).stdout
    structs, cur, depth_cur = [], None, None
    for line in text.splitlines():
        m = re.match(r"^\s*<(\d+)><[0-9a-f]+>: Abbrev Number: \d+ \((\w+)\)", line)
        if m:
            depth, tag = int(m.group(1)), m.group(2)
            if tag == "DW_TAG_structure_type":
                cur = {"name": None, "size": None, "members": {}}
                depth_cur = depth
                structs.append(cur)
                member = None
            elif cur is not None and depth <= depth_cur:
                cur = None
            elif cur is not None and tag == "DW_TAG_member" and depth == depth_cur + 1:
                member = {"name": None, "off": None, "bits": False}
                cur.setdefault("_m", []).append(member)
            else:
                member = None if cur is None else member
            continue
        if cur is None:
            continue
        m = re.match(r"^\s*<[0-9a-f]+>\s+DW_AT_(\w+)\s*:\s*(.*)$", line)
        if not m:
            continue
        attr, val = m.group(1), m.group(2).strip()
        target = cur["_m"][-1] if cur.get("_m") and member is cur["_m"][-1] else None
        if target is None:
            if attr == "name":
                cur["name"] = val.split(":")[-1].strip()
            elif attr == "byte_size":
                cur["size"] = int(val, 0)
        else:
            if attr == "name":
                target["name"] = val.split(":")[-1].strip()
            elif attr == "data_member_location":
                target["off"] = int(val.split()[0], 0)
            elif attr == "data_bit_offset":
                target["off"] = int(val.split()[0], 0) // 8
                target["bits"] = True
            elif attr == "bit_size":
                target["bits"] = True
    for s in structs:
        s["members"] = {m["name"]: (m["off"], m["bits"]) for m in s.pop("_m", []) if m["name"]}
    return structs


def check_data(objs, nm="nm"):
    """Within each unit, the distance between consecutive initialized globals must equal
    the distance between the same symbols in the original object 3 (manifest.json)."""
    import json
    symbols = json.loads((ROOT / "manifest.json").read_text())["symbols"]
    orig = {n: int(v.split(":")[1], 16) for n, v in symbols.items() if v.startswith("3:")}
    pairs = bad = 0
    for obj in objs:
        text = subprocess.run([nm, "-n", str(obj)], capture_output=True, text=True).stdout
        syms = []
        for line in text.splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[1] in "Dd" and parts[2].startswith("_"):
                syms.append((int(parts[0], 16), parts[2][1:]))
        for (ga, a), (gb, b) in zip(syms, syms[1:]):
            if a in orig and b in orig and orig[b] > orig[a]:
                pairs += 1
                if orig[b] - orig[a] != gb - ga:
                    bad += 1
                    if bad <= 20:
                        print(f"DATA {obj.name.replace('.c.obj', '')}: {a} -> {b}: "
                              f"gcc +{gb - ga:#x}, original +{orig[b] - orig[a]:#x}")
    print(f"{pairs} adjacent initialized-data pairs compared with the original, {bad} differ")
    return bad


def main(argv):
    data_mode = "--data" in argv
    argv = [a for a in argv if a != "--data"]
    build = Path(argv[0]) if argv else ROOT / "build" / "port"
    objdump = "objdump"
    objs = sorted(build.glob("CMakeFiles/ke_game.dir/**/*.obj"))
    if not objs:
        print(f"no historical objects under {build}; build first")
        return 2
    cat = catalogue()
    checked = mismatches = 0
    seen = set()
    for obj in objs:
        for s in dwarf_structs(obj, objdump):
            if s["name"] not in cat or s["size"] is None:   # unknown, or a declaration only
                continue
            want = cat[s["name"]]
            unit = obj.name.replace(".c.obj", "")
            checked += 1
            seen.add(s["name"])
            problems = []
            if s["size"] != want["size"]:
                problems.append(f"size {s['size']} != {want['size']}")
            for member, off in want["members"].items():
                if member not in s["members"]:
                    continue
                got, bits = s["members"][member]
                # bit-fields: docs/types.md lists the storage unit; the bit may sit in any byte
                ok = (off <= got < off + 4) if bits else got == off
                if not ok:
                    problems.append(f"{member} at {got:#x} != {off:#x}")
            if problems:
                mismatches += 1
                print(f"MISMATCH {s['name']} in {unit}: " + "; ".join(problems))
    missing = sorted(set(cat) - seen)
    print(f"{checked} struct definitions checked against docs/types.md "
          f"({len(seen)}/{len(cat)} catalogued types seen), {mismatches} mismatches")
    if missing:
        print("not found in DWARF (unused or renamed): " + ", ".join(missing))
    if data_mode:
        mismatches += check_data(objs)
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
