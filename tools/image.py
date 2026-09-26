"""End-state verification: ONE WLINK run that produces KE.EXE, with explicit RAW DEBT for unrecovered regions.

    python tools/image.py                    # canonical hybrid: verified sources where they link exactly, raw debt elsewhere
    python tools/image.py --mode raw         # all game code/data raw (only runtime libraries + stub real)
    python tools/image.py --mode canonical --no-predict     # link every canonical source even if predicted to differ
    python tools/image.py --exclude fn:f_3b2 --exclude unit:T11     # force items to raw debt

Output (build/image/<mode>/): objs/*.obj, ke.lnk, ke.exe, ke.map, report.json; a summary is printed.
Exit status 0 iff the linked ke.exe is byte-identical to assets/KE.EXE.

Link plan (one WLINK 10.0 GA run through tools/dosrun.py): game objects in address order (canonical units,
per-function objects and asm modules where admitted, RAW DEBT objects elsewhere; raw data carriers placed so
each DGROUP class concatenates to the original layout), the obj2 IRQ module, then clib3s/math387s/emu387 from the
pinned 10.0 GA install; `system dos4g`, `option stub=` 10.0 GA BINB/wstub.exe, `option heapsize=20000`,
`name ke.exe`.  WLINK places BEGTEXT (cstrt386) first.  Nothing is copied into or patched in the linked file.

RAW DEBT objects (tools/omfwrite.py) hold the original bytes, one FIXUPP per LE fixup of their range (targets:
own segment, or EXTDEF of the public that owns the target address), self-relative fixups for calls to known
entry points outside the object, PUBDEFs for every address other objects reference, and EXTDEFs in
first-reference order (library members are pulled in demand order).  Their LEDATA/FIXUPP plan reproduces the
original per-page LE fixup record order under the WLINK model measured in build/image/probe:
  * the fixups following one LEDATA are processed in reverse order;
  * each page keeps a list of 512-byte blocks of LE records; a record that does not fit opens a new block,
    blocks are written newest first; sel16 (base) records live in a separate list written before off32 ones.
The same model predicts the fixup order of real objects from their OMF, so a substitution that cannot be
identical (e.g. a function object that cuts one of the original LEDATA chunks) is kept as raw debt and reported.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import sys
from bisect import bisect_right
from dataclasses import dataclass, field
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build" / "pylib"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import capstone  # noqa: E402
import dosrun  # noqa: E402
import le as lemod  # noqa: E402
import omf  # noqa: E402
from omfwrite import ALIGN_BYTES, Obj, ledata_groups  # noqa: E402

ORIG = ROOT / "assets" / "KE.EXE"
BLOCK = 512
LIBS = [("clib3s.lib", "LIB386/DOS/clib3s.lib"), ("math387s.lib", "LIB386/math387s.lib"),
        ("emu387.lib", "LIB386/DOS/emu387.lib")]
STUB = "BINB/wstub.exe"
NAME_RX = re.compile(r"^([fga])_([0-9a-f]+)$")
GAME_DATA_CLASSES = ("CONST", "_DATA", "_BSS")


def h(x):
    return f"{x:#x}"


# ============================================================================ original image
def invert_blocks(F, cap=BLOCK):
    """Processing (insertion) order S of one page's record list F under the WLINK block model."""
    sz = [len(f.raw) for f in F]
    n = len(F)

    @lru_cache(None)
    def solve(start, nf):
        if start == n:
            return ()
        tot = 0
        for end in range(start + 1, n + 1):
            tot += sz[end - 1]
            if tot > cap:
                break
            if nf is not None and tot + nf <= cap:
                continue  # an older block must be full w.r.t. the first record of the next newer block
            rest = solve(end, sz[start])
            if rest is not None:
                return (end,) + rest
        return None

    cuts = solve(0, None)
    if cuts is None:
        return None
    b = [0, *cuts]
    blocks = [F[b[i]:b[i + 1]] for i in range(len(b) - 1)]
    return [f for blk in reversed(blocks) for f in blk]


def forward_blocks(S, cap=BLOCK):
    blocks = []
    for f in S:
        if not blocks or sum(len(x.raw) for x in blocks[0]) + len(f.raw) > cap:
            blocks.insert(0, [])
        blocks[0].append(f)
    return [f for blk in blocks for f in blk]


