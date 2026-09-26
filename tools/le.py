"""Minimal LE (Linear Executable, DOS/4GW flavour) reader.

Pure data: parses the MZ stub, LE header, object table, page map, fixups,
entry/name tables into plain dicts. No policy, no heuristics.

    python tools/le.py assets/KE.EXE            # JSON summary to stdout
    python tools/le.py assets/KE.EXE --fixups   # include every fixup record
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

HDR_FIELDS = [  # (offset, name, fmt)
    (0x02, "byte_order", "B"), (0x03, "word_order", "B"), (0x04, "format_level", "I"),
    (0x08, "cpu_type", "H"), (0x0A, "os_type", "H"), (0x0C, "module_version", "I"),
    (0x10, "module_flags", "I"), (0x14, "num_pages", "I"), (0x18, "eip_object", "I"),
    (0x1C, "eip", "I"), (0x20, "esp_object", "I"), (0x24, "esp", "I"),
    (0x28, "page_size", "I"), (0x2C, "last_page_size", "I"), (0x30, "fixup_section_size", "I"),
    (0x34, "fixup_section_checksum", "I"), (0x38, "loader_section_size", "I"),
    (0x3C, "loader_section_checksum", "I"), (0x40, "object_table_off", "I"),
    (0x44, "num_objects", "I"), (0x48, "object_page_table_off", "I"),
    (0x4C, "object_iter_pages_off", "I"), (0x50, "resource_table_off", "I"),
    (0x54, "num_resources", "I"), (0x58, "resident_name_table_off", "I"),
    (0x5C, "entry_table_off", "I"), (0x60, "module_directives_off", "I"),
    (0x64, "num_module_directives", "I"), (0x68, "fixup_page_table_off", "I"),
    (0x6C, "fixup_record_table_off", "I"), (0x70, "import_module_table_off", "I"),
    (0x74, "num_import_modules", "I"), (0x78, "import_proc_table_off", "I"),
    (0x7C, "per_page_checksum_off", "I"), (0x80, "data_pages_off", "I"),
    (0x84, "num_preload_pages", "I"), (0x88, "nonresident_name_table_off", "I"),
    (0x8C, "nonresident_name_table_len", "I"), (0x90, "nonresident_name_checksum", "I"),
    (0x94, "auto_data_object", "I"), (0x98, "debug_info_off", "I"),
    (0x9C, "debug_info_len", "I"), (0xA0, "instance_preload", "I"),
    (0xA4, "instance_demand", "I"), (0xA8, "heap_size", "I"),
]

OBJ_FLAGS = {0x1: "R", 0x2: "W", 0x4: "X", 0x8: "RES", 0x10: "DISCARD", 0x20: "SHARED",
             0x40: "PRELOAD", 0x80: "INVALID", 0x100: "ZEROFILL", 0x200: "RESIDENT",
             0x1000: "ALIAS16", 0x2000: "BIG", 0x4000: "CONFORM", 0x8000: "IOPL"}

SRC_TYPES = {0: "byte", 2: "sel16", 3: "ptr16:16", 5: "off16", 6: "ptr16:32", 7: "off32", 8: "rel32"}


@dataclass
class Fixup:
    page: int              # 0-based logical page index
    src_type: int          # low nibble of source byte
    src_flags: int         # full source byte
    tgt_flags: int
    src_offsets: list      # offsets within the page (signed 16-bit)
    target: dict           # kind + fields
    rec_off: int           # offset of the record in the file
    raw: bytes = field(repr=False, default=b"")

    def to_json(self):
        return {"page": self.page, "type": SRC_TYPES.get(self.src_type, hex(self.src_type)),
                "src": self.src_flags, "tf": self.tgt_flags, "offs": self.src_offsets,
                "target": self.target, "raw": self.raw.hex()}


class LE:
    def __init__(self, path):
        self.path = Path(path)
        self.data = d = self.path.read_bytes()
        if d[:2] != b"MZ":
            raise ValueError("not MZ")
        self.le_off = struct.unpack_from("<I", d, 0x3C)[0]
        if d[self.le_off:self.le_off + 2] != b"LE":
            raise ValueError("no LE header at e_lfanew")
        self.hdr = {n: struct.unpack_from("<" + f, d, self.le_off + o)[0] for o, n, f in HDR_FIELDS}
        self._objects()
        self._pages()
        self._fixups()

    # --- helpers -------------------------------------------------------------
    def _u(self, fmt, off):
        return struct.unpack_from("<" + fmt, self.data, off)[0]

    def _objects(self):
        h, base = self.hdr, self.le_off + self.hdr["object_table_off"]
        self.objects = []
        for i in range(h["num_objects"]):
            vsize, reloc, flags, pti, npages, res = struct.unpack_from("<6I", self.data, base + 24 * i)
            self.objects.append({"n": i + 1, "vsize": vsize, "base": reloc, "flags": flags,
                                 "flag_names": [v for k, v in OBJ_FLAGS.items() if flags & k],
                                 "page_index": pti, "num_pages": npages, "reserved": res})

    def _pages(self):
        h = self.hdr
        base = self.le_off + h["object_page_table_off"]
        self.page_map = []
        for i in range(h["num_pages"]):
            b = self.data[base + 4 * i: base + 4 * i + 4]
            self.page_map.append({"num": (b[0] << 16) | (b[1] << 8) | b[2], "flags": b[3]})

    def page_file_range(self, page_index):
        """File range of 0-based logical page (LE: physical pages are sequential)."""
        h = self.hdr
        num = self.page_map[page_index]["num"]
        start = h["data_pages_off"] + (num - 1) * h["page_size"]
        size = h["last_page_size"] if num == h["num_pages"] else h["page_size"]
        return start, size

    def object_bytes(self, obj):
        """Initialized bytes of an object (concatenated pages; may be shorter than vsize)."""
        out = bytearray()
        for p in range(obj["page_index"] - 1, obj["page_index"] - 1 + obj["num_pages"]):
            s, n = self.page_file_range(p)
            out += self.data[s:s + n]
        return bytes(out)

    def page_object(self, page_index):
        for o in self.objects:
            if o["page_index"] - 1 <= page_index < o["page_index"] - 1 + o["num_pages"]:
                return o, (page_index - (o["page_index"] - 1)) * self.hdr["page_size"]
        raise KeyError(page_index)

    def _fixups(self):
        h, d = self.hdr, self.data
        fpt = self.le_off + h["fixup_page_table_off"]
        frt = self.le_off + h["fixup_record_table_off"]
        offs = [self._u("I", fpt + 4 * i) for i in range(h["num_pages"] + 1)]
        self.fixup_page_offsets = offs
        self.fixups = []
        for page in range(h["num_pages"]):
            p, end = frt + offs[page], frt + offs[page + 1]
            while p < end:
                start = p
                src, tf = d[p], d[p + 1]
                p += 2
                if src & 0x20:
                    cnt = d[p]; p += 1
                    srcoffs = None
                else:
                    srcoffs = [struct.unpack_from("<h", d, p)[0]]; p += 2
                kind = tf & 3
                tgt = {}
                if kind == 0:  # internal reference
                    if tf & 0x40:
                        tgt["obj"] = self._u("H", p); p += 2
                    else:
                        tgt["obj"] = d[p]; p += 1
                    if (src & 0xF) != 2:
                        if tf & 0x10:
                            tgt["off"] = self._u("I", p); p += 4
                        else:
                            tgt["off"] = self._u("H", p); p += 2
                    tgt["kind"] = "internal"
                elif kind in (1, 2):
                    if tf & 0x40:
                        tgt["mod"] = self._u("H", p); p += 2
                    else:
                        tgt["mod"] = d[p]; p += 1
                    if kind == 1:
                        if tf & 0x80:
                            tgt["ord"] = d[p]; p += 1
                        elif tf & 0x10:
                            tgt["ord"] = self._u("I", p); p += 4
                        else:
                            tgt["ord"] = self._u("H", p); p += 2
                    else:
                        if tf & 0x10:
                            tgt["name_off"] = self._u("I", p); p += 4
                        else:
                            tgt["name_off"] = self._u("H", p); p += 2
                    tgt["kind"] = "import_ord" if kind == 1 else "import_name"
                else:
                    if tf & 0x40:
                        tgt["entry"] = self._u("H", p); p += 2
                    else:
                        tgt["entry"] = d[p]; p += 1
                    tgt["kind"] = "entry"
                if tf & 0x04:  # additive
                    if tf & 0x20:
                        tgt["add"] = self._u("I", p); p += 4
                    else:
                        tgt["add"] = self._u("H", p); p += 2
                if srcoffs is None:
                    srcoffs = [struct.unpack_from("<h", d, p + 2 * i)[0] for i in range(cnt)]
                    p += 2 * cnt
                self.fixups.append(Fixup(page, src & 0xF, src, tf, srcoffs, tgt, start, d[start:p]))
            if p != end:
                raise ValueError(f"fixup parse overrun on page {page}: {p:#x} != {end:#x}")

    def resolved_fixups(self):
        """Yield (src_obj, src_obj_offset, type, target dict) for every fixup source site."""
        for f in self.fixups:
            obj, pbase = self.page_object(f.page)
            for so in f.src_offsets:
                yield obj["n"], pbase + so, SRC_TYPES.get(f.src_type, hex(f.src_type)), f.target

    def names(self, off, limit=None):
        out, p = [], off
        while self.data[p]:
            n = self.data[p]
            name = self.data[p + 1:p + 1 + n].decode("latin-1")
            out.append({"name": name, "ord": self._u("H", p + 1 + n)})
            p += 3 + n
            if limit and p >= limit:
                break
        return out

    def summary(self, with_fixups=False):
        h = self.hdr
        s = {
            "file": self.path.name, "size": len(self.data),
            "sha256": hashlib.sha256(self.data).hexdigest(),
            "stub": {"size": self.le_off, "sha256": hashlib.sha256(self.data[:self.le_off]).hexdigest()},
            "le_off": self.le_off, "header": h, "objects": [],
            "resident_names": self.names(self.le_off + h["resident_name_table_off"]),
            "nonresident_names": (self.names(h["nonresident_name_table_off"])
                                  if h["nonresident_name_table_off"] else []),
            "fixup_count": len(self.fixups),
            "fixup_sites": sum(len(f.src_offsets) for f in self.fixups),
        }
        end_pages = h["data_pages_off"] + (h["num_pages"] - 1) * h["page_size"] + h["last_page_size"]
        s["data_pages_end"] = end_pages
        s["trailing_bytes"] = len(self.data) - end_pages
        for o in self.objects:
            b = self.object_bytes(o)
            s["objects"].append({**o, "init_size": len(b), "sha256": hashlib.sha256(b).hexdigest()})
        hist = {}
        for f in self.fixups:
            key = f"{SRC_TYPES.get(f.src_type, hex(f.src_type))}/{f.target['kind']}/src{f.src_flags:02x}/tf{f.tgt_flags:02x}"
            hist[key] = hist.get(key, 0) + len(f.src_offsets)
        s["fixup_histogram"] = dict(sorted(hist.items()))
        if with_fixups:
            s["fixups"] = [f.to_json() for f in self.fixups]
        return s


def main(argv):
    path = argv[1]
    le = LE(path)
    json.dump(le.summary("--fixups" in argv), sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main(sys.argv)
