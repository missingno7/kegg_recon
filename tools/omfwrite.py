"""Minimal Intel OMF-386 object writer (the record subset WLINK 10.0 needs).

Used by tools/image.py to express unrecovered regions of KE.EXE as explicit RAW DEBT objects:
original bytes in LEDATA records plus FIXUPP records for every LE fixup, publics for the entry points
other objects use and EXTDEFs for what the region references.  Pure data in, bytes out; the writer
does not decide what a region contains.

    from omfwrite import Obj
    o = Obj("raw_0010")
    flat = o.group("FLAT", [])
    t = o.segment("_TEXT", "CODE", align=1, size=len(code), data=code)
    o.public("f_10", t, 0)
    e = o.extern("strcpy")
    o.fixup(t, 0x21, "off32", target=("ext", e), frame=("grp", flat), self_rel=True)
    Path("raw.obj").write_bytes(o.to_bytes())

Record order: THEADR, COMENT, LNAMES, SEGDEF*, GRPDEF*, EXTDEF (in extern() call order), PUBDEF*,
then per segment the LEDATA chunks each followed by the FIXUPP records whose sites lie in the chunk
(subrecords in fixup() call order, one FIXUPP record per `fixup_group`), MODEND.

    python tools/omfwrite.py FILE.OBJ     # record-level listing (LEDATA ranges, FIXUPP site order)
"""
from __future__ import annotations

import struct
import sys
from dataclasses import dataclass, field

ALIGN = {"abs": 0, "byte": 1, "word": 2, "para": 3, "page": 4, "dword": 5}
LOC = {"lobyte": 0, "off16": 1, "base16": 2, "ptr16:16": 3, "hibyte": 4, "off16l": 5, "off32": 9,
       "ptr16:32": 11, "off32l": 13}
LOC_SIZE = {0: 1, 1: 2, 2: 2, 3: 4, 4: 1, 5: 2, 9: 4, 11: 6, 13: 4}
MAX_LEDATA = 1024


def _rec(typ: int, body: bytes) -> bytes:
    rec = bytes([typ]) + struct.pack("<H", len(body) + 1) + body
    return rec + bytes([(-sum(rec)) & 0xFF])


def _name(s: str) -> bytes:
    b = s.encode("latin-1")
    if len(b) > 255:
        raise ValueError(f"name too long: {s}")
    return bytes([len(b)]) + b


def _idx(i: int) -> bytes:
    if i < 0x80:
        return bytes([i])
    if i >= 0x8000:
        raise ValueError(f"index {i} too large")
    return bytes([0x80 | (i >> 8), i & 0xFF])


@dataclass
class Seg:
    name: str
    cls: str
    align: int
    size: int
    use32: bool
    combine: int
    data: bytes | None
    chunks: list = field(default_factory=list)  # [(start, end)] LEDATA boundaries


@dataclass
class Fix:
    seg: int
    off: int
    loc: int
    self_rel: bool
    target: tuple      # ("seg"|"grp"|"ext", index)
    frame: tuple       # ("seg"|"grp"|"ext", index) | ("target",) | ("loc",)
    disp: int
    group: int         # FIXUPP record key: fixups with equal key after one LEDATA share a record


