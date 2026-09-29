"""Compare the data layout of the ILP32 game world objects with the Windows i686 objects.

    python port/android/tools/check_ilp32_layout.py --abi arm64-v8a|x86_64 \
        [--android-objs DIR] [--windows-objs DIR]

The Windows objects (build/port, i686 gcc + port/tools/gcc_pack_data.py) have the layout
that port/tools/check_layouts.py --data proves against the original KE.EXE. For every
historical unit this tool requires the ILP32 object (port/android/tools/ilp32_world.py) to
define the same data symbols, in the same section kind (initialized data / zero data), at
the same offsets from the section start, with the same bytes where they are not relocated.
Exit status 1 on any difference.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
NDK_BIN = None


def tool(name):
    return str(Path(NDK_BIN) / (name + ".exe")) if NDK_BIN else name


def symbols(obj: Path, elf: bool):
    """{name: (kind, section, value, size)} for data/bss symbols (kind 'd' or 'b')."""
    out = {}
    nm = tool("llvm-nm")
    text = subprocess.run([nm, "--defined-only", "-S" if elf else "--defined-only",
                           str(obj)] if elf else [nm, "--defined-only", str(obj)],
                          capture_output=True, text=True).stdout
    for line in text.splitlines():
        parts = line.split()
        if elf and len(parts) == 4:
            value, size, kind, name = parts
        elif not elf and len(parts) == 3:
            value, kind, name = parts
            size = "0"
            if name.startswith("_"):
                name = name[1:]
        else:
            continue
        if kind.lower() not in "db":
            continue
        out[name] = (kind.lower(), int(value, 16), int(size, 16))
    return out


def section_of(obj: Path, elf: bool):
    """{symbol: section name} via llvm-objdump -t."""
    text = subprocess.run([tool("llvm-objdump"), "-t", str(obj)], capture_output=True,
                          text=True).stdout
    result = {}
    for line in text.splitlines():
        if elf:
            m = re.match(r"^[0-9a-f]+\s+.{7}\s+(\S+)\s+[0-9a-f]+\s+(?:\.hidden\s+)?(\S+)$", line)
            if m:
                result[m.group(2)] = m.group(1)
        else:
            m = re.match(r"^\[\s*\d+\]\(sec\s+(-?\d+)\).*0x[0-9a-f]+\s+(\S+)$", line)
            if m:
                name = m.group(2)
                result[name[1:] if name.startswith("_") else name] = m.group(1)
    return result


def section_bytes(obj: Path, section: str):
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "s.bin"
        r = subprocess.run([tool("llvm-objcopy"), "--dump-section", f"{section}={out}", str(obj),
                            str(Path(td) / "copy.o")], capture_output=True, text=True)
        if r.returncode or not out.exists():
            return None
        return out.read_bytes()


def reloc_offsets(obj: Path, section: str):
    """Offsets of relocations applied to `section` (COFF or ELF)."""
    text = subprocess.run([tool("llvm-objdump"), "-r", str(obj)], capture_output=True,
                          text=True).stdout
    offsets, cur = set(), None
    for line in text.splitlines():
        m = re.match(r"^RELOCATION RECORDS FOR \[(.+)\]:", line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"^([0-9a-f]{8,16})\s+(\S+)\s+(\S+)", line)
        if m and cur in (section, ".rela" + section, ".rel" + section):
            offsets.add(int(m.group(1), 16))
    return offsets


def self_reloc_slots(obj: Path):
    """Offsets (per section) of the 32-bit slots listed in the object's ke32_relocs table."""
    text = subprocess.run([tool("llvm-objdump"), "-r", str(obj)],
                          capture_output=True, text=True).stdout
    slots = []
    inside = False
    for line in text.splitlines():
        if line.startswith("RELOCATION RECORDS FOR"):
            inside = "[ke32_relocs]" in line
            continue
        if not inside:
            continue
        m = re.match(r"^([0-9a-f]{8,16})\s+R_\S+\s+(\S+?)(?:\+0x([0-9a-f]+))?$", line)
        if m and int(m.group(1), 16) % 16 == 0:
            slots.append((m.group(2), int(m.group(3) or "0", 16)))
    return slots


SECTION_PAIRS = [(".data", ".data"), (".data$KECONST3$00000000", ".data.keconst3_00000000"),
                 (".data$KECONST1", ".data.keconst1")]


def compare_contents(unit, wobj, aobj):
    bad = 0
    slots = self_reloc_slots(aobj)
    for wname, aname in SECTION_PAIRS:
        wdata = section_bytes(wobj, wname)
        adata = section_bytes(aobj, aname)
        wdata = wdata or b""
        adata = adata or b""
        if not wdata and not adata:
            continue
        # COFF rounds a section to its alignment; the tail must be zero padding
        if len(wdata) > len(adata) and not any(wdata[len(adata):]) and len(wdata) - len(adata) < 4:
            wdata = wdata[:len(adata)]
        if len(wdata) != len(adata):
            print(f"{unit}: section {wname}/{aname} sizes "
                  f"{None if wdata is None else len(wdata)} / {None if adata is None else len(adata)}")
            bad += 1
            continue
        mask = set()
        for off in reloc_offsets(wobj, wname):
            mask.update(range(off, off + 4))
        aslots = {off for sec, off in slots if sec == aname}
        for off in aslots:
            if adata[off:off + 4] != bytes(4):
                print(f"{unit}: {aname}+{off:#x} self-relocation slot is not zero")
                bad += 1
        wrel = {off for off in reloc_offsets(wobj, wname)}
        if wrel != aslots:
            print(f"{unit}: {wname} relocation sites differ: windows-only "
                  f"{sorted(hex(o) for o in wrel - aslots)[:8]}, ilp32-only "
                  f"{sorted(hex(o) for o in aslots - wrel)[:8]}")
            bad += 1
        diff = [i for i in range(len(wdata)) if i not in mask and wdata[i] != adata[i]]
        if diff:
            print(f"{unit}: {wname} content differs at {len(diff)} bytes, first +{diff[0]:#x}")
            bad += 1
    return bad