class Original:
    def __init__(self, path=ORIG):
        self.L = L = lemod.LE(path)
        self.bytes = {}
        for o in L.objects:
            b = L.object_bytes(o)
            self.bytes[o["n"]] = (b + bytes(max(0, o["vsize"] - len(b))))[:o["vsize"]]
        self.page_obj = {}
        for p in range(L.hdr["num_pages"]):
            o, base = L.page_object(p)
            self.page_obj[p] = (o["n"], base)
        self.sites = {}    # (obj, off) -> {"type", "tobj", "toff"}
        self.chains = {}   # (page, "off"|"sel") -> [(obj, off)] WLINK processing order
        pages = {}
        for f in L.fixups:
            if len(f.src_offsets) != 1:
                raise SystemExit("source-list fixups are not modelled")
            pages.setdefault(f.page, []).append(f)
        for p, F in pages.items():
            obj, base = self.page_obj[p]
            sel = [f for f in F if f.src_type == 2]
            off = [f for f in F if f.src_type != 2]
            if F[:len(sel)] != sel:
                raise SystemExit(f"page {p}: sel16 records not first (block model violated)")
            for kind, lst in (("off", off), ("sel", sel)):
                S = invert_blocks(lst)
                if S is None or forward_blocks(S) != lst:
                    raise SystemExit(f"page {p}: no block-model inversion")
                self.chains[(p, kind)] = [(obj, base + f.src_offsets[0]) for f in S]
            for f in F:
                site = (obj, base + f.src_offsets[0])
                t = f.target
                self.sites.setdefault(site, {"type": lemod.SRC_TYPES.get(f.src_type), "tobj": t.get("obj"),
                                             "toff": t.get("off")})

    def pages_of(self, obj, off, size=4):
        o = self.L.objects[obj - 1]
        first = o["page_index"] - 1
        ps = self.L.hdr["page_size"]
        return sorted({first + off // ps, first + (off + size - 1) // ps} & set(range(first, first + o["num_pages"])))


# ============================================================================ context
class Ctx:
    def __init__(self):
        self.man = json.loads((ROOT / "manifest.json").read_text())
        self.cfg = dosrun.config()
        inst = self.cfg["installs"]["wc100"]
        self.wroot = dosrun.tools_root() / inst["dir"]
        self.libs = []
        for name, rel in LIBS:
            p = self.wroot / rel
            dosrun._check(inst, self.wroot, rel)
            self.libs.append((name, p))
        dosrun._check(inst, self.wroot, STUB)
        self.stub = self.wroot / STUB
        self.orig = Original()
        rt = self.man["runtime"]
        self.members = [(int(m["start"], 16), int(m["end"], 16), m["lib"], m["member"]) for m in rt["members"]]
        self.code_end = min(s for s, *_ in self.members)
        self.rt_pub = {k: int(v.split(":")[1], 16) for k, v in rt["publics"].items()}
        dl = rt["data_layout"]
        c2 = dl["CONST2_istable"].split("..")
        self.classes = {  # game part of each DGROUP class in obj3 (cursor at class start .. first runtime piece)
            "CONST": (4, int(dl["first_runtime_CONST"], 16)),
            "_DATA": (int(c2[1], 16), int(dl["first_runtime_initialized_DATA"], 16)),
            "_BSS": (int(dl["YIE"], 16), int(dl["first_runtime_BSS"], 16)),
        }
        self.funcs = sorted((f for f in self.man["functions"]), key=lambda f: (f.get("object", 1), int(f["start"], 16)))
        self.fn_by_name = {f["name"]: f for f in self.funcs}
        self.fn_start = {(f.get("object", 1), int(f["start"], 16)): f["name"] for f in self.funcs}
        self._libmods = None

    # --- library modules ----------------------------------------------------
    def libmods(self):
        """{lib name: [(module name, omf.Module, raw bytes)]} and public -> (lib, module)."""
        if self._libmods is None:
            mods, pubs = {}, {}
            for name, p in self.libs:
                data = p.read_bytes()
                lst = []
                for off, m in omf.parse_library(data):
                    lst.append((m.name, m, off))
                    for n, s, o, loc in m.publics:
                        if not loc:
                            pubs.setdefault(n, (name, m.name))
                    for c in m.comdefs:
                        pubs.setdefault(c["name"], (name, m.name))
                mods[name] = lst
            self._libmods = (mods, pubs)
        return self._libmods

    def main_addr(self):
        """Address of `main`, decoded from cmain386's call (the runtime needs the game to define it)."""
        mods, _ = self.libmods()
        s = next(st for st, _, lib, mem in self.members if mem == "cmain386")
        m = next(m for n, m, _ in mods["clib3s.lib"] if n == "cmain386")
        f = next(f for f in m.fixups if m.target_name(f) == "main")
        code = self.orig.bytes[1]
        v = struct.unpack_from("<i", code, s + f.off)[0]
        return s + f.off + 4 + v

    def lib_public_at(self, addr):
        """(name, public address) of the runtime code public owning obj1 address addr."""
        mem = next(((s, e) for s, e, *_ in self.members if s <= addr < e), None)
        best = None
        for n, a in self.rt_pub.items():
            if a <= addr and (mem is None or mem[0] <= a) and (best is None or a > best[1]):
                best = (n, a)
        if addr < 0x10 and "___begtext" in self.rt_pub:
            return "___begtext", self.rt_pub["___begtext"]
        return best

    def addr_of_name(self, name):
        """(obj, addr) that a game symbol name denotes (manifest function/symbol, main, naming convention)."""
        if name == "main":
            return (1, self.main_addr())
        f = self.fn_by_name.get(name)
        if f:
            return (f.get("object", 1), int(f["start"], 16))
        s = self.man.get("symbols", {}).get(name)
        if s and name not in self.rt_pub:
            o, a = s.split(":")
            return (int(o), int(a, 16))
        m = NAME_RX.match(name)
        if m:
            return (3 if m.group(1) == "g" else 1, int(m.group(2), 16))
        return None


# ============================================================================ items
@dataclass
class Item:
    key: str               # raw:1:0010 | unit:T11 | fn:f_3b2 | asm:a_982c | asm16:irq | rawdata:..
    kind: str              # raw | unit | c | asm
    obj: int               # LE object of the code (1, 2), 3 for pure data carriers
    start: int
    end: int
    src: str | None = None
    profile: str | None = None
    functions: list = field(default_factory=list)
    objpath: Path | None = None
    mod: object = None
    raw_mod: bytes | None = None
    segbase: dict = field(default_factory=dict)     # seg index -> (LE obj, base) for placed segments
    data: dict = field(default_factory=dict)        # class -> (base, size) for non-empty game data pieces
    problems: list = field(default_factory=list)
    notes: list = field(default_factory=list)
    publics: dict = field(default_factory=dict)     # name -> (obj, addr)
    externs: list = field(default_factory=list)     # EXTDEF names in record order
    pieces: list = field(default_factory=list)      # raw data carriers: [(class, start, end)]
    exports: dict = field(default_factory=dict)     # raw: name -> (obj, addr)
    order: float = 0.0

    @property
    def real(self):
        return self.kind != "raw"


def cache_compile(ctx, src: Path, profile: str, cache: Path):
    """Compile a canonical source with its manifest profile (tools/check.py's compile path); cached by content."""
    import check
    prof = ctx.cfg["profiles"][profile]
    inc = b"".join(p.read_bytes() for p in sorted((ROOT / "include").rglob("*")) if p.is_file()) \
        if (ROOT / "include").exists() else b""
    key = hashlib.sha256(src.read_bytes() + json.dumps(prof, sort_keys=True).encode() + inc).hexdigest()[:20]
    out = cache / f"{src.stem}-{key}.obj"
    if not out.exists():
        work = cache / f"work-{src.stem}"
        shutil.rmtree(work, ignore_errors=True)
        objp, _ = check.compile_candidate(src, profile, work)
        shutil.copyfile(objp, out)
        shutil.rmtree(work, ignore_errors=True)
    return out


def canonical_items(ctx, cache):
    man = ctx.man
    items = []
    unit_src = {}
    for u in man.get("units", []):
        it = Item(f"unit:{u['id']}", "unit", 1, int(u["start"], 16), int(u["end"], 16), u["src"], u["profile"])
        it.functions = [f["name"] for f in ctx.funcs if f.get("src") == u["src"]]
        unit_src[u["src"]] = it
        items.append(it)
    irq = None
    for f in ctx.funcs:
        if f.get("status") != "matching" or not f.get("src") or f["src"] in unit_src:
            continue
        o = f.get("object", 1)
        if o == 2:
            if irq is None:
                irq = Item("asm16:irq", "asm", 2, 0, len(ctx.orig.bytes[2]), f["src"], f["profile"])
                items.append(irq)
            irq.functions.append(f["name"])
            continue
        kind = "asm" if f["kind"] == "asm" else "c"
        it = Item(f"{'asm' if kind == 'asm' else 'fn'}:{f['name']}", kind, 1, int(f["start"], 16),
                  int(f["end"], 16), f["src"], f["profile"])
        it.functions = [f["name"]]
        items.append(it)
    for it in items:
        try:
            it.objpath = cache_compile(ctx, ROOT / it.src, it.profile, cache)
        except SystemExit as e:
            it.problems.append(f"compile failed: {str(e).splitlines()[0]}")
            continue
        it.raw_mod = it.objpath.read_bytes()
        it.mod = omf.parse_object(it.raw_mod)[0]
    return items


def place_real(ctx, it):
    """Code placement, data bases, publics; problems that make the object unlinkable in place."""
    m = it.mod
    if m is None:
        return
    code = [i for i, s in enumerate(m.segments) if s and s.cls.upper() == "CODE" and s.size]
    if it.obj == 1:
        if len(code) != 1:
            it.problems.append(f"expected one non-empty CODE segment, got {[m.segments[i].name for i in code]}")
            return
        s = m.segments[code[0]]
        if s.name != "_TEXT":
            it.problems.append(f"code segment {s.name} ({'PARA' if s.align == 3 else s.align}): WLINK combines it "
                               f"with other {s.name} contributions after all _TEXT, not at {h(it.start)}")
        a = ALIGN_BYTES.get(s.align, 1)
        if it.start % a:
            it.problems.append(f"segment alignment {a} cannot place it at {h(it.start)}")
        if s.size != it.end - it.start:
            it.problems.append(f"code size {h(s.size)} != original extent {h(it.end - it.start)}")
        it.segbase[code[0]] = (1, it.start)
    else:
        cur = 0
        for i in code:
            s = m.segments[i]
            a = ALIGN_BYTES.get(s.align, 1)
            cur = (cur + a - 1) // a * a
            it.segbase[i] = (2, cur)
            cur += s.size
        if cur != it.end:
            it.problems.append(f"USE16 layout ends at {h(cur)} != {h(it.end)}")
    # data segments: base from publics with address names, else from code fixups vs original LE targets
    for i, s in enumerate(m.segments):
        if not s or i in it.segbase or s.frame is not None or s.cls.upper() in ("DEBSYM", "DEBTYP"):
            continue
        if s.size == 0:
            continue
        if s.cls.upper() not in ("DATA", "BSS") or s.name not in ("CONST", "CONST2", "_DATA", "_BSS"):
            it.problems.append(f"segment {s.name}/{s.cls} ({s.size} bytes) has no place in the image")
            continue
        bases = set()
        for n, si, o, loc in m.publics:
            if si == i:
                a = ctx.addr_of_name(n)
                if a and a[0] == 3:
                    bases.add(a[1] - o)
        for f in m.fixups:
            if f.seg in it.segbase and (f.target[0] & 3) == 0 and f.target[1] == i and not f.self_rel and f.loc == 9:
                o, base = it.segbase[f.seg]
                site = ctx.orig.sites.get((o, base + f.off))
                if site and site["tobj"] == 3:
                    fld = int.from_bytes(m.segments[f.seg].data[f.off:f.off + 4], "little")
                    bases.add(site["toff"] - fld - f.disp)
        if len(bases) != 1:
            it.problems.append(f"cannot place {s.name} ({s.size} bytes): bases {sorted(map(h, bases))}")
            continue
        base = bases.pop()
        it.segbase[i] = (3, base)
        if s.name == "CONST2":
            it.problems.append(f"CONST2 contribution at {h(base)}: the original has no game CONST2 bytes")
        else:
            it.data[s.name] = (base, s.size)
    for n, si, o, loc in m.publics:
        if si in it.segbase and not loc:
            ob, base = it.segbase[si]
            it.publics[n] = (ob, base + o)
    it.externs = [e for e in m.externs if e]


# ============================================================================ planning
class Plan:
    def __init__(self, ctx, reals, mode):
        self.ctx = ctx
        self.mode = mode
        self.reals = [r for r in reals if not r.problems]
        self.rejected = [r for r in reals if r.problems]

    # --- ownership ----------------------------------------------------------
    def build(self):
        ctx = self.ctx
        o = ctx.orig
        self.real_code = sorted((r for r in self.reals if r.obj == 1), key=lambda r: r.start)
        for a, b in zip(self.real_code, self.real_code[1:]):
            if a.end > b.start:
                raise SystemExit(f"overlapping real items {a.key} {b.key}")
        # raw code pieces: gaps between real code, split at proposed object boundaries that the fixup
        # order allows (never inside an original LEDATA chunk that has fixups on both sides)
        self.forbid = self.forbidden_splits()
        cuts = set()
        tus = ROOT / "build" / "tus.json"
        if tus.exists():
            for t in json.loads(tus.read_text()):
                cuts |= {int(t["range"][0], 16), int(t["range"][1], 16)}
        for f in ctx.funcs:
            if f.get("object", 1) == 1 and f["kind"] == "asm":
                cuts |= {int(f["start"], 16), int(f["end"], 16)}
        self.rejected_cuts = sorted(c for c in cuts if 0x10 < c < ctx.code_end and not self.split_ok(c))
        cuts = sorted(c for c in cuts if 0x10 < c < ctx.code_end and self.split_ok(c))
        raws = []
        cur = 0x10
        for r in self.real_code + [None]:
            end = r.start if r else ctx.code_end
            if end > cur:
                pts = [cur] + [c for c in cuts if cur < c < end] + [end]
                for a, b in zip(pts, pts[1:]):
                    raws.append(Item(f"raw:1:{a:05x}", "raw", 1, a, b))
            if r:
                cur = r.end
        self.raw_code = raws
        irq = next((r for r in self.reals if r.obj == 2), None)
        self.raw16 = None if irq else Item("raw:2:00000", "raw", 2, 0, len(o.bytes[2]))
        # link order: code items by address, then the obj2 module
        seq = sorted(self.real_code + raws, key=lambda r: r.start)
        for i, it in enumerate(seq):
            it.order = float(i)
        tail = irq or self.raw16
        tail.order = float(len(seq))
        self.code_items = seq
        self.tail = tail
        self.plan_data()
        self.items = sorted(self.code_items + [self.tail] + self.carriers, key=lambda r: r.order)

    def forbidden_splits(self):
        """Intervals (lo, hi] where an object boundary would reorder an original page's fixup chain,
        plus the inside of every fixup field."""
        iv = []
        for (p, kind), ch in self.ctx.orig.chains.items():
            mx = None
            for obj, a in ch:
                if mx is not None and mx[1] > a and mx[0] == obj:
                    iv.append((obj, a, mx[1]))
                if mx is None or a > mx[1]:
                    mx = (obj, a)
        for (obj, a), s in self.ctx.orig.sites.items():
            iv.append((obj, a, a + (2 if s["type"] == "sel16" else 4) - 1))
        return iv

    def split_ok(self, d, obj=1):
        """True if an object boundary at d keeps every original page chain in object order."""
        for o, lo, hi in self.forbid:
            if o == obj and lo < d <= hi:
                return False
        return True

    def plan_data(self):
        ctx = self.ctx
        carriers = {}
        self.data_problems = []

        def carrier(pos):
            if pos not in carriers:
                it = Item(f"rawdata:{pos + 1:g}", "raw", 3, 0, 0)
                it.order = pos + 0.5
                carriers[pos] = it
            return carriers[pos]

        for cls in GAME_DATA_CLASSES:
            lo, hi = ctx.classes[cls]
            reals = sorted(((r.data[cls], r) for r in self.reals if cls in r.data), key=lambda x: x[1].order)
            cur, pos = lo, -1.0
            for (base, size), r in reals:
                if base < cur:
                    self.data_problems.append(f"{r.key}: {cls} at {h(base)} precedes the previous contribution "
                                              f"end {h(cur)} (link order != address order)")
                    r.problems.append(f"{cls} at {h(base)} out of class order")
                    continue
                if base > cur:
                    carrier(pos).pieces.append((cls, cur, base))
                cur, pos = base + size, r.order
            if hi > cur:
                carrier(pos).pieces.append((cls, cur, hi))
        self.carriers = list(carriers.values())

    # --- names --------------------------------------------------------------
    def owner(self, obj, addr, end_ok=False):
        """Link item that owns (obj, addr) — game code/data — or None (runtime)."""
        if obj in (1, 2):   # code: an address equal to an object's end belongs to the next object
            for it in self.code_items + [self.tail]:
                if it.obj == obj and it.start <= addr < it.end:
                    return it
            return None
        for it in self.reals:
            for cls, (base, size) in it.data.items():
                if base <= addr < base + size or (end_ok and addr == base + size):
                    return it
        for it in self.carriers:
            for cls, a, b in it.pieces:
                if a <= addr < b or (end_ok and addr == b):
                    return it
        return None


# ============================================================================ raw objects
def disasm_rel32(code, base, start, end, funcs):
    """[(field offset, target)] of rel32 call/jmp/jcc inside manifest functions of [start, end)."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out = []
    for f in funcs:
        if f.get("object", 1) != 1:
            continue
        a, b = max(start, int(f["start"], 16)), min(end, int(f["end"], 16))
        if a >= b:
            continue
        for ins in md.disasm(code[a:b], a):
            by = ins.bytes
            if len(by) == 5 and by[0] in (0xE8, 0xE9):
                fo = ins.address + 1
            elif len(by) == 6 and by[0] == 0x0F and 0x80 <= by[1] <= 0x8F:
                fo = ins.address + 2
            else:
                continue
            rel = struct.unpack_from("<i", by, fo - ins.address)[0]
            out.append((fo, ins.address + len(by) + rel))
    return out


class RawBuilder:
    def __init__(self, plan):
        self.plan = plan
        self.ctx = plan.ctx
        self.stats = {"le_fixups": 0, "rel32_fixups": 0, "rel32_baked_cross": 0, "exports": 0,
                      "group_relative": 0, "segment_relative_out_of_range": 0}

    def canonical_name(self, obj, addr):
        n = self.ctx.fn_start.get((obj, addr))
        if n:
            return n
        return {1: "f_%x", 2: "a16_%x", 3: "g_%x"}[obj] % addr

    def target_ref(self, it, obj, addr, want_exact=False):
        """How `it` refers to (obj, addr): ('self', seg key, field) | ('ext', name, field) | ('grp', field)."""
        ctx = self.ctx
        if obj in (1, 2) and it.kind == "raw" and it.obj == obj and it.start <= addr < it.end:
            return ("self", "text", addr - it.start)
        if obj == 3 and it.kind == "raw":
            for cls, a, b in it.pieces:
                if a <= addr <= b:
                    return ("self", cls, addr - a)
        own = self.plan.owner(obj, addr, end_ok=True)
        if own is None:
            if obj == 1:
                n, a = ctx.lib_public_at(addr)
                return ("ext", n, addr - a)
            self.stats["group_relative"] += 1
            return ("grp", addr) if obj == 3 else ("seg-out", addr)
        if own.real:
            exact = [n for n, (o, a) in own.publics.items() if o == obj and a == addr]
            if exact:
                return ("ext", sorted(exact)[0], 0)
            if want_exact:
                return None
            best = max(((a, n) for n, (o, a) in own.publics.items() if o == obj and a <= addr), default=None)
            if best:
                return ("ext", best[1], addr - best[0])
            self.stats["group_relative"] += 1
            return ("grp", addr) if obj == 3 else ("seg-out", addr)
        name = self.canonical_name(obj, addr)
        own.exports[name] = (obj, addr)
        return ("ext", name, 0)

    def request_exports(self):
        """Names real objects (and the runtime: main) import from raw objects."""
        ctx, plan = self.ctx, self.plan
        _, libpubs = ctx.libmods()
        need = [("main", (1, ctx.main_addr()), "runtime cmain386")]
        for r in plan.reals:
            for e in r.externs:
                if e in libpubs or e in r.publics:
                    continue
                need.append((e, ctx.addr_of_name(e), r.key))
        defined = {}
        for r in plan.reals:
            for n in r.publics:
                defined.setdefault(n, r.key)
        problems = []
        for name, where, who in need:
            if name in defined:
                continue
            if where is None:
                problems.append((who, f"extern {name}: no known address"))
                continue
            own = plan.owner(*where)
            if own is None or own.real:
                problems.append((who, f"extern {name} = {where[0]}:{where[1]:x} lies in "
                                      f"{own.key if own else 'the runtime'} which does not define it"))
                continue
            own.exports[name] = where
        return problems

    # --- processing order -----------------------------------------------------
    def processing_order(self, it, sites):
        """Global order of `sites` consistent with every original page chain (Kahn, page-major)."""
        o = self.ctx.orig
        sset = set(sites)
        succ, indeg, key = {s: [] for s in sites}, {s: 0 for s in sites}, {}
        for (p, kind), ch in sorted(o.chains.items()):
            sub = [s for s in ch if s in sset]
            for i, s in enumerate(sub):
                key.setdefault(s, (p, 0 if kind == "off" else 1, i))
            for a, b in zip(sub, sub[1:]):
                succ[a].append(b)
                indeg[b] += 1
        import heapq
        heap = [(key[s], s) for s in sites if indeg[s] == 0]
        heapq.heapify(heap)
        out = []
        while heap:
            _, s = heapq.heappop(heap)
            out.append(s)
            for t in succ[s]:
                indeg[t] -= 1
                if indeg[t] == 0:
                    heapq.heappush(heap, (key[t], t))
        if len(out) != len(sites):
            raise SystemExit(f"{it.key}: original page chains are cyclic for this object")
        return out

    @staticmethod
    def group_sites(Q, size):
        """Split processing order Q into consecutive groups with pairwise disjoint field spans."""
        groups = []
        for s in Q:
            lo, hi = s[1], s[1] + size(s)
            if groups and not (lo >= groups[-1][1]):   # descends or stays inside: same LEDATA
                g = groups[-1]
                g[0], g[1] = min(g[0], lo), max(g[1], hi)
                g[2].append(s)
            else:
                groups.append([lo, hi, [s]])
            # merge with any earlier group whose span overlaps the current one
            while True:
                cur = groups[-1]
                k = next((i for i in range(len(groups) - 1)
                          if groups[i][0] < cur[1] and cur[0] < groups[i][1]), None)
                if k is None:
                    break
                merged = [min(g[0] for g in groups[k:]), max(g[1] for g in groups[k:]),
                          [x for g in groups[k:] for x in g[2]]]
                groups[k:] = [merged]
        return groups

    # --- analysis: fixups, references, exports (no output) ------------------------------
    def analyze(self, it):
        """Fixup specs of a raw object: [(range key, LE obj, site, loc, self_rel, tref, frame)]."""
        ctx, o = self.ctx, self.ctx.orig
        st = {"le_fixups": 0, "rel32_fixups": 0, "rel32_baked_cross": 0}
        ranges = []
        if it.obj in (1, 2):
            ranges.append(("text", it.obj, it.start, it.end))
        ranges += [(c, 3, a, b) for c, a, b in it.pieces]
        it.ranges = ranges

        def seg_of(obj, addr):
            for k, ro, a, b in ranges:
                if ro == obj and a <= addr < b:
                    return k, a
            return None
        specs = []
        le_sites = set()
        for (sobj, sa), info in sorted(o.sites.items()):
            sk = seg_of(sobj, sa)
            if not sk:
                continue
            le_sites.add((sobj, sa))
            if info["type"] == "sel16":
                specs.append((sk[0], sobj, sa, "base16", False, ("dgroup",), ("dgroup",)))
                continue
            if info["type"] != "off32":
                raise SystemExit(f"{it.key}: unsupported LE fixup type {info['type']}")
            tref = self.target_ref(it, info["tobj"], info["toff"])
            specs.append((sk[0], sobj, sa, "off32", False, tref, ("flat",) if sobj != 2 else ("target",)))
        st["le_fixups"] = len(le_sites)
        if it.obj == 1:
            fields = {a + k for (_, a) in le_sites for k in range(4)}
            entries = {int(f["start"], 16) for f in ctx.funcs if f.get("object", 1) == 1} | set(ctx.rt_pub.values())
            for fo, tgt in disasm_rel32(o.bytes[1], 0, it.start, it.end, ctx.funcs):
                if it.start <= tgt < it.end or not (0 <= tgt < len(o.bytes[1])):
                    continue
                if any(fo + k in fields for k in range(4)) or fo + 4 > it.end:
                    continue
                tref = self.target_ref(it, 1, tgt, want_exact=True) if tgt in entries else None
                if tref is None or tref[0] != "ext" or tref[2] != 0:
                    st["rel32_baked_cross"] += 1
                    continue
                specs.append(("text", 1, fo, "off32", True, tref, ("flat",)))
                st["rel32_fixups"] += 1
        it.specs, it.le_sites, it.stats = specs, le_sites, st

    # --- emission ----------------------------------------------------------------
    def emit(self, it, outdir):
        o = self.ctx.orig
        ob = Obj(it.key.replace(":", "_"))
        flat = ob.group("FLAT", [])
        segs = {}
        if it.obj in (1, 3):
            code = o.bytes[1][it.start:it.end] if it.obj == 1 else b""
            segs["text"] = ob.segment("_TEXT", "CODE", "byte", data=code)
            for cls, clsname in (("CONST", "DATA"), ("CONST2", "DATA"), ("_DATA", "DATA"), ("_BSS", "BSS")):
                pcs = [(a, b) for c, a, b in it.pieces if c == cls]
                if len(pcs) > 1:
                    raise SystemExit(f"{it.key}: two {cls} pieces")
                a, b = pcs[0] if pcs else (0, 0)
                segs[cls] = ob.segment(cls, clsname, "byte", size=b - a,
                                       data=None if cls == "_BSS" else o.bytes[3][a:b])
            dgroup = ob.group("DGROUP", [segs[c] for c in ("CONST", "CONST2", "_DATA", "_BSS")])
        else:
            segs["text"] = ob.segment("RAW16_TEXT", "CODE", "para", data=o.bytes[2][it.start:it.end], use32=False)
            dgroup = None
        rstart = {k: a for k, _, a, _ in it.ranges}
        data = {k: bytearray(ob.segs[si - 1].data) for k, si in segs.items() if ob.segs[si - 1].data is not None}
        refs = []
        le_fix = {}
        for key, sobj, site, loc, self_rel, tref, frame in it.specs:
            fr = {"flat": ("grp", flat), "target": ("target",), "dgroup": ("grp", dgroup)}[frame[0]]
            kind = tref[0]
            if kind == "dgroup":
                tgt, fld = ("grp", dgroup), None
            elif kind == "self":
                tgt, fld = ("seg", segs[tref[1]]), tref[2]
            elif kind == "ext":
                tgt, fld = ("ext", tref[1]), tref[2]
                refs.append((site, tref[1]))
            elif kind == "grp":
                tgt, fld = ("grp", dgroup), tref[1]
            else:  # seg-out: own text segment with an out-of-range displacement
                tgt, fld = ("seg", segs["text"]), tref[1] - it.start
            f = ob.fixup(segs[key], site - rstart[key], loc, target=tgt, frame=fr, self_rel=self_rel)
            if fld is not None:
                struct.pack_into("<I", data[key], site - rstart[key], fld & 0xFFFFFFFF)
            if not self_rel:
                le_fix[(sobj, site)] = f
        for k, si in segs.items():
            if k in data:
                ob.segs[si - 1].data = bytes(data[k])
        # EXTDEF order: first reference address; pseudo references (runtime demand order) inserted
        first = {}
        for a, n in refs:
            first[n] = min(first.get(n, 1 << 40), a)
        order = sorted(first, key=lambda n: first[n])
        for names, at in getattr(it, "pseudo", []):
            idx = next((i for i, m in enumerate(order) if first[m] > at), len(order))
            for k, pn in enumerate([n for n in names if n not in order]):
                order.insert(idx + k, pn)
        ob.externs = order
        for f in ob.fixups:
            if f.target[0] == "ext":
                f.target = ("ext", order.index(f.target[1]) + 1)
        it.first_ref = first
        # LEDATA plan reproducing the original processing order of this object's LE fixups
        Q = self.processing_order(it, sorted(le_fix))
        size = lambda s: 2 if o.sites[s]["type"] == "sel16" else 4
        it.groups = 0
        for key, lobj, a, b in it.ranges:
            q = [s for s in Q if s[0] == lobj and a <= s[1] < b]
            for lo, hi, grp in self.group_sites(q, size):
                ob.ledata(segs[key], lo - a, hi - a, [le_fix[s] for s in reversed(grp)])
                it.groups += 1
        # publics requested by other objects (and by the runtime: main)
        for n, (obj, addr) in sorted(it.exports.items(), key=lambda x: (x[1], x[0])):
            sk = next(((k, a) for k, ro, a, b in it.ranges if ro == obj and a <= addr < b), None) \
                or next(((k, a) for k, ro, a, b in it.ranges if ro == obj and addr == b), None)
            if not sk:
                raise SystemExit(f"{it.key}: cannot export {n} at {obj}:{addr:x}")
            ob.public(n, segs[sk[0]], addr - sk[1])
        path = outdir / f"{it.key.replace(':', '_')}.obj"
        path.write_bytes(ob.to_bytes())
        it.objpath = path
        it.raw_mod = path.read_bytes()
        it.mod = omf.parse_object(it.raw_mod)[0]
        it.segbase = {}
        segi = {s.name: i for i, s in enumerate(it.mod.segments) if s}
        if it.obj in (1, 2):
            it.segbase[1] = (it.obj, it.start)
        for cls, a, b in it.pieces:
            it.segbase[segi[cls]] = (3, a)
        return ob


# ============================================================================ prediction
def processed_sites(ctx, it):
    """[(kind, (obj, addr))] in WLINK processing order for the LE-generating fixups of a linked object."""
    out = []
    m = it.mod if it.mod is not None else omf.parse_object(it.raw_mod)[0]
    for si, off, fl in ledata_groups(it.raw_mod):
        if si not in it.segbase:
            continue
        lobj, base = it.segbase[si]
        for site, loc, self_rel in reversed(fl):
            if self_rel:
                continue
            if loc == 2:
                out.append(("sel", (lobj, base + site)))
            elif loc in (9, 13):
                out.append(("off", (lobj, base + site)))
    return out


def predict_chains(ctx, items):
    """Predicted per-page chains of game sites for the link order `items`."""
    o = ctx.orig
    pred = {}
    for it in items:
        for kind, (obj, a) in processed_sites(ctx, it):
            for p in o.pages_of(obj, a, 2 if kind == "sel" else 4):
                pred.setdefault((p, kind), []).append((obj, a))
    return pred


def is_game_site(ctx, obj, a):
    if obj == 1:
        return 0x10 <= a < ctx.code_end
    if obj == 2:
        return True
    return any(lo <= a < hi for lo, hi in ctx.classes.values())


def compare_chains(ctx, pred):
    bad = []
    for k, ch in ctx.orig.chains.items():
        want = [s for s in ch if is_game_site(ctx, *s)]
        got = pred.get(k, [])
        if want != got:
            i = next((i for i, (x, y) in enumerate(zip(want, got)) if x != y), min(len(want), len(got)))
            bad.append((k, i, want[i] if i < len(want) else None, got[i] if i < len(got) else None))
    return bad


def simulate_libs(ctx, items):
    """Demand-driven library extraction: (loaded module names in order, puller map name -> item)."""
    mods, pubs = ctx.libmods()
    index = {(lib, n): m for lib, lst in mods.items() for n, m, _ in lst}
    syms, defined, who = [], set(), {}
    seen = set()
    for it in items:
        m = it.mod if it.mod is not None else omf.parse_object(it.raw_mod)[0]
        for n, *_ in m.publics:
            defined.add(n)
        for e in (x for x in m.externs if x):
            if e not in seen:
                seen.add(e); syms.append(e); who[e] = it.key
    loaded = []
    i = 0
    while i < len(syms):
        s = syms[i]; i += 1
        if s in defined or s not in pubs:
            continue
        lib, mname = pubs[s]
        if (lib, mname) in loaded:
            continue
        loaded.append((lib, mname))
        who.setdefault(("module", mname), s)
        m = index[(lib, mname)]
        for n, *_ in m.publics:
            defined.add(n)
        for c in m.comdefs:
            defined.add(c["name"])
        for e in (x for x in m.externs if x):
            if e not in seen:
                seen.add(e); syms.append(e); who[e] = (lib, mname)
    return loaded, who


# ============================================================================ link + compare
def link(ctx, items, outdir):
    lines = ["system dos4g", "option quiet, map=ke.map", f"option stub={str(ctx.stub).replace('/', chr(92))}",
             "option heapsize=20000", "name ke.exe"]
    for it in items:
        lines.append(f"file {it.objpath.relative_to(outdir).as_posix().replace('/', chr(92))}")
    lines.append("library " + ", ".join(str(p).replace("/", "\\") for _, p in ctx.libs))
    (outdir / "ke.lnk").write_text("\n".join(lines) + "\n")
    exe = outdir / "ke.exe"
    if exe.exists():
        exe.unlink()
    r = dosrun.run("wlink", ["@ke.lnk"], cwd=outdir)
    (outdir / "wlink.log").write_text(r.out)
    return r, exe


class Owners:
    def __init__(self, ctx, plan):
        self.iv = {1: [], 2: [], 3: []}
        for s, e, lib, mem in ctx.members:
            self.iv[1].append((s, e, f"lib {lib}:{mem}"))
        self.iv[1].append((0, 0x10, "lib clib3s.lib:cstrt386 BEGTEXT"))
        for it in plan.code_items + [plan.tail]:
            fn = [f["name"] for f in ctx.funcs if f.get("object", 1) == it.obj
                  and int(f["start"], 16) < it.end and int(f["end"], 16) > it.start]
            self.iv[it.obj].append((it.start, it.end, it.key, fn))
        for it in plan.reals:
            for cls, (b, n) in it.data.items():
                self.iv[3].append((b, b + n, f"{it.key} {cls}"))
        for it in plan.carriers:
            for cls, a, b in it.pieces:
                self.iv[3].append((a, b, f"{it.key} {cls}"))
        for v in self.iv.values():
            v.sort()

    def at(self, obj, addr):
        for x in self.iv.get(obj, []):
            if x[0] <= addr < x[1]:
                label = x[2]
                if len(x) > 3:
                    fn = [n for n in x[3]]
                    label += f" ({', '.join(fn[:3])}{'...' if len(fn) > 3 else ''})"
                return label
        return "runtime/linker (outside game ranges)"


def compare(ctx, plan, exe):
    o = ctx.orig
    rep = {}
    a = ORIG.read_bytes()
    b = exe.read_bytes()
    rep["identical"] = a == b
    rep["sha256"] = {"original": hashlib.sha256(a).hexdigest(), "linked": hashlib.sha256(b).hexdigest()}
    rep["size"] = {"original": len(a), "linked": len(b)}
    L = lemod.LE(exe)
    rep["stub_equal"] = a[:o.L.le_off] == b[:L.le_off]
    rep["header_diffs"] = {k: [o.L.hdr[k], L.hdr[k]] for k in o.L.hdr if o.L.hdr[k] != L.hdr.get(k)}
    rep["object_table_diffs"] = [{"n": x["n"], "field": k, "original": x[k], "linked": y[k]}
                                 for x, y in zip(o.L.objects, L.objects) for k in x
                                 if k != "flag_names" and x[k] != y[k]]
    rep["page_map_equal"] = o.L.page_map == L.page_map
    own = Owners(ctx, plan)
    objs = []
    for x, y in zip(o.L.objects, L.objects):
        ob, lb = o.L.object_bytes(x), L.object_bytes(y)
        n = min(len(ob), len(lb))
        d = next((i for i in range(n) if ob[i] != lb[i]), None)
        ent = {"n": x["n"], "equal": ob == lb, "init_original": len(ob), "init_linked": len(lb)}
        if d is not None:
            ent.update(first_diff=h(d), owner=own.at(x["n"], d), diff_bytes=sum(ob[i] != lb[i] for i in range(n)),
                       original=ob[d:d + 8].hex(), linked=lb[d:d + 8].hex())
        objs.append(ent)
    rep["objects"] = objs
    fx = []
    po, pl = {}, {}
    for f in o.L.fixups:
        po.setdefault(f.page, []).append(f)
    for f in L.fixups:
        pl.setdefault(f.page, []).append(f)
    for p in range(max(o.L.hdr["num_pages"], L.hdr["num_pages"])):
        A, B = po.get(p, []), pl.get(p, [])
        if [f.raw for f in A] == [f.raw for f in B]:
            continue
        i = next((i for i, (x, y) in enumerate(zip(A, B)) if x.raw != y.raw), min(len(A), len(B)))
        obj, base = o.page_obj.get(p, (None, 0))
        e = {"page": p, "object": obj, "count": [len(A), len(B)], "first_diff_index": i}
        if i < len(A):
            e["original"] = {"site": h(base + A[i].src_offsets[0]), **A[i].to_json()}
            e["owner"] = own.at(obj, base + A[i].src_offsets[0])
        if i < len(B):
            e["linked"] = {"site": h(base + B[i].src_offsets[0]), **B[i].to_json()}
        fx.append(e)
    rep["fixup_pages_differing"] = fx
    return rep


def accounting(ctx, plan):
    acc = {"obj1": {}, "obj2": {}, "obj3_init": {}, "obj3_bss": {}}

    def add(k, cat, n):
        acc[k][cat] = acc[k].get(cat, 0) + n
    o = ctx.orig
    add("obj1", "library", 0x10 + (len(o.bytes[1]) - ctx.code_end))
    for it in plan.code_items:
        add("obj1", "raw" if not it.real else it.kind, it.end - it.start)
    t = plan.tail
    add("obj2", "raw" if not t.real else t.kind, t.end - t.start)
    init_end = o.L.objects[2]["vsize"] if False else len(o.L.object_bytes(o.L.objects[2]))
    game3 = 0
    for it in plan.reals:
        for cls, (b, n) in it.data.items():
            add("obj3_bss" if cls == "_BSS" else "obj3_init", it.kind, n)
            game3 += n
    for it in plan.carriers:
        for cls, a, b in it.pieces:
            add("obj3_bss" if cls == "_BSS" else "obj3_init", "raw", b - a)
            game3 += b - a
    g_init = sum(v for k, v in acc["obj3_init"].items())
    g_bss = sum(v for k, v in acc["obj3_bss"].items())
    add("obj3_init", "library", init_end - g_init)
    add("obj3_bss", "library", o.L.objects[2]["vsize"] - init_end - g_bss)
    tot = {}
    for k, v in acc.items():
        for c, n in v.items():
            tot[c] = tot.get(c, 0) + n
    acc["total"] = tot
    return acc


# ============================================================================ driver
def run(args):
    ctx = Ctx()
    out = ROOT / "build" / "image" / args.mode
    objdir = out / "objs"
    shutil.rmtree(objdir, ignore_errors=True)
    objdir.mkdir(parents=True)
    cache = ROOT / "build" / "image" / "objcache"
    cache.mkdir(parents=True, exist_ok=True)
    reals = []
    if args.mode == "canonical":
        reals = canonical_items(ctx, cache)
        for r in reals:
            place_real(ctx, r)
            if r.key in args.exclude:
                r.problems.append("excluded on the command line")
    excluded_log = []
    rounds = 0
    while True:
        rounds += 1
        plan = Plan(ctx, reals, args.mode)
        plan.build()
        rb = RawBuilder(plan)
        probs = rb.request_exports()
        newly = [(k, p) for k, p in probs if k in {r.key for r in plan.reals}]
        if newly:
            for k, p in newly:
                next(r for r in reals if r.key == k).problems.append(p)
            continue
        if probs:
            raise SystemExit(f"unresolvable runtime reference: {probs}")
        raws = [it for it in plan.code_items + [plan.tail] + plan.carriers if not it.real]
        for it in raws:
            rb.analyze(it)
        for it in raws:
            rb.emit(it, objdir)
        for it in plan.reals:
            dst = objdir / f"{it.key.replace(':', '_')}.obj"
            shutil.copyfile(it.objpath, dst)
            it.objpath = dst
        items = plan.items
        # --- runtime member order: pseudo references for members no game object pulls in time
        order = [(lib, mem) for s, e, lib, mem in ctx.members]
        loaded, who = simulate_libs(ctx, items)
        lib_code = [x for x in loaded if x in set(order)]
        lib_problem = None
        if lib_code != order:
            i = next((i for i, (x, y) in enumerate(zip(lib_code, order)) if x != y), min(len(lib_code), len(order)))
            lib_problem = (i, order[i] if i < len(order) else None, lib_code[i] if i < len(lib_code) else None)
        if lib_problem and not any(getattr(it, "pseudo", None) for it in items) and args.mode in ("raw", "canonical"):
            if add_pseudo_refs(ctx, plan, rb, items, order, objdir):
                loaded, who = simulate_libs(ctx, items)
                lib_code = [x for x in loaded if x in set(order)]
                lib_problem = None
                if lib_code != order:
                    i = next((i for i, (x, y) in enumerate(zip(lib_code, order)) if x != y),
                             min(len(lib_code), len(order)))
                    lib_problem = (i, order[i] if i < len(order) else None, lib_code[i] if i < len(lib_code) else None)
        # --- fixup order prediction
        bad = compare_chains(ctx, predict_chains(ctx, items))
        culprit = None
        if bad and args.predict:
            for k, i, want, got in bad:
                for s in (got, want):
                    if s is None:
                        continue
                    own = plan.owner(*s)
                    if own is not None and own.real:
                        culprit = (own, f"LE fixup order on page {k[0]} ({k[1]}) differs at chain index {i}: "
                                        f"original {s[0]}:{s[1]:x} (object boundary cuts an original LEDATA chunk "
                                        f"or the object's own chunking differs)")
                        break
                if culprit:
                    break
        if culprit is None and lib_problem and args.predict:
            j, want, got = lib_problem
            if got is not None:
                mname = got[1]
                sym = who.get(("module", mname))
                src = who.get(sym)
                own = next((r for r in plan.reals if r.key == src), None)
                if own is not None:
                    culprit = (own, f"pulls runtime member {mname} via {sym} before {want[1] if want else '-'} "
                                    f"(original demand order)")
        if culprit:
            own, why = culprit
            own.problems.append(why)
            excluded_log.append((own.key, why))
            continue
        break
    report = {"mode": args.mode, "prediction_rounds": rounds}
    report["predicted_fixup_order_mismatches"] = [
        {"page": k[0], "list": k[1], "index": i, "original": w and f"{w[0]}:{w[1]:x}", "predicted": g and f"{g[0]}:{g[1]:x}"}
        for k, i, w, g in bad]
    report["predicted_runtime_order"] = "original" if not lib_problem else {
        "index": lib_problem[0], "original": lib_problem[1], "predicted": lib_problem[2]}
    r, exe = link(ctx, items, out)
    report["wlink"] = {"rc": r.rc, "output": r.out.strip().splitlines()[-20:]}
    if r.rc != 0 or not exe.exists():
        report["identical"] = False
        write_report(out, report, plan, ctx, rb, excluded_log)
        return 1
    report.update(compare(ctx, plan, exe))
    report["accounting"] = accounting(ctx, plan)
    write_report(out, report, plan, ctx, rb, excluded_log)
    return 0 if report["identical"] else 1


def add_pseudo_refs(ctx, plan, rb, items, order, objdir):
    """A runtime member that no game reference pulls in time (emu387 386inite: Watcom FP modules end with a
    trailing EXTDEF __init_387_emulator, __8087 — build/image/probe/fp.c) gets a pseudo EXTDEF in the raw object
    whose reference pulls the next member, placed just before that reference."""
    loaded, who = simulate_libs(ctx, items)
    mods, _ = ctx.libmods()
    lib_code = [x for x in loaded if x in set(order)]
    i = next((i for i, (x, y) in enumerate(zip(lib_code, order)) if x != y), None)
    if i is None or i + 1 >= len(order):
        return False
    want = order[i]
    m = next(m for n, m, _ in mods[want[0]] if n == want[1])
    names = [n for n, s, o, loc in m.publics if not loc]
    names = ["__init_387_emulator", "__8087"] if "__init_387_emulator" in names else names[:1]
    sym = who.get(("module", order[i + 1][1]))
    it = next((x for x in items if x.key == who.get(sym)), None)
    if it is None or it.real:
        return False
    it.pseudo = [(names, it.first_ref[sym] - 1)]
    it.notes.append(f"pseudo EXTDEF {names} before {sym} at {h(it.first_ref[sym])}: runtime member "
                    f"{want[0]}:{want[1]} has no game reference")
    rb.emit(it, objdir)
    return True


def write_report(out, report, plan, ctx, rb, excluded_log):
    report["items"] = {
        "real": [{"key": r.key, "range": f"{r.obj}:{r.start:x}..{r.end:x}", "src": r.src,
                  "data": {k: f"{h(b)}+{h(n)}" for k, (b, n) in r.data.items()}} for r in plan.reals],
        "rejected": [{"key": r.key, "range": f"{r.obj}:{r.start:x}..{r.end:x}", "src": r.src, "problems": r.problems}
                     for r in plan.rejected],
        "raw": [{"key": r.key, "range": f"{r.obj}:{r.start:x}..{r.end:x}" if r.obj != 3 else None,
                 "pieces": [f"{c} {h(a)}..{h(b)}" for c, a, b in r.pieces], "exports": len(r.exports),
                 "notes": r.notes} for r in plan.code_items + [plan.tail] + plan.carriers if not r.real],
    }
    st = {}
    for it in plan.code_items + [plan.tail] + plan.carriers:
        for k, v in getattr(it, "stats", {}).items():
            st[k] = st.get(k, 0) + v
    st["exports"] = sum(len(it.exports) for it in plan.code_items + [plan.tail] + plan.carriers if not it.real)
    report["raw_stats"] = st
    report["excluded_by_prediction"] = [{"key": k, "why": w} for k, w in excluded_log]
    report["rejected_split_points"] = [h(c) for c in plan.rejected_cuts]
    (out / "report.json").write_text(json.dumps(report, indent=1))
    print(summary(report, out))


def summary(rep, out):
    L = []
    L.append(f"mode {rep['mode']}: {'IDENTICAL' if rep.get('identical') else 'DIFFERENT'} "
             f"(sha256 {rep.get('sha256', {}).get('linked', '-')[:16]})  report {out / 'report.json'}")
    if rep.get("wlink", {}).get("rc"):
        L.append("  wlink failed: " + " | ".join(rep["wlink"]["output"][-5:]))
    it = rep["items"]
    L.append(f"  objects: {len(it['real'])} real, {len(it['raw'])} raw debt; {len(it['rejected'])} canonical "
             f"sources kept as raw ({len(rep['excluded_by_prediction'])} by fixup/runtime-order prediction)")
    if "accounting" in rep:
        t = rep["accounting"]["total"]
        L.append("  bytes: " + ", ".join(f"{k} {v}" for k, v in sorted(t.items())))
    if not rep.get("identical") and "objects" in rep:
        L.append(f"  stub {'=' if rep['stub_equal'] else '!='}, header diffs {list(rep['header_diffs'])[:6]}, "
                 f"page map {'=' if rep['page_map_equal'] else '!='}")
        for o in rep["objects"]:
            if not o["equal"]:
                L.append(f"  obj{o['n']}: first diff {o.get('first_diff')} in {o.get('owner')} "
                         f"({o.get('diff_bytes')} bytes; init {o['init_original']}/{o['init_linked']})")
        fx = rep["fixup_pages_differing"]
        if fx:
            e = fx[0]
            L.append(f"  fixups: {len(fx)} pages differ; first page {e['page']} index {e['first_diff_index']} "
                     f"({e.get('owner', '-')}) counts {e['count']}")
    if rep["predicted_fixup_order_mismatches"]:
        L.append(f"  predicted fixup-order mismatches: {len(rep['predicted_fixup_order_mismatches'])}")
    if rep["predicted_runtime_order"] != "original":
        L.append(f"  predicted runtime member order differs: {rep['predicted_runtime_order']}")
    return "\n".join(L)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--mode", choices=["raw", "canonical"], default="canonical")
    ap.add_argument("--exclude", action="append", default=[], help="item key to keep as raw debt")
    ap.add_argument("--no-predict", dest="predict", action="store_false",
                    help="do not exclude canonical objects predicted to break identity")
    return run(ap.parse_args(argv[1:]))


if __name__ == "__main__":
    sys.exit(main(sys.argv))
