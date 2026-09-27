"""Export KE.EXE's LE image for the native oracle harness (port/oracle).

    python port/tools/le_export.py [assets/KE.EXE] [--out build/port/oracle]

Writes (build output, never committed - it is the original program):
  ke_image.bin    objects + resolved fixup sites (format below)
  ke_symbols.txt  "name obj offset" for every manifest symbol (hex offsets)

ke_image.bin, little endian:
  "KEIM" u32 version=1  u32 object_count
  per object: u32 number, u32 link_base, u32 virtual_size, u32 init_size, bytes[init_size]
  u32 fixup_count
  per fixup:  u8 src_object, u8 kind (7 = off32, 2 = sel16), u16 0, u32 src_offset,
              u8 tgt_object, u8 0, u16 0, u32 tgt_offset
The KE.EXE identity is checked against manifest.json before anything is written.
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from le import LE  # noqa: E402

KIND = {"off32": 7, "sel16": 2}


def main(argv):
    exe = Path(argv[0]) if argv and not argv[0].startswith("--") else ROOT / "assets" / "KE.EXE"
    out = ROOT / "build" / "port" / "oracle"
    if "--out" in argv:
        out = Path(argv[argv.index("--out") + 1])
    manifest = json.loads((ROOT / "manifest.json").read_text())
    want = manifest["original"]["KE.EXE"]["sha256"]
    data = exe.read_bytes()
    got = hashlib.sha256(data).hexdigest()
    if got != want:
        print(f"refusing {exe}: sha256 {got} != manifest {want}")
        return 1
    le = LE(exe)
    blob = bytearray(b"KEIM" + struct.pack("<II", 1, len(le.objects)))
    for o in le.objects:
        init = le.object_bytes(o)[: o["vsize"]]
        blob += struct.pack("<IIII", o["n"], o["base"], o["vsize"], len(init)) + init
    sites = []
    for obj, off, typ, tgt in le.resolved_fixups():
        if typ not in KIND or tgt.get("kind") != "internal":
            print(f"unsupported fixup {typ} {tgt} at {obj}:{off:#x}")
            return 1
        if off < 0:
            continue
        sites.append((obj, KIND[typ], off, tgt["obj"], tgt.get("off", 0)))
    sites = sorted(set(sites))
    blob += struct.pack("<I", len(sites))
    for obj, kind, off, tobj, toff in sites:
        blob += struct.pack("<BBHIBBHI", obj, kind, 0, off, tobj, 0, 0, toff)
    out.mkdir(parents=True, exist_ok=True)
    (out / "ke_image.bin").write_bytes(bytes(blob))
    lines = []
    for name, loc in sorted(manifest["symbols"].items()):
        if ":" not in loc:
            continue                      # absolute/linker symbols
        obj, off = loc.split(":")
        lines.append(f"{name} {obj} {off}")
    (out / "ke_symbols.txt").write_text("\n".join(lines) + "\n")
    print(f"wrote {out / 'ke_image.bin'} ({len(le.objects)} objects, {len(sites)} fixup sites) "
          f"and ke_symbols.txt ({len(lines)} symbols)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