def record_layouts(clang: str, abi: str, src: Path):
    """{struct: (size, {member: (offset, is_bitfield)})} from clang -fdump-record-layouts."""
    sys.path.insert(0, str(ROOT / "port" / "android" / "tools"))
    import ilp32_world
    resource = subprocess.run([clang, "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    cmd = [clang, f"--target={ilp32_world.ILP32_TARGET[abi]}", "-std=gnu89", "-fsyntax-only",
           "-fpack-struct=1", "-mms-bitfields", "-funsigned-char", "-nostdinc",
           "-I", str(ROOT / "port/android/ilp32/include"), "-I", str(ROOT / "port/include/watcom"),
           "-isystem", str(Path(resource) / "include"),
           "-include", str(ROOT / "port/include/watcom/watcom_compat.h"), "-w",
           "-Xclang", "-fdump-record-layouts", str(src)]
    protos = ROOT / "port/android/ilp32/include" / f"ke_clang_{src.stem}.h"
    if protos.exists():
        cmd[-1:-1] = ["-include", str(protos)]
    text = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout
    layouts, cur, members = {}, None, None
    for line in text.splitlines():
        m = re.match(r"^\s+0 \| struct (\w+)$", line)
        if m and cur is None:
            cur, members = m.group(1), {}
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+\| \[sizeof=(\d+)", line)
        if m:
            layouts.setdefault(cur, (int(m.group(1)), members))
            cur = None
            continue
        m = re.match(r"^\s+(\d+)(?::(\d+)-\d+)? \|   (?!  )(.+?) (\w+)$", line)
        if m:
            members[m.group(4)] = (int(m.group(1)), m.group(2) is not None)
    return layouts


def check_structs(clang: str, abi: str):
    sys.path.insert(0, str(ROOT / "port" / "tools"))
    import check_layouts
    cat = check_layouts.catalogue()
    checked = bad = 0
    seen = set()
    for src in sorted((ROOT / "src").glob("*.c")):
        for name, (size, members) in record_layouts(clang, abi, src).items():
            if name not in cat:
                continue
            want = cat[name]
            checked += 1
            seen.add(name)
            problems = []
            if size != want["size"]:
                problems.append(f"size {size} != {want['size']}")
            for member, off in want["members"].items():
                if member in members:
                    got, bits = members[member]
                    if not ((off <= got < off + 4) if bits else got == off):
                        problems.append(f"{member} at {got:#x} != {off:#x}")
            if problems:
                bad += 1
                print(f"STRUCT {name} in {src.name}: " + "; ".join(problems))
    print(f"{abi}: {checked} struct definitions checked against docs/types.md "
          f"({len(seen)}/{len(cat)} catalogued types seen), {bad} mismatches")
    return bad


def main(argv):
    global NDK_BIN
    ap = argparse.ArgumentParser()
    ap.add_argument("--abi", required=True)
    ap.add_argument("--android-objs")
    ap.add_argument("--windows-objs", default=str(ROOT / "build/port/CMakeFiles/ke_game.dir"))
    ap.add_argument("--ndk-bin", required=True)
    args = ap.parse_args(argv)
    NDK_BIN = args.ndk_bin
    android_dir = Path(args.android_objs or ROOT / "build/ilpw" / args.abi)
    windows = {p.name.replace(".c.obj", ""): p for p in Path(args.windows_objs).rglob("*.c.obj")}
    if not windows:
        print(f"no Windows objects under {args.windows_objs}: build the Windows port first")
        return 2
    compared = bad = 0
    for unit, wobj in sorted(windows.items()):
        aobj = android_dir / f"{unit}.o"
        if not aobj.exists():
            print(f"MISSING {aobj}")
            bad += 1
            continue
        wsyms, asyms = symbols(wobj, False), symbols(aobj, True)
        wsec, asec = section_of(wobj, False), section_of(aobj, True)
        for name, (kind, value, _size) in sorted(wsyms.items(), key=lambda kv: kv[1][1]):
            if name.lstrip("_").startswith("ke_original_const") or name.startswith((".", "LC")):
                continue
            compared += 1
            if name not in asyms:
                bad += 1
                print(f"{unit}: {name} missing in the ILP32 object")
                continue
            akind, avalue, _asize = asyms[name]
            ws = wsec.get(name, "?")
            as_ = asec.get(name, "?")
            if akind != kind or avalue != value:
                bad += 1
                print(f"{unit}: {name}: windows {kind} +{value:#x} ({ws}), "
                      f"ilp32 {akind} +{avalue:#x} ({as_})")
        bad += compare_contents(unit, wobj, aobj)
        extra = sorted(n for n in asyms if n not in wsyms and not n.startswith(".") and
                       not n.startswith("__ke_original_const"))
        if extra:
            print(f"{unit}: only in the ILP32 object: {' '.join(extra[:10])}")
    bad += check_structs(str(Path(NDK_BIN) / "clang.exe"), args.abi)
    print(f"{args.abi}: {compared} data symbols compared with the Windows layout, {bad} differ")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
