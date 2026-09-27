"""Check that gcc lays out the shared game structs exactly as Watcom did (docs/types.md).

    python port/tools/check_layouts.py [--data] [build/port]

Reads DWARF of every compiled historical unit (objdump --dwarf=info) and compares, for each
struct catalogued in docs/types.md, the byte size and every listed member offset. The
catalogue layouts are PROVEN by the byte-exact rebuild, so any difference is a port bug in
the compile flags (packing, bit-field allocation, enum size). --data also compares, per
unit, the spacing of consecutive initialized globals and BSS globals with the original
object 3 addresses (manifest.json symbols): overlays need Watcom's packed _DATA/_BSS
orders. Exit 1 on a mismatch.
"""
from __future__ import annotations

import re
import subprocess
import sys
import tempfile
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


def check_bss(objs, nm="nm"):
    """Compare each unit's public BSS order and spacing with the original LE object 3."""
    import json
    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    bss_start = int(manifest["le"]["objects"][2]["init"])
    symbols = manifest["symbols"]
    original = {
        name: int(value.split(":", 1)[1], 16)
        for name, value in symbols.items()
        if value.startswith("3:") and int(value.split(":", 1)[1], 16) >= bss_start
    }
    pairs = bad = 0
    for obj in objs:
        text = subprocess.run([nm, "-n", str(obj)], capture_output=True, text=True).stdout
        current = {}
        for line in text.splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[1] in "Bb" and parts[2].startswith("_"):
                name = parts[2][1:]
                if name in original:
                    current[name] = int(parts[0], 16)
        expected = sorted(current, key=lambda name: original[name])
        for a, b in zip(expected, expected[1:]):
            pairs += 1
            got = current[b] - current[a]
            want = original[b] - original[a]
            if got != want:
                bad += 1
                if bad <= 20:
                    print(f"BSS {obj.name.replace('.c.obj', '')}: {a} -> {b}: "
                          f"gcc {got:+#x}, original {want:+#x}")
    print(f"{pairs} adjacent BSS pairs compared with the original, {bad} differ")
    return bad


