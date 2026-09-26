"""Minimal Intel OMF (16/32-bit, Watcom flavour) object and library reader.

Pure data: turns .OBJ/.LIB bytes into modules with segments (bytes + coverage),
fixups, publics, externs.  Unsupported records are kept and reported, never
silently dropped.  Handles Phar Lap "Easy OMF-386" modules (COMENT class 0xAA),
which carry 32-bit fields inside 16-bit record types.

    python tools/omf.py FILE.OBJ          # JSON summary of each module
    python tools/omf.py FILE.LIB --list   # module names + public names
"""
from __future__ import annotations

import json
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

LOC_NAMES = {0: "lobyte", 1: "off16", 2: "base16", 3: "ptr16:16", 4: "hibyte", 5: "off16l",
             9: "off32", 11: "ptr16:32", 13: "off32l"}
LOC_SIZE = {0: 1, 1: 2, 2: 2, 3: 4, 4: 1, 5: 2, 9: 4, 11: 6, 13: 4}


@dataclass
class Segment:
    name: str
    cls: str
    align: int
    combine: int
    size: int
    use32: bool
    data: bytearray = field(default_factory=bytearray)
    covered: bytearray = field(default_factory=bytearray)


@dataclass
class Fixup:
    seg: int            # 1-based segment index
    off: int            # offset within segment of the fixed-up field
    loc: int            # location type
    self_rel: bool
    frame: tuple        # (method, datum)
    target: tuple       # (method, datum)  method 0=seg,1=grp,2=ext,(|4 = no disp)
    disp: int

    def kind(self):
        return LOC_NAMES.get(self.loc, f"loc{self.loc}")

    def size(self):
        return LOC_SIZE.get(self.loc, 0)


class Module:
    def __init__(self):
        self.name = None
        self.lnames = [None]
        self.segments: list = [None]
        self.groups = [None]
        self.externs = [None]          # 1-based; entries are names
        self.publics = []              # (name, seg, off, local)
        self.comdefs = []
        self.fixups: list = []
        self.comments = []
        self.unsupported = {}
        self.easy = False
        self.records = 0

    def target_name(self, f: Fixup):
        m, d = f.target
        m &= 3
        if m == 0:
            return "seg:" + self.segments[d].name
        if m == 1:
            return "grp:" + self.groups[d][0]
        if m == 2:
            return self.externs[d]
        return f"frame{d}"

    def summary(self):
        return {
            "name": self.name,
            "segments": [{"i": i, "name": s.name, "class": s.cls, "align": s.align, "size": s.size,
                          "use32": s.use32} for i, s in enumerate(self.segments) if s],
            "groups": [g for g in self.groups if g],
            "externs": [e for e in self.externs if e],
            "publics": [{"name": n, "seg": self.segments[s].name if s else None, "off": o,
                         **({"local": True} if loc else {})} for n, s, o, loc in self.publics],
            "comdefs": self.comdefs,
            "fixups": len(self.fixups),
            "unsupported": {hex(k): v for k, v in self.unsupported.items()},
        }


class _R:
    def __init__(self, b, is32):
        self.b, self.p, self.is32 = b, 0, is32

    def u8(self):
        v = self.b[self.p]; self.p += 1; return v

    def u16(self):
        v = struct.unpack_from("<H", self.b, self.p)[0]; self.p += 2; return v

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.p)[0]; self.p += 4; return v

    def off(self):
        return self.u32() if self.is32 else self.u16()

    def idx(self):
        v = self.u8()
        if v & 0x80:
            v = ((v & 0x7F) << 8) | self.u8()
        return v

    def name(self):
        n = self.u8(); s = self.b[self.p:self.p + n].decode("latin-1"); self.p += n; return s

    def more(self):
        return self.p < len(self.b)


def _expand_lidata(r: _R):
    rep = r.off()
    blocks = r.u16()
    if blocks == 0:
        n = r.u8(); content = r.b[r.p:r.p + n]; r.p += n
        return bytes(content) * rep
    out = b"".join(_expand_lidata(r) for _ in range(blocks))
    return out * rep


def _clen(r: _R):
    v = r.u8()
    if v <= 0x80:
        return v
    if v == 0x81:
        return r.u16()
    if v == 0x84:
        return r.u16() | (r.u8() << 16)
    if v == 0x88:
        return r.u32()
    raise ValueError(f"bad COMDEF length prefix {v:#x}")


