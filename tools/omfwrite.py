"""Minimal Intel OMF-386 object writer (the record subset WLINK 10.0 needs) and record lister.

tools/image.py uses it to express unrecovered regions of KE.EXE as explicit RAW DEBT objects: original
bytes in LEDATA records, FIXUPP records for every LE fixup of the region, PUBDEFs for the entry points other
objects use and EXTDEFs for what the region references.  Pure data in, bytes out: the writer never decides
what a region contains.

    from omfwrite import Obj
    o = Obj("raw_0010")
    flat = o.group("FLAT", [])
    t = o.segment("_TEXT", "CODE", align="byte", data=code)
    e = o.extern("strcpy")
    f = o.fixup(t, 0x21, "off32", target=("ext", e), frame=("grp", flat), self_rel=True)
    o.ledata(t, 0, 0x40, [f])          # optional explicit LEDATA/FIXUPP plan, emitted in call order
    Path("raw.obj").write_bytes(o.to_bytes())

Record order: THEADR, COMENT*, LNAMES, SEGDEF*, GRPDEF*, EXTDEF (extern() call order), PUBDEF*, then the
explicit LEDATA plan in call order (each LEDATA followed by one FIXUPP32 record holding its fixups in the given
order), then fixup-free LEDATA for the uncovered bytes (<= 1024 bytes each), MODEND.  Fixups that are not
listed in an explicit ledata() (e.g. self-relative calls) are appended to the LEDATA that contains them.

WLINK 10.0 processes the fixups that follow one LEDATA in reverse order (build/image/probe: probe1.py), so a
caller that needs a given LE record order passes each LEDATA's fixups reversed.

    python tools/omfwrite.py FILE.OBJ     # record listing: LEDATA ranges, FIXUPP site order ('r' = self-rel)
"""
from __future__ import annotations

import struct
import sys
from dataclasses import dataclass

ALIGN = {"abs": 0, "byte": 1, "word": 2, "para": 3, "page": 4, "dword": 5}
ALIGN_BYTES = {0: 1, 1: 1, 2: 2, 3: 16, 4: 256, 5: 4}
LOC = {"lobyte": 0, "off16": 1, "base16": 2, "ptr16:16": 3, "hibyte": 4, "off16l": 5, "off32": 9,
       "ptr16:32": 11, "off32l": 13}
LOC_SIZE = {0: 1, 1: 2, 2: 2, 3: 4, 4: 1, 5: 2, 9: 4, 11: 6, 13: 4}
MAX_LEDATA = 1024
MAX_REC = 4096   # explicit LEDATA spans may exceed the 1024-byte convention (WLINK reads the record length)


def _rec(typ: int, body: bytes) -> bytes:
    if len(body) + 1 > 0xFFFF:
        raise ValueError("record too long")
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


@dataclass(eq=False)
class Fix:
    seg: int
    off: int
    loc: int
    self_rel: bool
    target: tuple      # ("seg"|"grp"|"ext", index)
    frame: tuple       # ("seg"|"grp"|"ext", index) | ("target",) | ("loc",)
    disp: int

    @property
    def size(self):
        return LOC_SIZE[self.loc]