def check_const(objs, objdump="objdump", objcopy="objcopy"):
    """Verify packed writable CONST sections against the original LE object bytes."""
    sys.path.insert(0, str(ROOT / "port" / "tools"))
    import gcc_pack_data

    _const, const_limit, ranges, _targets, _symbols, _fixups, original_objects, _locations = \
        gcc_pack_data.original_const_layout()
    original3 = original_objects[2]
    checked = bad = 0
    aliases_checked = 0
    packed_ranges = []
    for obj in objs:
        packed = obj.with_suffix(".packed.s")
        if not packed.exists():
            continue
        text = packed.read_text(encoding="utf-8", errors="replace")
        markers = []
        current = None
        section_name = None
        cursor = 0
        marker_offsets_bad = False
        alias_bad = False
        for line in text.splitlines():
            section = re.match(r"^\s*\.section\s+([^,\s]+)", line)
            if section:
                section_name = section.group(1)
                current = None
                continue
            marker = re.match(r"^# KE_CONST_SECTION 3 (0x[0-9a-f]+) (0x[0-9a-f]+)$", line)
            if marker:
                current = (int(marker.group(1), 16), int(marker.group(2), 16))
                cursor = 0
                markers.append((current, section_name))
                packed_ranges.append((current[0], current[1], obj.name, section_name))
                continue
            if current is None:
                continue
            label = re.match(r"^# KE_CONST_LABEL 3 (0x[0-9a-f]+) (.+)$", line)
            if label:
                origin = int(label.group(1), 16)
                expected = origin - current[0]
                if cursor != expected:
                    marker_offsets_bad = True
                    print(f"CONST {obj.name}: label {label.group(2)} at packed +{cursor:#x}, "
                          f"original offset {origin:#x} requires +{expected:#x}")
                continue
            data = re.match(r"^\s*\.byte\s+(.+)$", line)
            if data:
                cursor += len(data.group(1).split(","))

        lines = text.splitlines()
        for index, line in enumerate(lines):
            alias = re.match(r"^# KE_CONST_ALIAS 3 (0x[0-9a-f]+) (\S+) (\S+)$", line)
            if not alias:
                continue
            aliases_checked += 1
            offset = int(alias.group(1), 16)
            local_name, target_name = alias.group(2), alias.group(3)
            expected_target = f"__ke_original_const3_{offset:08x}"
            next_line = lines[index + 1].strip() if index + 1 < len(lines) else ""
            if (target_name != expected_target or offset >= const_limit or
                    not re.fullmatch(rf"\.set\s+{re.escape(local_name)}\s*,\s*{re.escape(target_name)}", next_line)):
                alias_bad = True
                print(f"CONST {obj.name}: alias {local_name} does not resolve to original offset {offset:#x}")

        if not markers and not alias_bad:
            continue
        unit_bad = marker_offsets_bad or alias_bad
        section_text = subprocess.run([objdump, "-h", str(obj)], capture_output=True, text=True).stdout
        section_lines = section_text.splitlines()
        for (start, end), name in markers:
            checked += 1
            expected_name = f".data$KECONST3${start:08x}"
            if name != expected_name:
                unit_bad = True
                print(f"CONST {obj.name}: subsection {name!r}, expected original-order key {expected_name}")
            writable = False
            for index, line in enumerate(section_lines):
                header = re.match(r"^\s*\d+\s+(\S+)\s+", line)
                if name and header and header.group(1) == name:
                    flags = " ".join(section_lines[index + 1:index + 3])
                    writable = "DATA" in flags and "READONLY" not in flags
                    break
            if not writable:
                unit_bad = True
                print(f"CONST {obj.name}: {name or '.data$KECONST3'} is missing or not writable")
            expected = original3[start:end]
            try:
                with tempfile.TemporaryDirectory() as td:
                    dumped = Path(td) / "const.bin"
                    result = subprocess.run(
                        [objcopy, "--dump-section", f"{name}={dumped}", str(obj)],
                        capture_output=True, text=True,
                    )
                    if result.returncode:
                        unit_bad = True
                        print(f"CONST {obj.name}: cannot extract {name}: {result.stderr.strip()}")
                        continue
                    actual = dumped.read_bytes()
            except OSError as error:
                unit_bad = True
                print(f"CONST {obj.name}: {error}")
                continue
            if end > len(original3) or actual != expected:
                unit_bad = True
                mismatch = next((i for i, (a, b) in enumerate(zip(actual, expected)) if a != b),
                                min(len(actual), len(expected)))
                print(f"CONST {obj.name}: original [{start:#x},{end:#x}) is {len(expected)} bytes; "
                      f"{name} is {len(actual)} bytes, first mismatch +{mismatch:#x}")
        if unit_bad:
            bad += 1

    expected_ranges = [(0, const_limit)]
    actual_ranges = sorted((start, end) for start, end, _obj, _section in packed_ranges)
    if actual_ranges != expected_ranges:
        bad += 1
        missing = sorted(set(expected_ranges) - set(actual_ranges))
        extra = sorted(set(actual_ranges) - set(expected_ranges))
        print(f"CONST global ranges differ: missing {missing[:8]}, extra {extra[:8]}")
    else:
        cursor = expected_ranges[0][0] if expected_ranges else 0
        for start, end in expected_ranges:
            if start != cursor:
                bad += 1
                print(f"CONST global order has a gap/overlap at {cursor:#x}: next range starts {start:#x}")
            cursor = end
        if cursor != const_limit:
            bad += 1
            print(f"CONST global order ends at {cursor:#x}, expected {const_limit:#x}")
    print(f"{checked} original CONST range checked byte-for-byte and by label offset; "
          f"{aliases_checked} cross-unit labels checked; {bad} differ")
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
        mismatches += check_bss(objs)
        mismatches += check_const(objs)
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