def parse_module(data: bytes, pos: int = 0):
    """Parse one module starting at pos; returns (Module, end_pos)."""
    m = Module()
    last = None  # (seg_index, offset) of last LEDATA/LIDATA
    threads = {("T", i): None for i in range(4)}
    threads.update({("F", i): None for i in range(4)})
    while pos < len(data):
        typ = data[pos]
        ln = struct.unpack_from("<H", data, pos + 1)[0]
        body = data[pos + 3:pos + 3 + ln - 1]
        pos += 3 + ln
        m.records += 1
        r = _R(body, bool(typ & 1) or m.easy)
        if typ in (0x80, 0x82):
            m.name = r.name()
        elif typ == 0x88:
            m.comments.append(body[:2].hex() + ":" + body[2:].decode("latin-1", "replace"))
            if body[1:2] == b"\xaa":
                m.easy = True
        elif typ in (0x96, 0xCA):
            while r.more():
                m.lnames.append(r.name())
        elif typ in (0x98, 0x99):
            acbp = r.u8()
            align, comb, big, use32 = acbp >> 5, (acbp >> 2) & 7, (acbp >> 1) & 1, acbp & 1
            if align == 0:
                r.u16(); r.u8()
            size = r.off()
            if big:
                size = 0x100000000 if r.is32 else 0x10000
            nm, cl = r.idx(), r.idx(); r.idx()
            if m.easy:
                use32 = 1
            alloc = size if size < (1 << 24) else 0
            m.segments.append(Segment(m.lnames[nm], m.lnames[cl], align, comb, size, bool(use32),
                                      bytearray(alloc), bytearray(alloc)))
        elif typ == 0x9A:
            nm = m.lnames[r.idx()]
            members = []
            while r.more():
                r.u8(); members.append(m.segments[r.idx()].name)
            m.groups.append((nm, members))
        elif typ in (0x8C, 0xB4, 0xB5):
            while r.more():
                m.externs.append(r.name()); r.idx()
        elif typ in (0xB0, 0xB8):
            while r.more():
                n = r.name(); r.idx(); dt = r.u8()
                if dt == 0x61:
                    cnt = _clen(r); sz = _clen(r) * cnt
                else:
                    sz = _clen(r)
                m.externs.append(n); m.comdefs.append({"name": n, "type": dt, "size": sz})
        elif typ in (0x90, 0x91, 0xB6, 0xB7):
            gi, si = r.idx(), r.idx()
            if si == 0:
                r.u16()
            while r.more():
                n = r.name(); o = r.off(); r.idx()
                m.publics.append((n, si, o, typ in (0xB6, 0xB7)))
        elif typ in (0xA0, 0xA1):
            si = r.idx(); off = r.off(); chunk = body[r.p:]
            seg = m.segments[si]
            seg.data[off:off + len(chunk)] = chunk
            seg.covered[off:off + len(chunk)] = b"\x01" * len(chunk)
            last = (si, off)
        elif typ in (0xA2, 0xA3):
            si = r.idx(); off = r.off()
            chunk = b""
            while r.more():
                chunk += _expand_lidata(r)
            seg = m.segments[si]
            seg.data[off:off + len(chunk)] = chunk
            seg.covered[off:off + len(chunk)] = b"\x01" * len(chunk)
            last = (si, off)
        elif typ in (0x9C, 0x9D):
            while r.more():
                b0 = r.u8()
                if not b0 & 0x80:  # THREAD subrecord
                    meth = (b0 >> 2) & 7; num = b0 & 3
                    if b0 & 0x40:
                        threads[("F", num)] = (meth, r.idx() if meth < 3 else None)
                    else:
                        threads[("T", num)] = (meth, r.idx())
                    continue
                b1 = r.u8()
                self_rel = not (b0 & 0x40)
                loc = (b0 >> 2) & 0xF
                doff = ((b0 & 3) << 8) | b1
                fd = r.u8()
                if fd & 0x80:
                    frame = threads[("F", (fd >> 4) & 3)]
                else:
                    fm = (fd >> 4) & 7
                    frame = (fm, r.idx() if fm < 3 else None)
                if fd & 0x08:
                    t = threads[("T", fd & 3)]
                    target = ((t[0] & 3) | (fd & 4), t[1])
                else:
                    target = (fd & 7, r.idx())
                disp = 0 if target[0] & 4 else r.off()
                si, base = last
                m.fixups.append(Fixup(si, base + doff, loc, self_rel, frame, target, disp))
        elif typ in (0x8A, 0x8B):
            return m, pos
        elif typ in (0x94, 0x95):
            pass  # LINNUM: no code effect
        else:
            m.unsupported[typ] = m.unsupported.get(typ, 0) + 1
    raise ValueError("no MODEND")


def parse_object(data: bytes):
    mods, pos = [], 0
    while pos < len(data) and data[pos] != 0x00:
        m, pos = parse_module(data, pos)
        mods.append(m)
    return mods


def parse_library(data: bytes):
    """Return list of (file_offset, Module) for a Microsoft/Watcom .LIB."""
    if data[0] != 0xF0:
        raise ValueError("not a LIB")
    page = struct.unpack_from("<H", data, 1)[0] + 3
    dict_off = struct.unpack_from("<I", data, 3)[0]
    out, pos = [], page
    while pos < dict_off and data[pos] != 0xF1:
        start = pos
        m, pos = parse_module(data, pos)
        out.append((start, m))
        pos = (pos + page - 1) // page * page
    return out


def load(path):
    data = Path(path).read_bytes()
    if data[0] == 0xF0:
        return [m for _, m in parse_library(data)]
    return parse_object(data)


def main(argv):
    mods = load(argv[1])
    if "--list" in argv:
        for m in mods:
            print(m.name, " ".join(n for n, *_ in m.publics))
        return
    json.dump([m.summary() for m in mods], sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main(sys.argv)