class Obj:
    def __init__(self, name: str, comments=()):
        self.name = name
        self.lnames: list = []
        self.segs: list = []
        self.groups: list = []
        self.externs: list = []
        self.publics: list = []       # (name, seg, off)
        self.fixups: list = []
        self.plan: list = []          # explicit [(seg, a, b, [Fix])] in emission order
        self.comments = list(comments)  # (class byte, payload bytes)
        self.start = None             # (seg, off) program entry for MODEND

    # --- definitions ---------------------------------------------------------
    def _lname(self, s: str) -> int:
        if s not in self.lnames:
            self.lnames.append(s)
        return self.lnames.index(s) + 1

    def segment(self, name, cls, align="byte", size=None, data=None, use32=True, combine=2) -> int:
        """Add a SEGDEF (combine 2 = public, 5 = stack); returns its 1-based index. data=None -> no LEDATA."""
        if isinstance(align, str):
            align = ALIGN[align]
        if size is None:
            size = len(data or b"")
        if data is not None and len(data) != size:
            raise ValueError("data length != size")
        self._lname(name); self._lname(cls)
        self.segs.append(Seg(name, cls, align, size, use32, combine, bytes(data) if data is not None else None))
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

    def fixup(self, seg, off, loc="off32", target=None, frame=("target",), self_rel=False, disp=0) -> Fix:
        loc = LOC[loc] if isinstance(loc, str) else loc
        f = Fix(seg, off, loc, self_rel, target, frame, disp)
        self.fixups.append(f)
        return f

    def ledata(self, seg, a, b, fixups=()):
        """Explicit LEDATA [a, b) of seg, followed by one FIXUPP holding `fixups` in this order."""
        for f in fixups:
            if f.seg != seg or not (a <= f.off and f.off + f.size <= b):
                raise ValueError(f"fixup {f.off:#x} outside LEDATA {a:#x}..{b:#x}")
        self.plan.append((seg, a, b, list(fixups)))

    # --- serialisation ------------------------------------------------------
    def _fixupp(self, fl, base):
        out = bytearray()
        body = bytearray()
        for f in fl:
            rel = f.off - base
            if not 0 <= rel < 1024:
                raise ValueError(f"fixup {f.off:#x} is {rel:#x} bytes into its LEDATA (max 1023)")
            m = 0 if f.self_rel else 1
            sub = bytearray([0x80 | (m << 6) | (f.loc << 2) | (rel >> 8), rel & 0xFF])
            fmeth = {"seg": 0, "grp": 1, "ext": 2, "loc": 4, "target": 5}[f.frame[0]]
            tk, ti = f.target
            tmeth = {"seg": 0, "grp": 1, "ext": 2}[tk]
            p = 1 if f.disp == 0 else 0
            sub.append((fmeth << 4) | (p << 2) | tmeth)
            if fmeth < 3:
                sub += _idx(f.frame[1])
            sub += _idx(ti)
            if not p:
                sub += struct.pack("<I", f.disp & 0xFFFFFFFF)
            if len(body) + len(sub) > 1000:   # split long FIXUPP records (same LEDATA, order kept)
                out += _rec(0x9D, bytes(body)); body = bytearray()
            body += sub
        if body:
            out += _rec(0x9D, bytes(body))
        return bytes(out)

    def _layout(self):
        """Complete LEDATA plan: explicit entries + fixup-free fill; stray fixups attached to their LEDATA."""
        plan = [(s, a, b, list(fl)) for s, a, b, fl in self.plan]
        planned = {id(f) for *_, fl in plan for f in fl}
        for si, seg in enumerate(self.segs, 1):
            if seg.data is None:
                if any(f.seg == si for f in self.fixups) or any(p[0] == si for p in plan):
                    raise ValueError(f"fixups/LEDATA in data-less segment {seg.name}")
                continue
            spans = sorted((a, b) for s, a, b, _ in plan if s == si)
            for (a1, b1), (a2, b2) in zip(spans, spans[1:]):
                if a2 < b1:
                    raise ValueError(f"overlapping LEDATA in {seg.name}: {a1:#x}..{b1:#x} / {a2:#x}..{b2:#x}")
            stray = [f for f in self.fixups if f.seg == si and id(f) not in planned]
            fields = sorted((f.off, f.off + f.size) for f in stray)
            holes, cur = [], 0
            for a, b in spans + [(seg.size, seg.size)]:
                if a > cur:
                    holes.append((cur, a))
                cur = max(cur, b)
            for a, b in holes:
                while a < b:
                    c = min(b, a + MAX_LEDATA)
                    for fo, fe in fields:
                        if fo < c < fe:
                            c = fo if fo > a else fe
                    plan.append((si, a, c, []))
                    a = c
            for f in stray:
                for s, a, b, fl in plan:
                    if s == si and a <= f.off and f.off + f.size <= b:
                        fl.append(f)
                        break
                else:
                    raise ValueError(f"fixup {f.off:#x} in {seg.name} straddles LEDATA records")
        for s, a, b, _ in plan:
            if b - a > MAX_REC:
                raise ValueError(f"LEDATA {a:#x}..{b:#x} longer than {MAX_REC}")
        return plan

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
            out += _rec(0x9A, _idx(self._lname(name)) + b"".join(b"\xff" + _idx(i) for i in segs))
        body = bytearray()
        for n in self.externs:
            if len(body) + len(n) + 2 > 1000:
                out += _rec(0x8C, bytes(body)); body = bytearray()
            body += _name(n) + b"\x00"
        if body:
            out += _rec(0x8C, bytes(body))
        by_seg: dict = {}
        for n, si, off in self.publics:
            by_seg.setdefault(si, []).append((n, off))
        for si, lst in by_seg.items():
            grp = next((gi for gi, (_, segs) in enumerate(self.groups, 1) if si in segs), 0)
            head = _idx(grp) + _idx(si)
            body = bytearray(head)
            for n, off in lst:
                item = _name(n) + struct.pack("<I", off) + b"\x00"
                if len(body) + len(item) > 1000:
                    out += _rec(0x91, bytes(body)); body = bytearray(head)
                body += item
            out += _rec(0x91, bytes(body))
        for si, a, b, fl in self._layout():
            out += _rec(0xA1, _idx(si) + struct.pack("<I", a) + self.segs[si - 1].data[a:b])
            if fl:
                out += self._fixupp(fl, a)
        if self.start is not None:
            si, off = self.start
            out += _rec(0x8B, bytes([0xC1, 0x50]) + _idx(si) + struct.pack("<I", off))
        else:
            out += _rec(0x8B, b"\x00")
        return bytes(out)