class Obj:
    def __init__(self, name: str, comments=()):
        self.name = name
        self.lnames: list = []
        self.segs: list = []
        self.groups: list = []
        self.externs: list = []
        self.publics: list = []       # (name, seg, off)
        self.fixups: list = []
        self.comments = list(comments)  # (class byte, payload bytes)
        self.start = None             # (seg, off) program entry for MODEND

    # --- definitions ---------------------------------------------------------
    def _lname(self, s: str) -> int:
        if s not in self.lnames:
            self.lnames.append(s)
        return self.lnames.index(s) + 1

    def segment(self, name, cls, align=1, size=None, data=None, use32=True, combine=2, chunks=None) -> int:
        """Add a SEGDEF; returns its 1-based index. data=None -> no LEDATA (BSS/STACK)."""
        if isinstance(align, str):
            align = ALIGN[align]
        if size is None:
            size = len(data or b"")
        if data is not None and len(data) != size:
            raise ValueError("data length != size")
        self._lname(name); self._lname(cls)
        s = Seg(name, cls, align, size, use32, combine, bytes(data) if data is not None else None)
        s.chunks = list(chunks) if chunks else []
        self.segs.append(s)
        return len(self.segs)

    def group(self, name, segs) -> int:
        self._lname(name)
        self.groups.append((name, list(segs)))
        return len(self.groups)

    def extern(self, name) -> int:
        if name in self.externs:
            return self.externs.index(name) + 1
        self.externs.append(name)
        return len(self.externs)

    def public(self, name, seg, off):
        self.publics.append((name, seg, off))

    def fixup(self, seg, off, loc="off32", target=None, frame=("target",), self_rel=False, disp=0, group=0):
        loc = LOC[loc] if isinstance(loc, str) else loc
        self.fixups.append(Fix(seg, off, loc, self_rel, target, frame, disp, group))

    # --- serialisation ------------------------------------------------------
    def _chunks(self, si, seg):
        """LEDATA boundaries: caller-given cut points, else <=MAX_LEDATA pieces; a fixup field never
        straddles a chunk boundary."""
        cuts = sorted({c for c in (x for ab in seg.chunks for x in ab) if 0 < c < seg.size})
        fields = [(f.off, f.off + LOC_SIZE[f.loc]) for f in self.fixups if f.seg == si]
        out, a = [], 0
        bounds = cuts + [seg.size]
        for b in bounds:
            while b - a > MAX_LEDATA:
                c = a + MAX_LEDATA
                for fo, fe in fields:
                    if fo < c < fe:
                        c = fo
                out.append((a, c)); a = c
            if b > a:
                out.append((a, b)); a = b
        for fo, fe in fields:
            if not any(x <= fo and fe <= y for x, y in out):
                raise ValueError(f"fixup field {fo:#x}..{fe:#x} straddles LEDATA chunks in {seg.name}")
        return out

    def _fixupp(self, fl, base):
        body = bytearray()
        for f in fl:
            rel = f.off - base
            if not 0 <= rel < 1024:
                raise ValueError("fixup outside LEDATA")
            m = 0 if f.self_rel else 1
            body += bytes([0x80 | (m << 6) | (f.loc << 2) | (rel >> 8), rel & 0xFF])
            fk = f.frame[0]
            fmeth = {"seg": 0, "grp": 1, "ext": 2, "loc": 4, "target": 5}[fk]
            tk, ti = f.target
            tmeth = {"seg": 0, "grp": 1, "ext": 2}[tk]
            p = 1 if f.disp == 0 else 0
            body.append((fmeth << 4) | (p << 2) | tmeth)
            if fmeth < 3:
                body += _idx(f.frame[1])
            body += _idx(ti)
            if not p:
                body += struct.pack("<I", f.disp & 0xFFFFFFFF)
        return _rec(0x9D, bytes(body))

    def to_bytes(self) -> bytes:
        ov = self._lname("")  # SEGDEF overlay-name index: an empty LNAME, as Watcom emits
        out = bytearray(_rec(0x80, _name(self.name)))
        for cls, payload in self.comments:
            out += _rec(0x88, bytes([0x80, cls]) + payload)
        body = bytearray()
        for n in self.lnames:
            if len(body) + len(n) + 1 > 1000:
                out += _rec(0x96, bytes(body)); body = bytearray()
            body += _name(n)
        if body:
            out += _rec(0x96, bytes(body))
        for s in self.segs:
            acbp = (s.align << 5) | (s.combine << 2) | (1 if s.use32 else 0)
            out += _rec(0x99, bytes([acbp]) + struct.pack("<I", s.size) + _idx(self._lname(s.name))
                        + _idx(self._lname(s.cls)) + _idx(ov))
        for name, segs in self.groups:
            body = _idx(self._lname(name)) + b"".join(b"\xff" + _idx(i) for i in segs)
            out += _rec(0x9A, body)
        body = bytearray()
        for n in self.externs:
            if len(body) + len(n) + 2 > 1000:
                out += _rec(0x8C, bytes(body)); body = bytearray()
            body += _name(n) + b"\x00"
        if body:
            out += _rec(0x8C, bytes(body))
        # PUBDEF per segment (group index 0: WLINK takes the segment's group from GRPDEF)
        by_seg: dict = {}
        for n, si, off in self.publics:
            by_seg.setdefault(si, []).append((n, off))
        for si, lst in by_seg.items():
            body = bytearray(b"\x00" + _idx(si))
            for n, off in lst:
                item = _name(n) + struct.pack("<I", off) + b"\x00"
                if len(body) + len(item) > 1000:
                    out += _rec(0x91, bytes(body)); body = bytearray(b"\x00" + _idx(si))
                body += item
            out += _rec(0x91, bytes(body))
        for si, s in enumerate(self.segs, 1):
            if s.data is None:
                if any(f.seg == si for f in self.fixups):
                    raise ValueError(f"fixups in data-less segment {s.name}")
                continue
            for a, b in self._chunks(si, s):
                out += _rec(0xA1, _idx(si) + struct.pack("<I", a) + s.data[a:b])
                fl = [f for f in self.fixups if f.seg == si and a <= f.off < b]
                # one FIXUPP record per consecutive run of equal `group` keys, in call order
                run, key = [], None
                for f in fl:
                    if run and f.group != key:
                        out += self._fixupp(run, a); run = []
                    run.append(f); key = f.group
                    if len(run) >= 100:
                        out += self._fixupp(run, a); run = []
                if run:
                    out += self._fixupp(run, a)
        if self.start is not None:
            si, off = self.start
            # MODEND: main + start address, EFixDat F1? use frame=target(5), target seg(0) with disp
            body = bytes([0xC1, 0x50]) + _idx(si) + struct.pack("<I", off)
            out += _rec(0x8B, body)
        else:
            out += _rec(0x8B, b"\x00")
        return bytes(out)