# --- inspection ----------------------------------------------------------------
REC = {0x80: "THEADR", 0x88: "COMENT", 0x96: "LNAMES", 0x98: "SEGDEF", 0x99: "SEGDEF32", 0x9A: "GRPDEF",
       0x8C: "EXTDEF", 0x90: "PUBDEF", 0x91: "PUBDEF32", 0xA0: "LEDATA", 0xA1: "LEDATA32", 0x9C: "FIXUPP",
       0x9D: "FIXUPP32", 0x8A: "MODEND", 0x8B: "MODEND32", 0xB0: "COMDEF", 0xB4: "LEXTDEF", 0xB6: "LPUBDEF",
       0xB7: "LPUBDEF32", 0x94: "LINNUM", 0x95: "LINNUM32", 0xA2: "LIDATA", 0xA3: "LIDATA32"}


def records(d: bytes, pos: int = 0):
    """Yield (offset, type, body) for each record of the module starting at pos (through MODEND)."""
    p = pos
    while p < len(d):
        t = d[p]
        ln = struct.unpack_from("<H", d, p + 1)[0]
        yield p, t, d[p + 3:p + 2 + ln]
        p += 3 + ln
        if t in (0x8A, 0x8B):
            break


def fixupp_subrecords(t: int, body: bytes):
    """[(data-record-relative site, loc, self_rel)] of one FIXUPP record's FIXUP subrecords, in order."""
    q, out = 0, []
    wide = t & 1
    while q < len(body):
        b0 = body[q]
        if not b0 & 0x80:  # THREAD subrecord
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
        out.append((doff, (b0 >> 2) & 0xF, not (b0 & 0x40)))
    return out


def ledata_groups(d: bytes):
    """[(seg index, LEDATA offset, [(site offset in seg, loc, self_rel), ...])] in record order: the fixups
    WLINK processes together (all FIXUPP records following one LEDATA/LIDATA)."""
    out = []
    for _, t, body in records(d):
        if t in (0xA0, 0xA1, 0xA2, 0xA3):
            si = body[0] if body[0] < 0x80 else ((body[0] & 0x7F) << 8) | body[1]
            p = 1 if body[0] < 0x80 else 2
            off = struct.unpack_from("<I" if t & 1 else "<H", body, p)[0]
            out.append((si, off, []))
        elif t in (0x9C, 0x9D):
            si, off, fl = out[-1]
            fl.extend((off + o, loc, sr) for o, loc, sr in fixupp_subrecords(t, body))
    return out


def main(argv):
    d = open(argv[1], "rb").read()
    for p, t, body in records(d):
        s = f"{p:06x} {REC.get(t, hex(t))} len={len(body) + 1}"
        if t in (0xA0, 0xA1):
            off = struct.unpack_from("<I" if t & 1 else "<H", body, 1)[0]
            s += f" seg={body[0]} off={off:#x} n={len(body) - (5 if t & 1 else 3)}"
        elif t in (0x9C, 0x9D):
            s += " " + ",".join(f"{o:x}{'r' if r else ''}" for o, _, r in fixupp_subrecords(t, body))
        elif t == 0x88:
            s += f" {body[:2].hex()} {body[2:40]!r}"
        if t not in (0x94, 0x95):
            print(s)


if __name__ == "__main__":
    main(sys.argv)