# --- inspection ----------------------------------------------------------------
REC = {0x80: "THEADR", 0x88: "COMENT", 0x96: "LNAMES", 0x98: "SEGDEF", 0x99: "SEGDEF32", 0x9A: "GRPDEF",
       0x8C: "EXTDEF", 0x90: "PUBDEF", 0x91: "PUBDEF32", 0xA0: "LEDATA", 0xA1: "LEDATA32", 0x9C: "FIXUPP",
       0x9D: "FIXUPP32", 0x8A: "MODEND", 0x8B: "MODEND32", 0xB0: "COMDEF", 0xB4: "LEXTDEF", 0xB6: "LPUBDEF",
       0xB7: "LPUBDEF32", 0x94: "LINNUM", 0x95: "LINNUM32", 0xA2: "LIDATA", 0xA3: "LIDATA32"}


def records(d: bytes):
    """Yield (offset, type, body) for each record of one module."""
    p = 0
    while p < len(d):
        t = d[p]
        ln = struct.unpack_from("<H", d, p + 1)[0]
        yield p, t, d[p + 3:p + 2 + ln]
        p += 3 + ln
        if t in (0x8A, 0x8B):
            break


def fixupp_sites(t: int, body: bytes):
    """Data-record-relative sites of the FIXUP subrecords of one FIXUPP record, in record order."""
    q, out = 0, []
    wide = t & 1
    while q < len(body):
        b0 = body[q]
        if not b0 & 0x80:  # THREAD
            meth = (b0 >> 2) & 7
            q += 1
            if not (b0 & 0x40) or meth < 3:
                q += 2 if body[q] & 0x80 else 1
            continue
        doff = ((b0 & 3) << 8) | body[q + 1]
        q += 2
        fd = body[q]; q += 1
        if not fd & 0x80 and ((fd >> 4) & 7) < 3:
            q += 2 if body[q] & 0x80 else 1
        if not fd & 0x08:
            q += 2 if body[q] & 0x80 else 1
        if not fd & 4:
            q += 4 if wide else 2
        out.append((doff, not (b0 & 0x40)))
    return out


def main(argv):
    d = open(argv[1], "rb").read()
    for p, t, body in records(d):
        s = f"{p:06x} {REC.get(t, hex(t))} len={len(body) + 1}"
        if t in (0xA0, 0xA1):
            off = struct.unpack_from("<I" if t & 1 else "<H", body, 1)[0]
            s += f" seg={body[0]} off={off:#x} n={len(body) - (5 if t & 1 else 3)}"
        elif t in (0x9C, 0x9D):
            s += " " + ",".join(f"{o:x}{'r' if r else ''}" for o, r in fixupp_sites(t, body))
        elif t == 0x88:
            s += f" {body[:2].hex()} {body[2:40]!r}"
        if t not in (0x94, 0x95):
            print(s)


if __name__ == "__main__":
    main(sys.argv)
