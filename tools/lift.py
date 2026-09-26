"""Deterministic first-draft lifter for Watcom C 10.0 `wcc386 -3s -od` functions of KE.EXE.

    python tools/lift.py f_7032 f_2e4a --out DIR        # DIR/f_7032.c, DIR/f_2e4a.c
    python tools/lift.py --all --out DIR                # every non-matching C function
    python tools/lift.py --range 0x7000 0x8000 --out DIR
    python tools/lift.py --controls --out DIR           # every matching function (regression set)
    python tools/lift.py f_7032 --goto --out DIR        # skip control-flow structuring

How it works (all facts below are proven by probes or EXACT matches, see docs/compiler-notes.md):
  * `-od` compiles each statement in isolation and does no jump optimisation, so `if (c) goto L;` and
    `goto L;` compile to exactly one cmp/jcc resp. jmp.  The lifter symbolically executes the body,
    emits one C statement per store / call / branch, and writes control flow as gotos.  A structuring
    pass then folds the fixed -od layouts (if/else, while, for, do-while) into structured statements
    (identical code under -od).
  * Types: proven declarations in src/ first, then evidence gathered over the whole executable
    (access widths, movsx/movzx, signed/unsigned jumps, sar/shr, idiv/div, pointer use, how callers
    push arguments).  Memory is addressed generically as `*(T *)(base + offset)` with byte-sized
    base pointers, which Watcom compiles like the original field/array accesses.
  * Locals: every scalar auto takes a 4-byte slot; Watcom sorts autos (then temps such as the return
    spill) stably by size and assigns slots from [ebp-4] down.  The lifter declares locals in slot
    order and picks short/int and the return type so that this sort reproduces the original frame.
tools/check.py is the only authority for an EXACT match.
"""
from __future__ import annotations

import argparse
import itertools
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build" / "pylib"))
sys.path.insert(0, str(ROOT / "tools"))
import capstone  # noqa: E402
from capstone import x86  # noqa: E402
import le  # noqa: E402

# ---------------------------------------------------------------------------------------------
# C scalar types: c/sc = (unsigned/signed) char, us/s = (unsigned) short, u/i = (unsigned) int,
# p = byte pointer.  Watcom's plain char is unsigned.
TNAME = {"c": "unsigned char", "sc": "signed char", "s": "short", "us": "unsigned short",
         "i": "int", "u": "unsigned", "p": "unsigned char *"}
TW = {"c": 1, "sc": 1, "s": 2, "us": 2, "i": 4, "u": 4, "p": 4}
SIGNED = {"sc", "s", "i"}


def tcode(w, signed):
    return {1: ("sc", "c"), 2: ("s", "us"), 4: ("i", "u")}[w][0 if signed else 1]


def promote(t):
    return "i" if t in ("c", "sc", "s", "us") else t


LIB_HEADERS = {"getenv": "stdlib.h", "exit": "stdlib.h", "atexit": "stdlib.h", "memmove": "string.h",
               "printf": "stdio.h", "inp": "conio.h", "outp": "conio.h", "int386": "i86.h",
               "_disable": "i86.h", "_enable": "i86.h", "_rotl": "stdlib.h", "fopen": "stdio.h",
               "fclose": "stdio.h", "fread": "stdio.h", "fwrite": "stdio.h", "fseek": "stdio.h",
               "ftell": "stdio.h", "free": "stdlib.h", "getch": "conio.h", "outpw": "conio.h",
               "int386x": "i86.h", "memset": "string.h", "close": "io.h", "memcpy": "string.h",
               "strcpy": "string.h", "strcat": "string.h", "strlen": "string.h", "strchr": "string.h",
               "strcmp": "string.h", "_stricmp": "string.h", "strcmpi": "string.h", "strtoul": "stdlib.h",
               "itoa": "stdlib.h", "ltoa": "stdlib.h", "abs": "stdlib.h", "malloc": "stdlib.h",
               "_splitpath": "stdlib.h", "kbhit": "conio.h", "spawnlp": "process.h"}
# library prototypes (ret, params, varargs); "p" = pointer, "cp" = char pointer
LIB_SIGS = {"getenv": ("p", ["cp"], False), "exit": (None, ["i"], False), "atexit": ("i", ["p"], False),
            "memmove": ("p", ["p", "p", "u"], False), "printf": ("i", ["cp"], True),
            "inp": ("i", ["u"], False), "outp": ("i", ["u", "i"], False), "int386": ("i", ["i", "p", "p"], False),
            "_disable": (None, [], False), "_enable": (None, [], False), "_rotl": ("u", ["u", "u"], False),
            "fopen": ("p", ["cp", "cp"], False), "fclose": ("i", ["p"], False),
            "fread": ("u", ["p", "u", "u", "p"], False), "fwrite": ("u", ["p", "u", "u", "p"], False),
            "fseek": ("i", ["p", "i", "i"], False), "ftell": ("i", ["p"], False), "free": (None, ["p"], False),
            "getch": ("i", [], False), "outpw": ("u", ["u", "u"], False),
            "int386x": ("i", ["i", "p", "p", "p"], False), "memset": ("p", ["p", "i", "u"], False),
            "close": ("i", ["i"], False), "memcpy": ("p", ["p", "p", "u"], False),
            "strcpy": ("p", ["cp", "cp"], False), "strcat": ("p", ["cp", "cp"], False),
            "strlen": ("u", ["cp"], False), "strchr": ("p", ["cp", "i"], False),
            "strcmp": ("i", ["cp", "cp"], False), "_stricmp": ("i", ["cp", "cp"], False),
            "strcmpi": ("i", ["cp", "cp"], False), "strtoul": ("u", ["cp", "p", "i"], False),
            "itoa": ("p", ["i", "cp", "i"], False), "ltoa": ("p", ["i", "cp", "i"], False),
            "abs": ("i", ["i"], False), "malloc": ("p", ["u"], False), "kbhit": ("i", [], False),
            "_splitpath": (None, ["cp", "cp", "cp", "cp", "cp"], False),
            "spawnlp": ("i", ["i", "cp", "cp"], True)}
JREL = {"je": "==", "jne": "!=", "jl": "<", "jle": "<=", "jg": ">", "jge": ">=",
        "jb": "<", "jbe": "<=", "ja": ">", "jae": ">=", "js": "<", "jns": ">="}
JUNS = {"jb", "jbe", "ja", "jae"}
NEG = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}
SWAP = {"==": "==", "!=": "!=", "<": ">", ">": "<", "<=": ">=", ">=": "<="}
BINOPS = {"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>"}
SUBREG = {"al": ("eax", 1), "ax": ("eax", 2), "bl": ("ebx", 1), "bx": ("ebx", 2), "cl": ("ecx", 1),
          "cx": ("ecx", 2), "dl": ("edx", 1), "dx": ("edx", 2), "si": ("esi", 2), "di": ("edi", 2)}
ARITH = {"add": "+", "sub": "-", "and": "&", "or": "|", "xor": "^", "shl": "<<", "sal": "<<",
         "sar": ">>", "shr": ">>", "imul": "*"}


def s32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


# ---------------------------------------------------------------------------------------------
class Var:
    """A named storage location: global (g_X), argument (aK) or local (v_N)."""

    def __init__(self, kind, name, key):
        self.kind, self.name, self.key = kind, name, key
        self.acc = Counter()      # (width, how) with how in mov/sx/zx/cmp/arith/store/const/rmw
        self.sign = Counter()     # True -> signed evidence, False -> unsigned evidence
        self.ptr = 0              # used as an address base
        self.array = False        # global used with an index / address taken
        self.addr = False         # address taken (&v)
        self.type = None          # resolved type code
        self.decl = None          # proven declaration (globals)
        self.volatile = False
        self.psum = 0             # pointer-arithmetic evidence (see Lift.pointer_sum)
        self.stride = None        # pointee size of a pointer stepped with p++ (not 1)

    def note(self, w, how):
        self.acc[(w, how)] += 1

    def widths(self, *hows):
        return {w for (w, h), n in self.acc.items() if not hows or h in hows}


class E:
    """Expression node.  op: k const, v var load, m memory load, & address, fn function,
    call, trunc, un ops (neg, ~), binary ops (+ - * / % & | ^ << >>), cmp."""
    __slots__ = ("op", "a", "b", "v", "w", "x", "var", "uns", "args", "ptr", "rel", "name", "typ", "seq", "flip")
    counter = 0

    def __init__(self, op, a=None, b=None, **kw):
        self.op, self.a, self.b = op, a, b
        E.counter += 1
        self.seq, self.flip = E.counter, False
        self.v = kw.get("v")
        self.w = kw.get("w", 4)          # width of the value as held in the register
        self.x = kw.get("x")             # extension of a narrow load: 's' / 'z' / None
        self.var = kw.get("var")
        self.uns = kw.get("uns", False)  # unsigned machine op (shr, div, jb...)
        self.args = kw.get("args")
        self.ptr = False
        self.rel = kw.get("rel")
        self.name = kw.get("name")
        self.typ = kw.get("typ")

    def kids(self):
        out = [k for k in (self.a, self.b) if isinstance(k, E)]
        if self.args:
            out += [x for x in self.args if isinstance(x, E)]
        return out

    def walk(self):
        yield self
        for k in self.kids():
            yield from k.walk()

    def key(self):
        return (self.op, self.a.key() if isinstance(self.a, E) else self.a,
                self.b.key() if isinstance(self.b, E) else self.b, self.v, self.w, self.x,
                id(self.var) if self.var else None, self.uns, self.name,
                tuple(x.key() for x in self.args) if self.args else None)


def K(v, w=4):
    return E("k", v=s32(v), w=w)


# ---------------------------------------------------------------------------------------------
class Program:
    def __init__(self):
        self.man = json.loads((ROOT / "manifest.json").read_text())
        L = le.LE(ROOT / "assets" / "KE.EXE")
        self.code = L.object_bytes(L.objects[0])[:L.objects[0]["vsize"]]
        self.fix = {off: t for obj, off, typ, t in L.resolved_fixups() if obj == 1}
        self.funcs = {f["name"]: f for f in self.man["functions"]}
        self.byaddr = {int(f["start"], 16): f["name"] for f in self.man["functions"]}
        self.symname = {}
        for k, v in self.man.get("symbols", {}).items():
            if not re.fullmatch(r"[0-9]+:[0-9a-f]+", str(v)):
                continue
            obj, off = v.split(":")
            key = (int(obj), int(off, 16))
            if key not in self.symname or re.match(r"[fg]_[0-9a-f]+$", self.symname[key]):
                self.symname[key] = k       # a library name beats an address alias
        for k, v in sorted(self.man.get("runtime", {}).get("publics", {}).items(), reverse=True):
            if re.fullmatch(r"1:[0-9a-f]+", str(v)):
                self.symname[(1, int(v[2:], 16))] = k      # runtime library publics (proven)
        self.globals = {}
        self.sigs = defaultdict(lambda: {"calls": [], "params": {}, "ret": None})
        self.decls = self.load_decls()
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.lifts = {}

    # proven declarations -------------------------------------------------------------------
    def load_decls(self):
        """Proven declarations: a function's own definition in its matching source first, then the
        most common declaration of each symbol across src/."""
        from context import top_level_decls
        seen = defaultdict(Counter)
        own = {}
        for src in sorted((ROOT / "src").glob("*.c")):
            text = src.read_text(errors="replace")
            for stmt in top_level_decls(text):
                if not stmt.startswith("extern ") and "(" not in stmt:
                    continue
                for d in parse_decl(stmt):
                    seen[d["name"]][json.dumps(d, sort_keys=True)] += 1
            body = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
            for m in re.finditer(r"(?m)^([A-Za-z_][\w \t*]*?)\b(f_[0-9a-f]+)\s*\(([^)]*)\)\s*\{", body):
                for d in parse_decl(f"{m.group(1)} {m.group(2)}({m.group(3)});"):
                    own[d["name"]] = d
        out = {n: json.loads(c.most_common(1)[0][0]) for n, c in seen.items()}
        out.update(own)
        self.own_defs = own
        return out

    def fname(self, addr):
        return self.symname.get((1, addr)) or self.byaddr.get(addr) or f"f_{addr:x}"

    def gvar(self, off, obj=3):
        key = (obj, off)
        v = self.globals.get(key)
        if v is None:
            name = self.symname.get(key) or f"g_{off:x}"
            v = self.globals[key] = Var("g", name, key)
            d = self.decls.get(name)
            if d and d["kind"] == "var":
                v.decl = d
                v.volatile = d.get("volatile", False)
        return v

    # whole-program type resolution ---------------------------------------------------------
    def resolve(self):
        for v in self.globals.values():
            resolve_global(v)
        for name, sig in self.sigs.items():
            resolve_sig(self, name, sig)


SCALAR_RE = re.compile(r"^(?:(volatile)\s+)?((?:unsigned|signed)\s+)?(char|short|int|long)?\s*(?:\b(volatile)\b\s*)?$")


def scalar_code(spec):
    spec = " ".join(spec.replace("const", " ").split())
    m = SCALAR_RE.match(spec)
    if not m or not (m.group(2) or m.group(3)):
        return None
    sign, base = (m.group(2) or "").strip(), m.group(3) or "int"
    w = {"char": 1, "short": 2, "int": 4, "long": 4}[base]
    signed = sign == "signed" or (sign == "" and base != "char")
    return tcode(w, signed), bool(m.group(1) or m.group(4))


def parse_decl(stmt):
    """Very small declaration parser for the proven src/ declarations: scalars, scalar pointers,
    arrays and function prototypes with scalar/pointer parameters."""
    s = re.sub(r"^extern\s+", "", stmt.rstrip(";").strip())
    out = []
    fm = re.match(r"^(.*?)\b([fg]_[0-9a-f]+)\s*\((.*)\)$", s)
    if fm and "(*" not in s:
        rt = fm.group(1).strip()
        params = [p.strip() for p in fm.group(3).split(",")] if fm.group(3).strip() not in ("", "void") else []
        ps = []
        for p in params:
            p = re.sub(r"\b[a-z_]\w*$", "", p).strip() if re.search(r"\w\s+\w+$", p) and "*" not in p.split()[-1] and p.split()[-1] not in ("int", "char", "short", "long", "unsigned", "signed") else p
            if "*" in p or "..." in p:
                ps.append("p" if "*" in p else None)
            else:
                sc = scalar_code(p)
                ps.append(sc[0] if sc else None)
        ret = None if rt == "void" else ("p" if "*" in rt else (scalar_code(rt) or ("i", 0))[0])
        out.append({"kind": "func", "name": fm.group(2), "params": ps, "ret": ret, "void": rt == "void"})
        return out
    # possibly several declarators: extern int a, b, *c;
    m = re.match(r"^((?:volatile\s+|unsigned\s+|signed\s+|const\s+)*(?:char|short|int|long|unsigned|signed)?)\s*(.*)$", s)
    if not m or not m.group(1).strip():
        return out
    base = m.group(1)
    for decl in m.group(2).split(","):
        decl = decl.strip()
        dm = re.match(r"^(\*?)\s*(volatile\s+)?([fg]_[0-9a-f]+)\s*(\[\s*\d*\s*\])?$", decl)
        if not dm:
            continue
        sc = scalar_code(base)
        if not sc:
            continue
        d = {"kind": "var", "name": dm.group(3), "volatile": sc[1] or bool(dm.group(2))}
        if dm.group(1):
            d["type"], d["pointee"] = "p", sc[0]
        elif dm.group(4):
            d["type"], d["elem"] = "arr", sc[0]
        else:
            d["type"] = sc[0]
        out.append(d)
    return out


def signedness(v, default):
    s, u = v.sign[True], v.sign[False]
    if u > s:
        return False
    if s > u:
        return True
    return default


def resolve_global(v):
    d = v.decl
    if v.widths("rmwn") and not v.array:
        v.volatile = True
    if v.psum and not v.array and (not d or d["type"] in ("i", "u", "p")) and not v.widths("sx", "zx"):
        v.type, v.volatile = "p", False     # pointer arithmetic somewhere: a byte pointer
        return
    if d and d["type"] not in ("p", "arr") and not v.array:
        v.type = d["type"]
        return
    if v.array or (d and d["type"] == "arr"):
        v.type = "arr"
        return
    ws = v.widths("mov", "sx", "zx", "cmp", "arith", "store", "const", "rmw", "push")
    if v.ptr or (d and d["type"] == "p"):
        v.type = "p"
        return
    stores = v.widths("store", "const", "rmw")
    w = max(stores) if stores else (max(ws) if ws else 4)
    if v.widths("sx") & {w}:
        signed = True
    elif v.widths("zx") & {w}:
        signed = False
    else:
        signed = signedness(v, w != 1)
    v.type = tcode(w, signed)


def resolve_local(v, size):
    if size == 4 and v.widths("rmwn"):
        v.volatile = True
    if size == 4 and v.ptr and not v.sign[True]:
        v.type = "p"
        return
    if size == 4:
        v.type = tcode(4, signedness(v, True))
        return
    if v.widths("sx") & {size}:
        signed = True
    elif v.widths("zx") & {size}:
        signed = False
    else:
        signed = signedness(v, size != 1)
    v.type = tcode(size, signed)


def resolve_sig(prog, name, sig):
    """Parameter types of a callee from its body (argument vars), call sites and proven decls."""
    if name in LIB_SIGS:
        ret, ps, va = LIB_SIGS[name]
        sig.update(ret=ret or "i", void=ret is None, params=ps, varargs=False, lib=True)
        return
    d = prog.decls.get(name)
    lift = prog.lifts.get(name)
    counts = Counter(len(c) for c in sig["calls"])
    n = max(counts) if counts else 0
    if lift:
        n = max(n, lift.nparams)
    params = []
    for k in range(n):
        t = None
        if d and d["kind"] == "func" and k < len(d["params"]) and d["params"][k]:
            t = d["params"][k]
        elif lift and k in lift.args and lift.args[k].type:
            t = lift.args[k].type
        else:
            forms = Counter(c[k] for c in sig["calls"] if k < len(c))
            if (forms["narrow"] or forms["narrow16"]) and not forms["wide"]:
                t = "s"
            else:
                t = "p" if forms["ptr"] and not forms["wide"] else "i"
        params.append(t)
    sig["params"] = params
    sig["varargs"] = len(counts) > 1
    if d and d["kind"] == "func":
        sig["ret"] = d["ret"]
        sig["void"] = d["void"]
        if len(d["params"]) == n:
            sig["params"] = [p or q for p, q in zip(d["params"], params)]
        sig["varargs"] = False
    elif lift:
        sig["ret"] = lift.ret_type
        sig["void"] = lift.ret_type is None
    else:
        sig["ret"], sig["void"] = "i", False


def make_sig_view(prog, name):
    return prog.sigs[name]


# ---------------------------------------------------------------------------------------------
class Ins:
    __slots__ = ("addr", "size", "mn", "text", "ops", "dfix", "ifix", "next")

    def __init__(self, ci, fix):
        self.addr, self.size, self.mn, self.text = ci.address, ci.size, ci.mnemonic, ci.op_str
        self.dfix = self.ifix = None
        sites = [a for a in range(ci.address, ci.address + ci.size) if a in fix]
        for a in sites:
            rel = a - ci.address
            if ci.disp_size and rel == ci.disp_offset:
                self.dfix = fix[a]
            elif ci.imm_size and rel == ci.imm_offset:
                self.ifix = fix[a]
            elif ci.disp_size:
                self.dfix = fix[a]
            else:
                self.ifix = fix[a]
        self.ops = []
        for o in ci.operands:
            if o.type == x86.X86_OP_REG:
                self.ops.append(("r", ci.reg_name(o.reg), o.size))
            elif o.type == x86.X86_OP_IMM:
                self.ops.append(("i", o.imm, o.size))
            elif o.type == x86.X86_OP_MEM:
                m = o.mem
                base = ci.reg_name(m.base) if m.base else None
                index = ci.reg_name(m.index) if m.index else None
                if index == "ebp" and m.scale == 1:
                    base, index = index, base
                self.ops.append(("m", base, index, m.scale, m.disp, o.size))


class StmtList(list):
    """Statement list that remembers the instruction address each statement was emitted at."""

    def __init__(self, owner):
        super().__init__()
        self.owner, self.addr = owner, []

    def append(self, x):
        super().append(x)
        self.addr.append(self.owner.cur)

    def pop(self, k=-1):
        self.addr.pop(k)
        return super().pop(k)


class Lift:
    """Symbolic execution of one function into a flat goto program."""

    def __init__(self, prog, fn):
        self.prog, self.fn, self.name = prog, fn, fn["name"]
        self.start, self.end = int(fn["start"], 16), int(fn["end"], 16)
        code = prog.code
        # jump tables: runs of 4-spaced code fixups pointing into the function; decode around them
        sites = [a for a in range(self.start, self.end) if a in prog.fix and prog.fix[a].get("obj") == 1
                 and self.start <= prog.fix[a]["off"] < self.end]
        self.tables, run = {}, []
        for a in sites + [None]:
            if run and a == run[-1] + 4:
                run.append(a)
                continue
            if len(run) >= 2:
                self.tables[run[0]] = [prog.fix[x]["off"] for x in run]
            run = [a] if a is not None else []
        ins, pos = [], self.start
        for t in sorted(self.tables) + [self.end]:
            ins += [Ins(ci, prog.fix) for ci in prog.md.disasm(code[pos:t], pos)]
            pos = t + 4 * len(self.tables[t]) if t in self.tables else t
        self.switches, self.switch_tmps = {}, set()
        self.find_switches(ins)
        self.ins = ins
        self.args, self.locals = {}, {}
        self.stmts = StmtList(self)
        self.cur = self.start
        self.unknown = []
        self.nparams = 0
        self.ret_type = None
        self.spill = None
        self.frame = 0
        self.ok = True

    # structure of the function -------------------------------------------------------------
    def find_switches(self, ins):
        """`cmp [tmp],max; ja dflt; mov eax,[tmp]; shl eax,2; jmp cs:[eax+table]` (the switch value
        was stored in a compiler temp slot `tmp` first; a `dec`/`sub` normalises the lowest case)."""
        for n, i in enumerate(ins):
            if not (i.mn == "jmp" and i.ops and i.ops[0][0] == "m" and i.dfix and i.dfix.get("off") in self.tables):
                continue
            cases = self.tables[i.dfix["off"]]
            win = ins[max(0, n - 6):n]
            dflt = next((x.ops[0][1] for x in reversed(win) if x.mn == "ja"), None)
            cmp = next((x for x in reversed(win) if x.mn == "cmp"), None)
            tmp = None
            if cmp and cmp.ops[0][0] == "m" and cmp.ops[0][1] == "ebp":
                tmp = -cmp.ops[0][4]
            skip = {x.addr for x in win[win.index(cmp):]} if cmp in win else set()
            # the compiler's own `jmp` over the table and the alignment filler before it
            t0 = i.dfix["off"]
            before = [x for x in ins if x.addr < t0]
            while before and (before[-1].mn == "nop" or before[-1].text in ("eax, eax", "eax, [eax]")):
                skip.add(before.pop().addr)
            if before and before[-1].mn == "jmp" and before[-1].ops[0][0] == "i" and                     before[-1].ops[0][1] == t0 + 4 * len(cases):
                skip.add(before[-1].addr)
            self.switches[i.addr] = {"cases": cases, "default": dflt, "tmp": tmp, "skip": skip}
            if tmp:
                self.switch_tmps.add(tmp)

    def find_tree_switches(self, body):
        """Small/sparse switch: `mov [tmp],r; cmp [tmp],k; jcc ...` - a decision tree of compares
        on a compiler temp that nothing else touches (Watcom's code for a switch without a table)."""
        self.trees = {}
        idx = {x.addr: n for n, x in enumerate(body)}
        refs = Counter()
        for x in body:
            for o in x.ops:
                if o[0] == "m" and o[1] == "ebp" and o[2] is None and o[4] < 0:
                    refs[-o[4]] += 1
        for n, st in enumerate(body[:-2]):
            c = body[n + 1]
            if not (st.mn == "mov" and st.ops[0][0] == "m" and st.ops[0][1] == "ebp" and st.ops[0][2] is None
                    and st.ops[0][4] < 0 and st.ops[1][0] == "r" and c.mn == "cmp" and c.ops[0] == st.ops[0]
                    and c.ops[1][0] == "i"):
                continue
            tmp = st.ops[0]
            nodes, todo, inner, jmps = {}, [n + 1], set(), {}
            ok = True
            while todo and ok:
                k = todo.pop()
                if k in nodes or k in jmps:
                    continue
                x = body[k]
                if x.mn == "cmp" and x.ops[0] == tmp and x.ops[1][0] == "i" and k + 1 < len(body) and \
                        body[k + 1].mn in JREL:
                    j = body[k + 1]
                    nodes[k] = (x.ops[1][1], j.mn, j.ops[0][1], body[k + 2].addr if k + 2 < len(body) else None)
                    inner.add(x.addr)
                    for t in (j.ops[0][1], nodes[k][3]):
                        if t in idx and ((body[idx[t]].mn == "cmp" and body[idx[t]].ops[0] == tmp) or
                                         (body[idx[t]].mn == "jmp" and t == nodes[k][3])):
                            todo.append(idx[t])
                elif x.mn == "jmp" and x.ops[0][0] == "i" and k > n + 1:
                    jmps[k] = x.ops[0][1]
                    inner.add(x.addr)
                else:
                    ok = False
            if not ok or len(nodes) < 1 or refs[-tmp[4]] != len(nodes) + 1:
                continue

            def leaf(a):
                k = idx.get(a)
                if k in jmps:
                    return jmps[k]
                return None if k in nodes else a

            def run(v):
                k = n + 1
                for _ in range(64):
                    kk, jm, t, fall = nodes[k]
                    w = tmp[5]
                    uv, uk = v & ((1 << 8 * w) - 1), kk & ((1 << 8 * w) - 1)
                    sv = uv - (1 << 8 * w) if uv >> (8 * w - 1) else uv
                    sk = uk - (1 << 8 * w) if uk >> (8 * w - 1) else uk
                    a, b = (uv, uk) if jm in JUNS else (sv, sk)
                    rel = JREL[jm]
                    taken = {"==": a == b, "!=": a != b, "<": a < b, "<=": a <= b, ">": a > b, ">=": a >= b}[rel]
                    nxt = t if taken else fall
                    lf = leaf(nxt)
                    if lf is not None:
                        return lf
                    k = idx[nxt]
                return None
            ks = sorted({s32(x[0]) for x in nodes.values()})
            dflt = run(max(ks) + 1)
            at = defaultdict(list)
            for v in ks:
                t = run(v)
                if t is not None and t != dflt:
                    at[t].append(v)
            if not at or dflt is None or run(min(ks) - 1) != dflt:
                continue
            inner |= {body[k + 1].addr for k in nodes}
            self.trees[st.addr] = {"tmp": -tmp[4], "at": dict(at), "default": dflt, "inner": inner}

    def frame_parts(self):
        ins = self.ins
        lo = 0
        if [x.mn for x in ins[:6]] == ["push", "push", "push", "push", "mov", "sub"]:
            lo = 6
            self.frame = ins[5].ops[1][1]
        else:
            self.ok = False
        hi = len(ins)
        while hi > lo and ins[hi - 1].mn in ("nop",) or (hi > lo and ins[hi - 1].mn in ("mov", "lea") and ins[hi - 1].text in ("eax, eax", "eax, [eax]")):
            hi -= 1
        if hi > lo and ins[hi - 1].mn == "ret":
            hi -= 1
        for _ in range(3):
            if hi > lo and ins[hi - 1].mn == "pop":
                hi -= 1
        if hi > lo and ins[hi - 1].mn in ("leave",) or (hi > lo and ins[hi - 1].mn == "pop" and ins[hi - 1].text == "ebp"):
            hi -= 1
        epi = ins[hi].addr if hi < len(ins) else self.end
        if hi > lo and ins[hi - 1].mn == "mov" and ins[hi - 1].ops[0][0] == "r" and ins[hi - 1].ops[0][1] in ("eax", "ax", "al") \
                and ins[hi - 1].ops[1][0] == "m" and ins[hi - 1].ops[1][1] == "ebp":
            m = ins[hi - 1].ops[1]
            self.spill = (-m[4], m[5])
            hi -= 1
            epi = ins[hi].addr
        self.epi = epi
        return ins[lo:hi]

    # variables ------------------------------------------------------------------------------
    def local(self, off):
        v = self.locals.get(off)
        if v is None:
            v = self.locals[off] = Var("l", f"v_{off:x}", off)
        return v

    def arg(self, k):
        v = self.args.get(k)
        if v is None:
            v = self.args[k] = Var("a", f"a{k}", k)
        return v

    # operand access ---------------------------------------------------------------------------
    def reg(self, r):
        if r in SUBREG:
            full, w = SUBREG[r]
            return self.narrow(self.regs.get(full) or self.raw(full), w)
        if r in ("ah", "bh", "ch", "dh"):
            return self.regs.get(r) or self.raw(r)
        return self.regs.get(r) or self.raw(r)

    def raw(self, r):
        self.unknown.append(f"read of undefined register {r}")
        return E("raw", name=r)

    def narrow(self, e, w):
        if e.w <= w:
            return e
        if e.op == "k":
            v = e.v & ((1 << (8 * w)) - 1)
            return E("k", v=v, w=w)
        if e.op in ("v", "m") and e.x:            # a narrow load that was extended
            return self.load_like(e, w)
        return E("trunc", e, w=w)

    def load_like(self, e, w):
        n = E(e.op, e.a, w=w, var=e.var)
        return n

    def setreg(self, r, e):
        old = self.regs.get(SUBREG[r][0] if r in SUBREG else r)
        if old is not None and old.op == "post" and id(old) not in self.consumed and                 not any(n is old for n in e.walk()):
            self.consumed.add(id(old))          # `mov eax,[i]; inc [i]` then eax reused: `i++;`
            self.stmts.append(("expr", old))
            if old.a.var:
                old.a.var.volatile = True
        if r in SUBREG:
            self.regs[SUBREG[r][0]] = e
        else:
            self.regs[r] = e

    def mem_loc(self, op, ins, w=None):
        """Lvalue node for a memory operand (no evidence recorded)."""
        _, base, index, scale, disp, size = op
        w = w or size
        if base == "ebp" and index is None:
            if disp >= 0x14:
                k, sub = divmod(disp - 0x14, 4)
                v = self.arg(k)
                if sub:
                    return E("m", self.addr_node(E("addr", var=v), K(sub)), w=w)
                return E("v", var=v, w=w)
            if disp < 0:
                return E("v", var=self.local(-disp), w=w)
        if base is None and index is None and ins.dfix:
            t = ins.dfix
            if t.get("obj") == 3 or t.get("obj") == 2:
                return E("v", var=self.prog.gvar(t["off"], t.get("obj")), w=w)
        return E("m", self.address(op, ins), w=w)

    def address(self, op, ins):
        _, base, index, scale, disp, size = op
        terms = []
        if base:
            if base == "ebp":
                v = self.local(-disp) if disp < 0 else self.arg((disp - 0x14) // 4)
                if index:
                    v.addr = True       # indexed: an array
                terms.append(E("addr", var=v))
                disp = 0
            else:
                terms.append(self.reg(base))
        if index:
            iv = self.reg(index)
            terms.append(iv if scale == 1 else E("*", iv, K(scale)))
        if ins.dfix and ins.dfix.get("obj") in (2, 3):
            g = self.prog.gvar(ins.dfix["off"], ins.dfix.get("obj"))
            g.array = True
            terms.insert(0, E("addr", var=g))
        elif disp:
            terms.append(K(disp))
        # pointer evidence: the single unscaled non-constant term is the base
        flat = [x for t in terms for x in leaves(t)]
        cand = [t for t in flat if t.op in ("v", "m") and t.w == 4 and not t.x]
        if not any(t.op == "addr" or t.ptr for t in flat) and len(cand) > 1:
            cand = [t for t in cand if t.op == "m"][:1] if sum(t.op == "m" for t in cand) == 1 else cand
        if len(cand) == 1 and not any(t.op == "addr" for t in flat):
            c = cand[0]
            if c.op == "v":
                c.var.ptr += 1
            c.ptr = True
        e = terms[0] if terms else K(0)
        for t in terms[1:]:
            e = E("+", e, t)
        return e

    def addr_node(self, a, b):
        return E("+", a, b)

    def read(self, op, ins, how="mov", w=None):
        if op[0] == "r":
            return self.reg(op[1])
        if op[0] == "i":
            if ins.ifix:
                t = ins.ifix
                far = self.regs.get("eax") is not None and self.regs["eax"].op == "cs"
                if t.get("obj") == 1:
                    return E("fn", name=self.prog.fname(t["off"]), x="far" if far else None)
                if t.get("obj") == 2:
                    return E("fn", name=f"o2_{t['off']:x}", x="far" if far else None)
                g = self.prog.gvar(t["off"], t.get("obj"))
                g.array = True
                return E("addr", var=g)
            return K(op[1], w or op[2])
        e = self.mem_loc(op, ins)
        if e.var:
            e.var.note(e.w, how)
        for n, (loc, op) in enumerate(self.pre_pending):
            if loc.op == e.op and ((e.op == "v" and loc.var is e.var) or (e.op == "m" and same_place(loc, e))):
                del self.pre_pending[n]
                if isinstance(op, E):
                    return op
                return E("pre", loc, w=e.w, name=op)
        return e

    def pointer_sum(self, a, b, src):
        """`add edx, X` with edx a plain int load (int sums are built in eax): pointer arithmetic.
        Probes: `ptr + int` -> `mov edx,[p]; add edx,[i]`; `ptr + expr` -> expr in eax first, then
        `mov edx,[p]; add edx,eax`; `int + ptr` -> `mov eax,[p]; mov edx,[i]; add edx,eax`."""
        plain = lambda e: e.op in ("v", "m") and e.w == 4 and not e.x and (e.op == "m" or e.var.kind == "g")

        def mark(e):
            e.ptr = True
            if e.op == "m":
                pass
            else:
                self.local_ptr.add(e.var)
                e.var.psum += 1
        if not plain(a):
            return
        if src[0] == "i":
            # `mov edx,[p]; mov eax,[q]; add edx,imm`: the constant is added after another load
            if a.op == "v" and any(e is not None and e is not a and e.seq > a.seq and e.op in ("v", "m")
                                   for e in self.regs.values()):
                mark(a)
            return
        if src[0] == "m":
            mark(a)
        elif plain(b) and b.seq < a.seq:
            mark(b)
        elif plain(b) and a.op == b.op:
            mark(a)
        elif b.op not in ("v", "m", "k", "ext") and b.seq < a.seq:
            mark(a)

    def live_values(self, exclude=()):
        return any(e is not None and e.op not in ("raw", "cs") and id(e) not in self.consumed and id(e) not in exclude
                   for e in self.regs.values())

    # statements -------------------------------------------------------------------------------
    def emit(self, st, uses=()):
        used = set()
        for u in uses:
            if isinstance(u, E):
                used |= {id(n) for n in u.walk()}
        self.consumed |= used
        # calls that ran earlier and whose value is not used here become statements first
        keep = []
        live = set()
        for r, e in self.regs.items():
            if r != "eax" and e is not None:
                live |= {id(n) for n in e.walk()}
        for c in self.pending:
            if id(c) in used:
                continue
            if id(c) in live:
                keep.append(c)
                continue
            self.stmts.append(("expr", c))
        self.pending = keep
        if self.bf_pending:
            loc, k, uns = self.bf_pending
            self.bf_pending = None
            self.stmts.append(("opset", loc, "&", K(k, loc.w), uns))
        while self.pre_pending:
            loc, op = self.pre_pending.pop(0)
            if isinstance(op, E):
                self.stmts.append(("set", loc, op.b))
            else:
                self.stmts.append(("incdec", loc, op))
        for r, e in list(self.regs.items()):
            if e is not None and e.op == "post" and id(e) not in used and id(e) not in self.consumed:
                self.consumed.add(id(e))
                self.stmts.append(("expr", e))
                if e.a.var:
                    e.a.var.volatile = True
                self.regs[r] = None
        if st:
            self.stmts.append(st)

    def flush(self):
        self.emit(None)
        for c in self.pending:
            self.stmts.append(("expr", c))
        self.pending = []

    def store(self, dst, val, ins):
        loc = self.mem_loc(dst, ins)
        if loc.op == "v" and loc.var.kind == "l" and loc.var.key in self.switch_tmps:
            self.switch_val[loc.var.key] = val     # the switch expression's compiler temp
            del self.locals[loc.var.key]
            return
        if loc.op == "v" and loc.var.kind == "l" and self.spill and loc.var.key == self.spill[0]:
            self.emit(("ret", val), [val])
            self.retval = True
            return
        if loc.var:
            how = "const" if val.op == "k" else "store"
            loc.var.note(loc.w, how)
            if loc.var.kind in ("l", "a") and loc.w == 4 and val.w == 2 and not val.x and val.op != "k":
                loc.var.note(2, "nval")
            if loc.var.kind in ("l", "a") and loc.w == 4 and val.w == 4 and not val.x and val.op in ("v", "m"):
                loc.var.note(4, "wval")
        mine = {id(n) for n in loc.walk()} | {id(n) for n in val.walk()}
        nx = self.body[self.pos + 1] if self.pos + 1 < len(self.body) else None
        embedded = nx is not None and nx.mn in ("cmp", "add", "sub", "and", "or", "xor", "imul") and             len(nx.ops) == 2 and nx.ops[1] == dst and nx.ops[0][0] == "r" and             self.regs.get(nx.ops[0][1]) is not None and id(self.regs[nx.ops[0][1]]) not in mine | self.consumed
        if loc.op == "v" and embedded and not self.pending:
            # `edx = a + b; ebx = c + d; mov [v], ebx; cmp edx, [v]`: an assignment inside the
            # expression; it becomes `(v = c + d)` where v is read next
            self.pre_pending.append((loc, E("asg", loc, val, w=val.w)))
            return
        self.emit(("set", loc, val), [loc, val])

    def run(self):
        body = self.frame_parts()
        self.regs, self.pending, self.pushes = {}, [], []
        self.flags = None
        self.consumed = set()
        self.flags_same = False
        self.bf_pending = None
        self.switch_val = {}
        self.copy = None
        self.local_ptr = set()
        self.local_int = set()
        self.pre_pending = []
        self.last_load = None
        self.retval = False
        targets = set()
        for i in body:
            if i.mn.startswith("j") and i.ops and i.ops[0][0] == "i":
                targets.add(i.ops[0][1])
        for sw in self.switches.values():
            targets.update(sw["cases"])
            if sw["default"]:
                targets.add(sw["default"])
        self.find_tree_switches(body)
        for tree in self.trees.values():
            targets -= tree["inner"]
            targets |= set(tree["at"]) | {tree["default"]}
        self.targets = targets
        n = 0
        self.body = body
        while n < len(body):
            self.pos = n
            i = body[n]
            self.cur = i.addr
            if i.addr in targets:
                self.flush()
                self.stmts.append(("label", i.addr))
                self.regs = {}
                self.flags = None
            nxt = body[n + 1] if n + 1 < len(body) else None
            if self.flags and self.flags[0] in ("cmp", "test") and not (i.mn.startswith("j") and i.mn != "jmp"):
                self.dangling_cmp()
            n += self.step(i, nxt)
        if self.flags and self.flags[0] in ("cmp", "test"):
            self.dangling_cmp()
        self.flush()
        if self.epi in targets:
            self.stmts.append(("label", self.epi))
        self.nparams = max(self.args, default=-1) + 1
        return self

    def step(self, i, nxt):
        mn, ops = i.mn, i.ops
        if i.addr in self.trees:
            tree = self.trees[i.addr]
            val = self.read(ops[1], i)
            if ops[1][0] == "r" and ops[1][2] < 4:
                val = self.narrow(val, ops[1][2])
            self.emit(("switch", val, tree["at"], tree["default"]), [val])
            self.flush()
            self.regs = {}
            self.switch_tmps.add(tree["tmp"])
            self.locals.pop(tree["tmp"], None)
            return 1
        if any(i.addr in t["inner"] for t in self.trees.values()):
            return 1
        if any(i.addr in sw["skip"] for sw in self.switches.values()):
            if i.mn.startswith("j") and i.mn != "jmp":
                self.targets.discard(i.ops[0][1]) if False else None
            return 1
        if mn == "nop":
            return 1
        if mn == "mov" and ops[1][0] == "r" and ops[1][1] == "cs":
            self.regs[ops[0][1]] = E("cs")      # segment half of a far function address
            return 1
        if mn in ("mov", "movsx", "movzx"):
            dst, src = ops
            if dst[0] == "r":
                if src[0] == "m":
                    how = {"mov": "mov", "movsx": "sx", "movzx": "zx"}[mn]
                    e = self.read(src, i, how)
                    self.last_load = e
                    if mn != "mov":
                        e = self.extend(e, "s" if mn == "movsx" else "z", dst[2])
                elif mn == "mov":
                    e = self.read(src, i, w=dst[2])
                else:
                    e = self.extend(self.reg(src[1]), "s" if mn == "movsx" else "z", dst[2])
                    e = self.retyped(e, src[2], "s" if mn == "movsx" else "z")
                if dst[1] in ("ah", "bh", "ch", "dh"):
                    self.regs[dst[1]] = e
                else:
                    self.setreg(dst[1], e)
            else:
                val = self.read(src, i, w=dst[5])
                if src[0] == "r" and src[2] < 4:
                    val = self.narrow(val, src[2])
                self.store(dst, val, i)
            return 1
        if mn == "lea":
            dst, src = ops
            self.setreg(dst[1], self.lea(src, i))
            return 1
        if mn == "push":
            op = ops[0]
            e = self.read(op, i, "push")
            if op[0] == "i" and not i.ifix:
                form = "wide"
            elif op[0] == "r" and e.op == "k":
                form = "narrow16" if e.v & 0xFFFFFFFF > 0xFF else "narrow"
            elif op[0] == "r" and e.op == "v" and e.w == 4 and not e.x and e.var.kind in ("a", "g", "l") and                     self.last_load is e:
                e.var.volatile = True
                form = "wide"
            elif op[0] == "r" and e.op in ("v", "m") and e.x and e.var and 4 in e.var.widths("mov", "store", "const", "arith", "cmp", "rmw"):
                form = "narrow"
            elif op[0] == "m":
                form = "wide"
            elif e.op in ("addr", "fn"):
                form = "ptr"
            else:
                form = "any"
            self.pushes.append((e, form))
            return 1
        if mn == "call":
            return self.call(i, nxt)
        if mn in ("add", "sub") and ops[0][0] == "r" and ops[0][1] == "esp":
            return 1
        if mn in ("cmp", "test"):
            if mn == "test" and ops[0][0] == "m" and ops[0][5] > 1 and ops[1][0] == "i" and 0 <= ops[1][1] < 0x100:
                loc = self.mem_loc(ops[0], i)
                if loc.op == "v":
                    loc.var.volatile = True     # Watcom narrows `x & 2` to a byte test unless volatile
            high = {"ah": "eax", "bh": "ebx", "ch": "ecx", "dh": "edx"}.get(ops[0][1]) if ops[0][0] == "r" else None
            if mn == "test" and high and ops[1][0] == "i" and self.regs.get(high) is not None:
                # `test ah, 1` on a full-width value: `x & 0x100`
                full = self.regs[high]
                self.flags = ("test", full, K((ops[1][1] & 0xFF) << 8, full.w), False)
                return 1
            a = self.read(ops[0], i, "cmp")
            b = self.read(ops[1], i, "cmp", w=a.w if ops[1][0] == "i" else None)
            if ops[1][0] == "i" and ops[0][0] in ("r", "m"):
                w = ops[0][2] if ops[0][0] == "r" else ops[0][5]
                b = K(b.v, w) if b.op == "k" else b
                if mn == "cmp" and w == 2 and b.op == "k" and b.v & 0x8000 and a.op in ("v", "m"):
                    if a.op == "v":
                        a.var.sign[False] += 3
                    else:
                        a.x, a.typ, a.w = "z", 2, 2
                if w < 4 and b.op == "k":
                    b.v = b.v & ((1 << (8 * w)) - 1)
            if ops[0][0] == "r" and ops[0][2] < 4:
                a = self.narrow(a, ops[0][2])
            if ops[1][0] == "r" and ops[1][2] < 4:
                b = self.narrow(b, ops[1][2])
            self.flags = (mn, a, b, ops[0][0] == "r" and ops[1][0] == "r" and ops[0][1] == ops[1][1])
            self.flags_same = self.flags[3] and mn == "test"
            return 1
        if mn.startswith("j") and mn != "jmp":
            self.branch(i)
            return 1
        if mn == "jmp":
            self.jump(i)
            return 1
        if mn == "sbb" and ops[0][0] == "r" and ops[1][0] == "r":
            # `mov edx,x; sar edx,31; shl edx,k; sbb eax,edx; sar eax,k`: signed x / 2**k
            a, b = self.reg(ops[0][1]), self.reg(ops[1][1])
            if b.op == "<<" and b.b.op == "k" and b.a.op == ">>" and b.a.b.op == "k" and b.a.b.v == 31                     and b.a.a.key() == a.key():
                self.setreg(ops[0][1], E("sbbdiv", a, v=b.b.v))
                return 1
        if mn in ARITH or mn in ("inc", "dec", "neg", "not"):
            return self.arith(i, nxt)
        if mn == "cdq":
            self.regs["edx"] = E(">>", self.reg("eax"), K(31))
            return 1
        if mn in ("movsb", "movsw", "movsd", "rep movsb", "rep movsw", "rep movsd"):
            unit = {"b": 1, "w": 2, "d": 4}[mn[-1]]
            if self.copy is None:
                self.copy = [self.regs.get("edi") or self.raw("edi"), self.regs.get("esi") or self.raw("esi"), 0]
            cnt = 1
            if mn.startswith("rep"):
                c = self.regs.get("ecx")
                cnt = c.v if c is not None and c.op == "k" else 0
            self.copy[2] += unit * cnt
            if not (nxt and nxt.mn.lstrip("rep ").startswith("movs")):
                d, s_, n = self.copy
                self.copy = None
                self.emit(("copy", d, s_, n), [d, s_])
                self.regs.pop("esi", None)
                self.regs.pop("edi", None)
                self.regs.pop("ecx", None)
            return 1
        if mn in ("idiv", "div"):
            s = self.read(ops[0], i, "arith")
            a = self.reg("eax")
            uns = mn == "div"
            self.mark_sign(a, not uns)
            self.mark_sign(s, not uns)
            self.regs["eax"] = E("/", a, s, uns=uns)
            self.regs["edx"] = E("%", a, s, uns=uns)
            return 1
        if mn == "mul" and ops[0][0] == "r" and ops[0][1] == "ah":
            self.regs["eax"] = E("*", self.reg("al"), self.regs.get("ah") or self.raw("ah"), w=1)
            return 1
        self.unknown.append(f"{i.addr:x}: {mn} {i.text}")
        self.emit(("asm", f"{mn} {i.text}"))
        return 1

    def extend(self, e, x, w=4):
        if w == 2:
            n = E("ext", e, w=2, x=x)       # movzx ax, byte: an explicit (unsigned short) cast
            n.typ = e.w
            return n
        if e.op == "ext" and e.w == 2 and e.x == x:
            n = E("ext", e.a, w=4, x=x)
            n.typ, n.b = e.typ, "16"
            return n
        if e.op in ("v", "m"):
            n = E(e.op, e.a, w=4, x=x, var=e.var)
            n.typ = e.w
            return n
        n = E("ext", e, w=4, x=x)
        n.typ = e.w
        return n

    def retyped(self, e, w, x):
        return e

    def lea(self, src, i):
        _, base, index, scale, disp, size = src
        if base == "ebp" and index is not None:
            return self.address(src, i)
        if base == "ebp" and index is None:
            if disp < 0:
                v = self.local(-disp)
                v.addr = True
                return E("addr", var=v)
            v = self.arg((disp - 0x14) // 4)
            v.addr = True
            return E("addr", var=v)
        if i.dfix:
            return self.address(src, i)
        terms = []
        if base and index and base == index:
            e = E("*", self.reg(base), K(scale + 1))
        else:
            e = None
            if base:
                e = self.reg(base)
            if index:
                t = self.reg(index)
                t = t if scale == 1 else E("*", t, K(scale))
                e = t if e is None else E("+", e, t)
        if disp:
            e = K(disp) if e is None else E("+", e, K(disp))
        return e

    def mark_sign(self, e, signed):
        for n in (e,):
            if n.op in ("v",) and n.var:
                n.var.sign[signed] += 1

    def arith(self, i, nxt):
        mn, ops = i.mn, i.ops
        dst = ops[0]
        if mn in ("inc", "dec", "neg", "not"):
            if dst[0] == "r":
                if dst[1] in ("ah", "bh", "ch", "dh"):
                    self.unknown.append(f"{i.addr:x}: {mn} {i.text}")
                    return 1
                w = dst[2]
                a = self.reg(dst[1])
                if mn in ("inc", "dec"):
                    e = E("+" if mn == "inc" else "-", a, K(1), w=w)
                elif mn == "neg":
                    e = E("neg", a, w=w)
                else:
                    e = E("~", a, w=w)
                self.setreg(dst[1], e)
                self.flags = ("res", e, None, False)
                return 1
            loc = self.mem_loc(dst, i)
            if loc.var:
                loc.var.note(loc.w, "rmw")
            if mn in ("inc", "dec") and self.post_incdec(loc, "++" if mn == "inc" else "--"):
                pass
            elif mn in ("inc", "dec") and self.live_values():
                # `mov eax,[a]; sub eax,20h; inc [b]; cmp eax,[b]`: ++b inside the expression
                self.pre_pending.append((loc, "++" if mn == "inc" else "--"))
            elif mn in ("inc", "dec"):
                self.emit(("incdec", loc, "++" if mn == "inc" else "--"), [loc])
            else:
                self.emit(("set", loc, E("neg" if mn == "neg" else "~", loc, w=loc.w)), [loc])
            self.flags = ("res", loc, None, False)
            return 1
        if mn == "imul" and len(ops) == 3:
            a = self.read(ops[1], i, "arith")
            e = E("*", a, K(ops[2][1]))
            self.setreg(dst[1], e)
            return 1
        src = ops[1]
        op = ARITH[mn]
        uns = mn == "shr"
        if dst[0] == "r":
            if src[0] == "r" and src[1] == dst[1] and mn in ("xor", "sub"):
                self.setreg(dst[1], K(0, dst[2]))
                return 1
            high = {"ah": "eax", "bh": "ebx", "ch": "ecx", "dh": "edx"}.get(dst[1])
            if high and mn in ("or", "xor", "and") and src[0] == "i" and self.regs.get(high) is not None \
                    and self.regs[high].w >= 2:
                # `and ah, 0efh` on a full-width value: a mask on its second byte
                full = self.regs[high]
                k = (src[1] & 0xFF) << 8
                if mn == "and":
                    k |= ((1 << (8 * full.w)) - 1) ^ 0xFF00
                e = E(op, full, K(k), w=full.w)
                self.regs[high] = e
                self.flags = ("res", e, None, False)
                return 1
            a = self.reg(dst[1])
            full = self.regs.get(SUBREG[dst[1]][0]) if dst[1] in SUBREG else None
            if src[0] == "r" and src[1] == dst[1] and mn == "add":
                e = E("*", a, K(2), w=dst[2])
            elif mn in ("or", "xor", "and") and src[0] == "i" and dst[2] < 4 and full is not None and full.w > dst[2]:
                # `or al, 13h` / `and al, 0fch` on a full-width value: Watcom's short form of a
                # 32-bit operation whose constant leaves the upper bytes alone
                k = src[1] & ((1 << (8 * dst[2])) - 1)
                if mn == "and":
                    k |= ((1 << (8 * full.w)) - 1) ^ ((1 << (8 * dst[2])) - 1)
                e = E(op, full, K(k), w=full.w)
                self.regs[SUBREG[dst[1]][0]] = e
                self.flags = ("res", e, None, False)
                return 1
            else:
                b = self.read(src, i, "arith")
                if mn in ("shl", "sal", "sar", "shr") and src[0] == "r" and src[1] == "cl" and                         (self.regs.get("ecx") is not None):
                    b = self.regs["ecx"]
                elif src[0] == "r" and src[2] < 4:
                    b = self.narrow(b, src[2])
                if mn in ("shl", "sal", "sar", "shr") and b.op in ("v", "m") and b.w == 1 and not b.x:
                    # `mov cl, byte [x]`: Watcom narrows the load of an int shift count
                    if b.var:
                        b.var.acc[(1, "mov")] -= 1
                        b.var.note(4, "mov")
                    b = E(b.op, b.a, w=4, var=b.var)
                if dst[2] < 4:
                    a = self.narrow(a, dst[2])
                if mn == "add" and dst[1] != "eax" and dst[2] == 4:
                    self.pointer_sum(a, b, src)
                elif mn == "add" and dst[2] == 4 and src[0] == "r":
                    # `shl eax,4; mov edx,[p]; add eax,edx`: an int operand would be a memory operand
                    plain = b.op == "v" and b.w == 4 and not b.x and b.var.kind == "g"
                    if plain and a.op not in ("v", "m", "k") and b.seq > max(n.seq for n in a.walk())                             and not any(n.op == "call" for n in a.walk()):
                        self.local_ptr.add(b.var)
                        b.var.psum += 1
                    elif plain and a.op == "call" and b.seq > a.seq:
                        self.local_int.add(b.var)     # `call f; mov edx,[g]; add eax,edx`: int g
                elif mn == "add" and dst[2] == 4 and src[0] == "m" and dst[1] == "eax" and b.op == "v" and                         b.var.kind == "g" and not b.x:
                    self.local_int.add(b.var)         # `add eax,[g]`: g is an int here
                if op in ("+", "*", "&", "|", "^") and src[0] == "r" and commute(a, b):
                    a, b = b, a
                e = E(op, a, b, uns=uns, w=dst[2])
                if mn in ("sar", "shr"):
                    self.mark_sign(a, mn == "sar")
            e = bitfield_read(simplify(e))
            self.setreg(dst[1], e)
            self.flags = ("res", e, None, False)
            return 1
        loc = self.mem_loc(dst, i)
        if mn == "or" and src[0] == "r" and self.bf_pending and self.bitfield_store(loc, self.reg(src[1])):
            return 1
        if loc.var:
            loc.var.note(loc.w, "rmw")
            if mn in ("sar", "shr"):
                loc.var.sign[mn == "sar"] += 1
        b = self.read(src, i, "arith", w=loc.w)
        if src[0] == "r" and src[2] < 4:
            b = self.narrow(b, src[2])
        if b.op == "k" and loc.w < 4:
            b = K(b.v & ((1 << (8 * loc.w)) - 1), loc.w)
        if b.op == "k" and loc.w == 4 and loc.var and mn in ("and", "or", "xor"):
            hi = (b.v & 0xFFFF0000) >> 16
            if (mn == "and" and hi == 0xFFFF) or (mn != "and" and hi == 0):
                loc.var.note(4, "rmwn")   # Watcom would narrow this for a plain int: volatile/short
        if mn in ("add", "sub") and b.op == "k" and loc.op == "v" and loc.w == 4 and 1 < b.v < 0x10000                 and self.post_incdec(loc, "++" if mn == "add" else "--"):
            loc.var.stride = b.v    # `mov eax,[p]; add [p],18h`: p++ on a pointer to 24-byte records
            return 1
        if mn == "and" and b.op == "k" and self.live_values({id(n) for n in loc.walk()}) and not self.bf_pending:
            # possibly the clearing half of a bitfield store (`and byte [m],~mask; or [m],val`)
            self.bf_pending = (loc, b.v, uns)
            return 1
        self.emit(("opset", loc, op, b, uns), [loc, b])
        self.flags = ("res", loc, None, False)
        return 1

    def bitfield_store(self, loc, val):
        """`and byte [m],~(mask<<o)` (deferred) then `or [m],(v & mask) << o`: m->field = v."""
        cl, k, _ = self.bf_pending
        u = loc.w
        delta = 0
        if cl.op == "m" and loc.op == "m":
            lc = [t for t in leaves(cl.a) if t.op != "k"]
            ll = [t for t in leaves(loc.a) if t.op != "k"]
            if sorted(map(id, lc)) != sorted(map(id, ll)) and not same_place(E("m", cl.a, w=u), loc):
                if [t.key() for t in lc] != [t.key() for t in ll]:
                    return False
            delta = sum(t.v for t in leaves(cl.a) if t.op == "k") - sum(t.v for t in leaves(loc.a) if t.op == "k")
        elif not (cl.op == loc.op == "v" and cl.var is loc.var):
            return False
        cleared = ((~k) & ((1 << (8 * cl.w)) - 1)) << (8 * delta)
        if cleared == 0 or delta < 0 or cleared >> (8 * u):
            return False
        o = (cleared & -cleared).bit_length() - 1
        w = bin(cleared).count("1")
        if cleared != ((1 << w) - 1) << o:
            return False
        v = val
        if o:
            if v.op == "<<" and v.b.op == "k" and v.b.v == o:
                v = v.a
            elif o == 1 and v.op == "*" and v.b.op == "k" and v.b.v == 2:
                v = v.a
            else:
                return False
        if v.op == "&" and v.b.op == "k" and v.b.v & ((1 << (8 * u)) - 1) == (1 << w) - 1:
            v = v.a
        self.bf_pending = None
        self.emit(("bfset", loc, o, w, v), [loc, v])
        self.flags = None
        return True

    def post_incdec(self, loc, op):
        """`mov eax,[x]; inc [x]` with eax used later: the expression `x++`."""
        for r, e in self.regs.items():
            if e is None:
                continue
            for n in e.walk():
                if id(n) in self.consumed:
                    continue
                if n.op == loc.op and n.w == loc.w and not n.x and (
                        (n.op == "v" and n.var is loc.var) or
                        (n.op == "m" and (same_place(n, loc) or n.a.key() == loc.a.key()))):
                    n.op, n.a, n.name = "post", E(loc.op, loc.a, w=loc.w, var=loc.var), op
                    return True
        return False

    def call(self, i, nxt):
        nargs = 0
        consumed = 1
        # the caller pops the arguments, possibly after saving eax (`mov edx,eax; add esp,4`)
        for j in range(self.pos + 1, min(self.pos + 4, len(self.body))):
            x = self.body[j]
            if x.mn == "add" and x.ops[0][0] == "r" and x.ops[0][1] == "esp" and x.ops[1][0] == "i":
                nargs = x.ops[1][1] // 4
                break
            if not (x.mn == "mov" and x.ops[0][0] == "r" and x.ops[1][0] == "r" and x.ops[1][1] == "eax"):
                break
        take = self.pushes[len(self.pushes) - nargs:] if nargs else []
        if nargs:
            del self.pushes[len(self.pushes) - nargs:]
        args = [e for e, f in reversed(take)]
        forms = [f for e, f in reversed(take)]
        op = i.ops[0]
        if op[0] == "i":
            name = self.prog.fname(op[1])
            self.prog.sigs[name]["calls"].append(forms)
            target = E("fn", name=name)
        else:
            target = self.read(op, i, "mov")
            if target.var:
                target.var.fnptr = True
            target.ptr = False
        c = E("call", target, args=args)
        c.name = forms
        self.emit(None, [c])
        self.regs = {k: v for k, v in self.regs.items() if k in ("ebx", "esi", "edi")}
        self.regs["eax"] = c
        self.pending.append(c)
        self.flags = None
        return consumed

    def cond(self, mn):
        kind, a, b, same = self.flags or ("res", self.raw("flags"), None, False)
        rel = JREL.get(mn)
        if rel is None:
            self.unknown.append(f"condition {mn}")
            rel = "!="
        uns = mn in JUNS
        if kind == "cmp":
            if rel in ("<", "<=", ">", ">="):
                self.mark_sign(a, not uns)
                self.mark_sign(b, not uns)
            return E("cmp", a, b, rel=rel, uns=uns)
        if kind == "test":
            if same:
                if rel in ("<", ">=") and mn in ("jl", "jge", "js", "jns"):
                    self.mark_sign(a, True)
                return E("cmp", a, K(0, a.w), rel=rel, uns=uns)
            return E("cmp", E("&", a, b, w=a.w), K(0), rel=rel, uns=uns)
        return E("cmp", a, K(0), rel=rel, uns=uns)

    def fuse_asg(self, c):
        if not self.stmts or c.op != "cmp" or c.b.op != "k" or c.b.v != 0 or c.a.op not in ("v", "m"):
            return
        last = self.stmts[-1]
        if last[0] == "set" and last[1].op == c.a.op and last[1].key() == E(c.a.op, c.a.a, w=c.a.w, var=c.a.var).key() \
                and self.flags_same:
            self.stmts.pop()
            c.a = E("asg", last[1], last[2], w=c.a.w)

    def fuse_pre(self, c):
        """`dec [eax+4]; cmp [eax+4],0` with the address register reused: `if (--p[1] == 0)`."""
        if not self.stmts or c.op != "cmp" or c.a.op != "m":
            return
        last = self.stmts[-1]
        if last[0] not in ("incdec", "opset") or last[1].op != "m":
            return
        if same_place(last[1], c.a):
            self.stmts.pop()
            if last[0] == "incdec":
                c.a = E("pre", last[1], w=c.a.w, name=last[2])
            else:
                c.a = E("preop", last[1], last[3], w=c.a.w, name=last[2])

    def dangling_cmp(self):
        """A compare without a branch: `if (x) {}` (Watcom still emits the compare under -od)."""
        c = self.cond("jne")
        self.emit(("if", c, None), [c])
        self.flags = None

    def branch(self, i):
        target = i.ops[0][1]
        c = self.cond(i.mn)
        self.fuse_pre(c)
        self.fuse_asg(c)
        self.emit(("if", c, target), [c])
        self.flush()
        self.flags = None

    def jump(self, i):
        if i.addr in self.switches:
            sw = self.switches[i.addr]
            val = self.switch_val.get(sw["tmp"])
            if val is None:
                m = i.ops[0]
                val = self.reg(m[2]) if m[2] else self.reg(m[1])
                if val.op in ("*", "<<") and val.b.op == "k" and val.b.v in (4, 2):
                    val = val.a
            base = 0
            if val.op in ("-", "+") and val.b.op == "k":
                base = val.b.v if val.op == "-" else -val.b.v
                val = val.a
            at = defaultdict(list)
            for n, t in enumerate(sw["cases"]):
                at[t].append(n + base)
            self.emit(("switch", val, dict(at), sw["default"]), [val])
            self.flush()
            self.regs = {}
            return
        if i.ops[0][0] != "i":
            self.unknown.append(f"{i.addr:x}: jmp {i.text}")
            self.emit(("asm", f"jmp {i.text}"))
            return
        target = i.ops[0][1]
        self.flush()
        if target == self.epi and (self.spill is None or self.stmts and self.stmts[-1][0] == "ret"):
            if self.spill is None:
                self.stmts.append(("ret", None))
            # a value return: the jmp belongs to the preceding return statement
            self.regs = {}
            return
        self.stmts.append(("goto", target))
        self.regs = {}


def commute(dst, src):
    """Source order of a register-register commutative op (`op dst, src`); True = src is the left
    operand.  Watcom puts the left operand in dst except (probes) when the right operand is an
    array element loaded after the left one, or when two converted loads are combined (the first
    loaded is the left)."""
    simple = lambda e: e.op in ("v", "m")
    calls = lambda e: [n.seq for n in e.walk() if n.op == "call"]
    if calls(dst) and calls(src):
        return min(calls(src)) < min(calls(dst))     # calls run in source order
    indexed = dst.op == "m" and any(n.op in ("*", "<<") for n in dst.a.walk())
    if indexed and simple(src) and src.seq < dst.seq:
        return True
    if simple(dst) and simple(src) and dst.x and src.x and src.seq < dst.seq:
        return True
    return False


def bitfield_read(e):
    """Watcom's unsigned bitfield extraction: `shl r,32-o-w; shr r,32-w` on a dword, or a byte-wide
    `shr al,o` of a loaded byte (C would promote before shifting)."""
    if e.op == ">>" and e.b.op == "k" and e.a.op == "<<" and e.a.b.op == "k" and e.w == 4:
        ld, k1, k2 = e.a.a, e.a.b.v, e.b.v
        if ld.op in ("v", "m") and ld.w == 4 and not ld.x and k2 >= k1:
            return E("bf", ld, v=(k2 - k1, 32 - k2), w=4, uns=e.uns)
    if e.op == ">>" and e.b.op == "k" and e.w == 1 and e.a.op in ("v", "m") and e.a.w == 1 and not e.a.x:
        return E("bf", e.a, v=(e.b.v, 8 - e.b.v), w=1, uns=e.uns)
    if e.op == "&" and e.b.op == "k" and e.a.op == "bf" and e.a.w == 1:
        o, w = e.a.v
        m = e.b.v & 0xFF
        if m and m & (m + 1) == 0 and m.bit_length() < w:
            return E("bf", e.a.a, v=(o, m.bit_length()), w=1, uns=e.a.uns)
    return e


def leaves(e):
    if e.op == "+":
        return leaves(e.a) + leaves(e.b)
    return [e]


def same_place(x, y):
    """Two memory operands that address the same place through the same register values."""
    if x.w != y.w:
        return False
    lx, ly = leaves(x.a), leaves(y.a)
    kx = sorted(t.v for t in lx if t.op == "k")
    ky = sorted(t.v for t in ly if t.op == "k")
    ix = sorted(id(t) for t in lx if t.op != "k")
    iy = sorted(id(t) for t in ly if t.op != "k")
    return kx == ky and ix == iy and bool(ix)


def simplify(e):
    if e.op == ">>" and e.a.op == "sbbdiv" and e.b.op == "k" and e.b.v == e.a.v:
        return E("/", e.a.a, K(1 << e.a.v))
    # (x - (x >> 31)) >> 1  ==  x / 2  (signed division by two)
    if e.op == ">>" and not e.uns and e.b.op == "k" and e.b.v == 1 and e.a.op == "-":
        x, s = e.a.a, e.a.b
        if s.op == ">>" and s.b.op == "k" and s.b.v == 31 and s.a.key() == x.key():
            return E("/", x, K(2))
    return e


# ---------------------------------------------------------------------------------------------
class Render:
    def __init__(self, lift, structured=True):
        self.L, self.prog = lift, lift.prog
        self.structured = structured
        self.used_globals, self.used_funcs, self.includes = {}, {}, set()
        self.tcache = {}
        self.bitfields = {}
        self.switches = []
        self.structs = set()
        self.far = set()

    def prepare(self):
        """Per-file prototypes: this function's call sites decide narrow/wide parameters (the
        original TUs did not always agree) and whether a void callee must return int here."""
        forms, used, wide = defaultdict(list), set(), set()
        for st in self.L.stmts:
            for x in st[1:]:
                if not isinstance(x, E):
                    continue
                if x.op == "call" and x.a.op == "fn" and st[0] != "expr":
                    wide.add(x.a.name)
                for n in x.walk():
                    for kid in n.kids():
                        if kid.op == "call" and kid.a.op == "fn" and n.op not in ("trunc", "ext"):
                            wide.add(kid.a.name)
                    if n.op == "call" and n.a.op == "fn":
                        forms[n.a.name].append(n.name or [])
                        if not (st[0] == "expr" and x is n):
                            used.add(n.a.name)
        self.protos = {}
        for name, fl in forms.items():
            sig = self.prog.sigs.get(name) or {"params": [], "ret": "i"}
            params = list(sig.get("params") or [])
            if not sig.get("lib"):
                for k in range(len(params)):
                    fs = Counter(f[k] for f in fl if k < len(f))
                    if fs["wide"] and params[k] in ("c", "sc", "s", "us"):
                        params[k] = "i"
                    elif fs["narrow16"] and params[k] not in ("s", "us"):
                        params[k] = "s"
                    elif fs["narrow"] and params[k] not in ("c", "sc", "s", "us"):
                        params[k] = "s"
            void = sig.get("void") and name not in used
            ret = sig.get("ret") or "i"
            if name in wide and ret in ("c", "sc", "s", "us") and not sig.get("lib"):
                ret = "i"
            self.protos[name] = {"params": params, "void": void, "ret": ret,
                                 "varargs": sig.get("varargs"), "lib": sig.get("lib")}

    # types -----------------------------------------------------------------------------------
    def ty(self, e):
        """C type code of the rendered expression."""
        op = e.op
        if id(e) in self.tcache:
            return self.tcache[id(e)]
        if op == "k":
            return "i"
        if op == "v":
            return self.load_type(e)[0]
        if op == "m":
            return self.mem_type(e)
        if op in ("addr", "fn"):
            return "i" if e.x == "far" else "p"
        if op == "call":
            if e.a.op == "fn":
                sig = self.protos.get(e.a.name) or self.prog.sigs.get(e.a.name)
                return (sig and sig.get("ret")) or "i"
            return "i"
        if op in ("trunc",):
            return tcode(e.w, False)
        if op == "ext":
            return "i"
        if op in ("neg", "~"):
            return promote(self.ty(e.a))
        if op in ("pre", "preop", "post", "asg"):
            return self.lv_type(e.a)
        if op == "bf":
            return "i"
        if op == "cmp":
            return "i"
        if op in ("raw", "cs"):
            return "i"
        ta, tb = promote(self.ty(e.a)), promote(self.ty(e.b))
        if op in ("+", "-"):
            if ta == "p" and tb == "p":
                return "i" if op == "-" else "p"
            if ta == "p" or tb == "p":
                return "p"
        if op in ("<<", ">>"):
            return ta if ta != "p" else "u"
        if "u" in (ta, tb) or "p" in (ta, tb):
            return "u"
        return "i"

    def load_type(self, e):
        """(type, needs-cast) for a var load of width e.typ/e.w."""
        v = e.var
        vt = v.type or "i"
        aw = e.typ if e.x else e.w
        if (vt.startswith("arr") or getattr(v, "agg", None)) and e.ptr and aw == 4:
            return "p", True
        if vt.startswith("arr") or getattr(v, "agg", None) or vt not in TW:
            return tcode(aw, e.x == "s" or (not e.x and aw > 1)), True
        if TW[vt] == aw:
            if e.x == "s" and vt not in SIGNED:
                return tcode(aw, True), True
            if e.x == "z" and vt in SIGNED:
                return tcode(aw, False), True
            return vt, False
        if aw < TW[vt]:
            if v.kind == "l" or v.kind == "a":
                pass
            return tcode(aw, e.x == "s" if e.x else vt in SIGNED), True
        return vt, False   # dword access of a narrow local/arg (plain move)

    def mem_type(self, e):
        aw = e.typ if e.x else e.w
        if aw == 4 and e.ptr:
            return "p"
        if aw == 4:
            return "i"
        return tcode(aw, e.x == "s" if e.x else aw == 2)

    # expressions -----------------------------------------------------------------------------
    def gname(self, v):
        self.used_globals[v.name] = v
        return v.name

    def lvalue(self, e):
        if e.op == "v":
            t = self.lv_type(e)
            return self.var_access(e, t) or self.vname(e.var)
        return self.deref(e, self.lv_type(e))

    def vname(self, v):
        return self.gname(v) if v.kind == "g" else v.name

    def var_access(self, e, t):
        """Text for an access of width/type t to var e.var when it is not a plain use of the
        variable (aggregate member, byte array, width mismatch); None otherwise."""
        v = e.var
        agg = getattr(v, "agg", None)
        if agg and agg[0] is not v:
            base, delta = agg
            return f"*({TNAME[t]} *)({base.name} + {delta})"
        if v.type and v.type.startswith("arr"):
            return f"*({TNAME[t]} *){self.vname(v)}"
        aw = e.typ if e.x else e.w
        if v.kind == "g" and TW[v.type] != aw:
            return f"*({TNAME[t]} *)&{self.vname(v)}"
        return None

    def lv_type(self, e):
        if e.op == "v":
            vt = e.var.type
            if vt in TW and not getattr(e.var, "agg", None) and TW[vt] == e.w:
                return vt
            if vt in TW and not getattr(e.var, "agg", None) and e.var.kind != "g" and e.w == 4:
                return vt
            return tcode(e.w, e.w != 1)
        return self.mem_type(e)

    def deref(self, e, t):
        base, off = self.split_addr(e.a)
        tn = TNAME[t]
        if off is None:
            return f"*({tn} *){base}"
        if off.startswith("-"):
            return f"*({tn} *)({base} - {off[1:]})"
        return f"*({tn} *)({base} + {off})"

    def bitfield(self, loc, o, w, unit, signed):
        name = f"BF{unit}{'s' if signed else ''}_{o}_{w}"
        base = {1: "signed char" if signed else "unsigned char", 4: "int" if signed else "unsigned"}[unit]
        pad = f"{base} :{o}; " if o else ""
        self.bitfields[name] = f"typedef struct {{ {pad}{base} f:{w}; }} {name};"
        if loc.op == "m":
            return f"(({name} *){self.ptr_text(loc.a)})->f"
        return f"(({name} *)&{self.vname(loc.var)})->f"

    def ptr_text(self, a):
        base, off = self.split_addr(a)
        return base if off is None else f"({base} + {off})"

    def split_addr(self, a):
        """Render an address expression as (byte-pointer base, integer offset or None)."""
        terms = []

        def flat(x):
            if x.op == "+":
                flat(x.a)
                flat(x.b)
            else:
                terms.append(x)
        flat(a)
        base = next((t for t in terms if t.op == "addr"), None)
        if base is None:
            base = next((t for t in terms if self.ty(t) == "p"), None)
        if base is None:
            base = next((t for t in terms if t.ptr), None)
        rest = [t for t in terms if t is not base]
        if base is None:
            # no pointer-typed term: treat the whole sum as an integer address
            return f"(unsigned char *)({self.expr(a)})", None
        if base.op == "addr":
            v = base.var
            agg = getattr(v, "agg", None)
            if agg and agg[0] is not v:
                bs = f"({agg[0].name} + {agg[1]})"
            elif v.type and v.type.startswith("arr"):
                bs = self.vname(v)
            else:
                bs = f"(unsigned char *)&{self.vname(v)}"
        else:
            bs = self.expr(base)
            if self.ty(base) != "p":
                bs = f"(unsigned char *){self.paren(base, bs)}"
        if not rest:
            return bs, None
        s = self.sum_text(rest)
        if len(rest) > 1 and base.op != "addr" and any(t.op != "k" and t.seq < base.seq for t in rest):
            s = f"({s})"        # the index was evaluated before the pointer: p[i + c]
        return (f"({bs})" if " " in bs and not bs.startswith("(") else bs), s

    def sum_text(self, terms):
        out = ""
        for n, t in enumerate(terms):
            if t.op == "k" and t.v < 0 and n:
                out += f" - {-t.v:#x}" if -t.v > 9 else f" - {-t.v}"
                continue
            s = self.expr(t)
            if self.ty(t) == "p":
                s = f"(int){self.paren(t, s)}"
            out += (" + " if n else "") + self.paren(t, s)
        return out

    def paren(self, e, s):
        if e.op in BINOPS or e.op == "cmp":
            return f"({s})"
        return s

    def const(self, v, t="i"):
        if t in ("c", "us") or t == "u":
            v &= {"c": 0xFF, "us": 0xFFFF, "u": 0xFFFFFFFF}[t]
        elif t in ("sc", "s"):
            w = TW[t] * 8
            v &= (1 << w) - 1
            if v >> (w - 1):
                v -= 1 << w
        if -10 < v < 10:
            return str(v)
        return f"-{-v:#x}" if v < 0 else f"{v:#x}"

    def expr(self, e, ctx=None):
        op = e.op
        if op == "k":
            return self.const(e.v, ctx or "i")
        if op == "v":
            t, cast = self.load_type(e)
            acc = self.var_access(e, t)
            if acc:
                return acc
            name = self.vname(e.var)
            if e.var.stride and e.var.type == "p":
                return f"((unsigned char *){name})"
            return f"({TNAME[t]}){name}" if cast else name
        if op == "m":
            return self.deref(e, self.mem_type(e))
        if op == "addr":
            v = e.var
            agg = getattr(v, "agg", None)
            if agg and agg[0] is not v:
                return f"({agg[0].name} + {agg[1]})"
            if v.type and v.type.startswith("arr"):
                return self.vname(v)
            return f"&{self.vname(v)}"
        if op == "fn":
            self.use_func(e.name)
            if e.x == "far":
                self.far.add(e.name)
                return f"(int){e.name}"
            return e.name
        if op == "call":
            return self.call(e)
        if op == "trunc":
            return f"({TNAME[tcode(e.w, False)]}){self.paren(e.a, self.expr(e.a))}"
        if op == "ext":
            t = tcode(2 if e.w == 2 or e.b == "16" else e.typ, e.x == "s")
            inner = e.a.a if e.a.op == "trunc" and e.a.w == TW[t] else e.a
            return f"({TNAME[t]}){self.paren(inner, self.expr(inner))}"
        if op == "neg":
            return f"-{self.paren(e.a, self.expr(e.a))}"
        if op == "~":
            return f"~{self.paren(e.a, self.expr(e.a))}"
        if op == "raw":
            return f"/*{e.name}*/0"
        if op == "cs":
            return "/*cs*/0"
        if op == "bf":
            return self.bitfield(e.a, e.v[0], e.v[1], e.a.w, not e.uns)
        if op == "pre":
            return f"{e.name}{self.lvalue(e.a)}"
        if op == "asg":
            return f"({self.lvalue(e.a)} = {self.expr(e.b)})"
        if op == "post":
            if e.a.op == "v" and e.a.var.stride and e.a.var.type == "p":
                self.structs.add(e.a.var.stride)
                return f"((unsigned char *){self.lvalue(e.a)}{e.name})"
            lv = self.lvalue(e.a)
            return f"({lv}){e.name}" if lv.startswith("*") else f"{lv}{e.name}"
        if op == "preop":
            return f"({self.lvalue(e.a)} {e.name}= {self.expr(e.b)})"
        if op == "cmp":
            return self.cmp(e)
        return self.binary(e)

    def binary(self, e):
        op = e.op
        ea, eb = (e.b, e.a) if e.flip else (e.a, e.b)     # --refine may swap commutative operands
        a = self.expr(ea)
        ta = self.ty(ea)
        b = self.expr(eb, promote(ta) if eb.op == "k" else None)
        tb = self.ty(eb)
        a, b = self.paren(ea, a), self.paren(eb, b)
        ca, cb = promote(ta), promote(tb)
        if op in (">>", "/", "%"):
            if e.uns and ca == "i" and (op == ">>" or cb == "i") and not (op == ">>" and self.nuns(e.a)):
                a, ca = f"(unsigned){a}", "u"
            elif not e.uns:
                if ca in ("u", "p"):
                    a, ca = f"(int){a}", "i"
                if op != ">>" and cb in ("u", "p"):
                    b, cb = f"(int){b}", "i"
        if op in ("*", "&", "|", "^", "<<", ">>", "/", "%") and ca == "p":
            a, ca = f"(int){a}", "i"
        if op in ("*", "&", "|", "^", "/", "%") and cb == "p":
            b, cb = f"(int){b}", "i"
        if op in ("+", "-") and ca == "p" and cb == "p":
            b, cb = f"(int){b}", "i"
        if op == "-" and cb == "p" and ca != "p":
            b, cb = f"(int){b}", "i"
        if op in ("+", "-") and "p" in (ca, cb):
            rt = "p"
        elif op in ("<<", ">>"):
            rt = ca
        else:
            rt = "u" if "u" in (ca, cb) else "i"
        self.tcache[id(e)] = rt
        return f"{a} {op} {b}"

    def nuns(self, e):
        """Expression Watcom types as a narrow unsigned value (compared at byte/word width)."""
        if e.op == "k":
            return 0 <= e.v <= 0xFFFF
        if e.op in ("v", "m", "pre", "post"):
            return self.ty(e) in ("c", "us")
        if e.op in ("&", "|", "^"):
            return self.nuns(e.a) and self.nuns(e.b)
        return e.op == "ext" and e.x == "z"

    def cmp(self, e):
        a, b, rel = e.a, e.b, e.rel
        if a.op == "k" and b.op != "k":
            a, b, rel = b, a, SWAP[rel]
        sa = self.paren(a, self.expr(a))
        ta = self.ty(a)
        sb = self.paren(b, self.expr(b, ta if b.op == "k" else None))
        tb = self.ty(b)
        if (ta == "p") != (tb == "p") and not (b.op == "k" and b.v == 0):
            if ta == "p":
                sa, ta = f"({'unsigned' if e.uns else 'int'}){sa}", "u" if e.uns else "i"
            else:
                sb, tb = f"({'unsigned' if e.uns else 'int'}){sb}", "u" if e.uns else "i"
        if rel in ("<", "<=", ">", ">="):
            pa, pb = promote(ta), promote(tb) if b.op != "k" else promote(ta)
            c_uns = "u" in (pa, pb) or "p" in (pa, pb) or (self.nuns(a) and (self.nuns(b) or b.op == "k"))
            if e.uns and not c_uns:
                narrow = ("s", "sc", "c", "us")
                if ta in narrow and (b.op == "k" or tb in narrow):
                    # a narrow unsigned compare: make the signed narrow operands unsigned
                    if ta in ("s", "sc"):
                        sa = f"({TNAME[tcode(TW[ta], False)]}){sa}"
                    if b.op != "k" and tb in ("s", "sc"):
                        sb = f"({TNAME[tcode(TW[tb], False)]}){sb}"
                else:
                    sa = f"(unsigned){sa}"
            elif not e.uns and c_uns:
                if ta in ("c", "us") and b.op == "k":
                    sa = f"({TNAME[tcode(TW[ta], True)]}){sa}"
                else:
                    sa = f"(int){sa}"
                    if b.op != "k":
                        sb = f"(int){sb}"
        return f"{sa} {rel} {sb}"

    def use_func(self, name):
        self.used_funcs[name] = True
        if name in LIB_HEADERS:
            self.includes.add(LIB_HEADERS[name])

    def call(self, e):
        if e.a.op == "fn":
            name = e.a.name
            self.use_func(name)
            sig = self.protos.get(name) or self.prog.sigs.get(name) or {}
            params = sig.get("params") or []
        else:
            sig = {}
            if e.a.op == "v":
                name = self.fnptr(e.a)
            elif e.a.op == "m":
                base, off = self.split_addr(e.a.a)
                name = f"(*(int (**)()){base if off is None else f'({base} + {off})'})"
            else:
                name = f"((int (*)()){self.paren(e.a, self.expr(e.a))})"
            params = []
        args = []
        for k, x in enumerate(e.args):
            s = self.expr(x)
            pt = params[k] if k < len(params) else None
            tx = self.ty(x)
            if pt == "cp":
                s = f"(char *){self.paren(x, s)}"
            elif pt == "p" and sig.get("lib") and not (x.op == "k" and x.v == 0):
                s = f"(void *){self.paren(x, s)}"
            elif pt == "p" and tx != "p" and not (x.op == "k" and x.v == 0):
                s = f"(void *){self.paren(x, s)}"
            elif pt and pt != "p" and tx == "p":
                s = f"(int){self.paren(x, s)}"
            args.append(s)
        return f"{name}({', '.join(args)})"

    def fnptr(self, e):
        v = e.var
        if v.kind == "g":
            self.gname(v)
            v.fnptr = True
            return f"(*{v.name})"
        return f"((int (*)()){v.name})"

    # statements -------------------------------------------------------------------------------
    def stmt(self, st):
        k = st[0]
        if k == "set" and st[1].op == "v" and st[1].var.stride and st[1].var.type == "p":
            n = st[1].var.stride
            self.structs.add(n)
            return f"{self.lvalue(st[1])} = (S{n} *){self.paren(st[2], self.expr(st[2]))};"
        if k == "opset" and st[1].op == "v" and st[1].var.stride and st[1].var.type == "p" and st[2] in ("+", "-"):
            n, v, name = st[1].var.stride, st[3], self.lvalue(st[1])
            self.structs.add(n)
            if v.op == "k" and v.v % n == 0:
                q = v.v // n
                return f"{name} {st[2]}= {q};" if q != 1 else f"{name}{st[2]}{st[2]};"
            if v.op == "*" and v.b.op == "k" and v.b.v == n:
                return f"{name} {st[2]}= {self.paren(v.a, self.expr(v.a))};"
            if v.op == "<<" and v.b.op == "k" and (1 << v.b.v) == n:
                return f"{name} {st[2]}= {self.paren(v.a, self.expr(v.a))};"
            return f"{name} = (S{n} *)((unsigned char *){name} {st[2]} {self.paren(v, self.expr(v))});"
        if k == "set":
            loc, val = st[1], st[2]
            lt = self.lv_type(loc)
            s = self.expr(val, lt if val.op == "k" else None)
            tv = self.ty(val)
            if lt == "p" and tv != "p" and not (val.op == "k" and val.v == 0):
                s = f"(unsigned char *){self.paren(val, s)}"
            elif lt != "p" and tv == "p":
                s = f"(int){self.paren(val, s)}"
            return f"{self.lvalue(loc)} = {s};"
        if k == "opset":
            loc, op, val, uns = st[1:]
            lt = self.lv_type(loc)
            s = self.expr(val, lt if val.op == "k" else None)
            if self.ty(val) == "p" and lt != "p":
                s = f"(int){self.paren(val, s)}"
            if lt == "p" and op not in ("+", "-"):
                return f"{self.lvalue(loc)} = (unsigned char *)((int){self.lvalue(loc)} {op} {s});"
            return f"{self.lvalue(loc)} {op}= {s};"
        if k == "incdec":
            return f"{st[2]}{self.lvalue(st[1])};"
        if k == "bfset":
            _, loc, o, w, v = st
            f = self.bitfield(loc, o, w, loc.w, False)
            if v.op in ("+", "-") and v.a.op == "bf" and v.a.v == (o, w) and v.b.op == "k" and                     (v.a.a.key() == loc.key() or (loc.op == "m" and same_place(v.a.a, loc))):
                if v.b.v == 1:
                    return f"{f}{v.op}{v.op};"
                return f"{f} {v.op}= {self.expr(v.b)};"
            return f"{f} = {self.expr(v)};"
        if k == "copy":
            n = st[3]
            self.structs.add(n)
            return f"*(S{n} *){self.ptr_text(st[1])} = *(S{n} *){self.ptr_text(st[2])};"
        if k == "expr":
            return f"{self.expr(st[1])};"
        if k == "ret":
            if st[1] is None:
                return "return;"
            rt = self.L.ret_type
            s = self.expr(st[1], rt if st[1].op == "k" else None)
            if rt == "p" and self.ty(st[1]) != "p":
                s = f"(unsigned char *){self.paren(st[1], s)}"
            elif rt != "p" and self.ty(st[1]) == "p":
                s = f"(int){self.paren(st[1], s)}"
            return f"return {s};"
        if k == "if":
            if st[2] is None:
                return f"if ({self.expr(st[1])}) {{}}"
            return f"if ({self.expr(st[1])}) goto L_{st[2]:x};"
        if k == "goto":
            return f"goto L_{st[1]:x};"
        if k in ("break", "continue"):
            return f"{k};"
        if k in ("ifbreak", "ifcontinue"):
            return f"if ({self.expr(st[1])}) {k[2:]};"
        if k == "ifthen":
            return "\n".join([f"if ({self.expr(st[1])}) {{"] + self.block(st[2]) + ["}"])
        if k == "ifelse":
            return "\n".join([f"if ({self.expr(st[1])}) {{"] + self.block(st[2]) + ["} else {"] +
                             self.block(st[3]) + ["}"])
        if k == "while":
            return "\n".join([f"while ({self.expr(st[1])}) {{"] + self.block(st[2]) + ["}"])
        if k == "for":
            init = self.stmt(st[4]).rstrip(";") if len(st) > 4 else ""
            step = ", ".join(self.stmt(x).rstrip(";") for x in st[2])
            return "\n".join([f"for ({init}; {self.expr(st[1])}; {step}) {{"] + self.block(st[3]) + ["}"])
        if k == "dowhile":
            return "\n".join(["do {"] + self.block(st[1]) + [f"}} while ({self.expr(st[2])});"])
        if k == "label":
            lines = []
            for sw in list(self.switches):
                if st[1] == sw["end"]:
                    lines.append("}")
                    self.switches.remove(sw)
                    continue
                for v in sw["at"].get(st[1], []):
                    lines.append(f"case {v}:")
                if st[1] == sw["default"]:
                    lines.append("default:")
            return "\n".join(lines + [f"L_{st[1]:x}:;"])
        if k == "asm":
            return f"/* unsupported: {st[1]} */"
        if k == "switch":
            # case labels are placed at their code (a table entry must point at the case body)
            _, val, at, dflt = st
            end = dflt if dflt is not None and dflt >= max(at) else None
            at = {t: v for t, v in at.items() if t != end}
            self.switches.append({"at": at, "end": end, "default": None if end else dflt})
            return f"switch ({self.expr(val)}) {{"
        return f"/* ? {k} */"

    def block(self, stmts):
        out = []
        for x in stmts:
            out.extend(self.stmt(x).split("\n"))
        return out

    # whole function ----------------------------------------------------------------------------
    def source(self):
        L = self.L
        saved = {v: (v.type, v.volatile) for v in L.local_ptr | L.local_int}
        for v in L.local_ptr:       # pointer arithmetic seen in this function: a pointer here
            if v.type in ("i", "u"):
                v.type, v.volatile = "p", False
        for v in L.local_int - L.local_ptr:     # plain int arithmetic here
            if v.type == "p" and ((v.decl and v.decl["type"] in ("i", "u")) or not v.ptr):
                v.type = v.decl["type"] if v.decl and v.decl["type"] in ("i", "u") else "i"
        try:
            return self._source()
        finally:
            for v, (t, vol) in saved.items():
                v.type, v.volatile = t, vol

    def _source(self):
        L = self.L
        self.prepare()
        body = []
        stmts = L.stmts
        used_labels = set()
        for st in stmts:
            if st[0] == "if" and st[2] is not None:
                used_labels.add(st[2])
            elif st[0] == "goto":
                used_labels.add(st[1])
            elif st[0] == "switch":
                used_labels.update(st[2])
                if st[3] is not None:
                    used_labels.add(st[3])
                    used_labels.add(st[3])
        flat = [st for st in stmts if not (st[0] == "label" and st[1] not in used_labels)]
        if self.structured:
            flat = Structurer(flat).run()
        for st in flat:
            body.extend(self.stmt(st).split("\n"))
        body += ["}"] * len(self.switches)     # switches whose default case lies inside them
        # declarations
        out = ["/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */"]
        for inc in sorted(self.includes):
            out.append(f"#include <{inc}>")
        for v in list(L.decl_locals) + list(self.used_globals.values()) + list(L.args.values()):
            if v.stride and v.type == "p":
                self.structs.add(v.stride)
        for n in sorted(self.bitfields):
            out.append(self.bitfields[n])
        for n in sorted(self.structs):
            out.append(f"typedef struct {{ unsigned char b[{n}]; }} S{n};")
        for name in sorted(self.used_globals):
            v = self.used_globals[name]
            out.append(global_decl(v))
        for name in sorted(self.used_funcs):
            if name == L.name or name in LIB_HEADERS:
                continue
            if name in self.far:
                out.append(f"extern void __far {name}(void);")
                continue
            out.append(func_decl(self.protos.get(name) or self.prog.sigs.get(name), name))
        ret = TNAME[L.ret_type] if L.ret_type else "void"
        params = ", ".join(f"{TNAME[L.args[k].type] if k in L.args else 'int'} a{k}" for k in range(L.nparams)) or "void"
        out.append(f"{ret} {L.name}({params})")
        out.append("{")
        for v in L.decl_locals:
            out.append(f"    {decl_text(v)};")
        inner = getattr(L, "inner_locals", [])
        if inner:           # autos of a nested block (allocated after the return temp)
            out.append("    {")
            for v in inner:
                out.append(f"    {decl_text(v)};")
        ind = 1
        for line in body:
            if line.startswith("}"):
                ind -= 1
            if line.startswith("L_"):
                out.append(line)
            else:
                out.append("    " * ind + line)
            if line.endswith("{"):
                ind += 1
        if inner:
            out.append("    }")
        out.append("}")
        return "\n".join(out) + "\n"


def decl_text(v):
    t = v.type
    vol = ""        # under -d2 (the game's flags) no `volatile` is needed; see compiler-notes
    if t == "p" and v.stride:
        return f"S{v.stride} * {vol}{v.name}"
    if t == "p":
        return f"unsigned char * {vol}{v.name}"
    if t.startswith("arr"):
        return f"{vol}unsigned char {v.name}[{t[3:]}]"
    return f"{vol}{TNAME[t]} {v.name}"


def global_decl(v):
    vol = ""
    if v.type == "p" and v.stride:
        return f"extern S{v.stride} * {vol}{v.name};"
    if getattr(v, "fnptr", False) and v.type not in ("arr",):
        return f"extern int (* {vol}{v.name})();"
    if v.type == "arr":
        return f"extern {vol}unsigned char {v.name}[];"
    if v.type == "p":
        return f"extern unsigned char * {vol}{v.name};" if vol else f"extern unsigned char *{v.name};"
    return f"extern {vol}{TNAME[v.type]} {v.name};"


def func_decl(sig, name):
    sig = sig or {"params": [], "ret": "i"}
    ret = "void" if sig.get("void") else TNAME[sig.get("ret") or "i"]
    if sig.get("varargs"):
        return f"extern {ret} {name}();"
    ps = ", ".join(("void *" if p == "p" else TNAME[p or "i"]) for p in sig["params"]) or "void"
    return f"extern {ret} {name}({ps});"


# ---------------------------------------------------------------------------------------------
def param_sizes(L):
    return [TW.get(L.args[k].type, 4) if k in L.args and L.args[k].type else 4 for k in range(L.nparams)]


def layout_locals(L):
    """Choose local types/declaration order so that Watcom's stable size sort reproduces the
    original slot offsets.  Slots are listed shallow ([ebp-4]) to deep."""
    offs = sorted(L.locals)
    spill_off = L.spill[0] if L.spill else None
    if spill_off is not None and spill_off not in L.locals:
        offs = sorted(set(offs) | {spill_off})
    deepest = max([L.frame] + offs) if offs or L.frame else 0
    # aggregates (structs/arrays): address-taken locals, word stores (scalar shorts are stored
    # as dwords) and odd offsets.  Scalars sort before them, so an aggregate extends up to the
    # contiguous run of accessed scalar slots at the top of the frame.
    agg_off = {o for o, v in L.locals.items() if v.addr or o % 4 or v.widths("const", "store") & {2}}
    aggs = {}
    if agg_off:
        top = 0
        while top + 4 in L.locals and top + 4 not in agg_off and top + 4 < min(agg_off) or top + 4 == spill_off:
            top += 4
        if spill_off and L.spill[1] == 4 and L.ret_type == "i":
            top = spill_off     # an int return temp sorts after every scalar auto
        deep = max([o for o in offs if o > top] + [L.frame])
        deep = (deep + 3) // 4 * 4
        # aggregates start at address-taken bases; the deepest one ends at the frame bottom
        starts = sorted({o for o in agg_off if o > top and L.locals[o].addr and o % 4 == 0} | {deep})
        prev = top
        for b in starts:
            if b - prev == 4 and not L.locals.get(b, Var("l", "", 0)).widths("const", "store") & {2} and                     not any(o % 4 for o in offs if prev < o <= b) and b != deep:
                prev = b
                continue       # a plain address-taken scalar
            if b not in L.locals:
                L.local(b)
            aggs[b] = (prev, b)
            prev = b
    agg_of = {}
    for b, (lo, hi) in aggs.items():
        for o in list(L.locals):
            if lo < o <= hi:
                agg_of[o] = b
    info = []
    u = 4
    while u <= max(deepest, 4) and (offs or L.frame):
        if u in aggs:
            lo, hi = aggs[u]
            base = L.locals.get(u) or L.local(u)
            base.type = f"arr{hi - lo}"
            base.fields = True
            for o, b in agg_of.items():
                if b == u:
                    L.locals[o].agg = (base, u - o)
            info.append({"off": u, "var": base, "temp": False, "range": (hi - lo, hi - lo), "agg": True})
            u += 4
            continue
        if any(b >= u > aggs[b][0] for b in aggs):
            u += 4
            continue
        v = L.locals.get(u)
        if u in L.switch_tmps:
            info.append({"off": u, "var": None, "temp": True, "range": (4, 4), "switch": True})
            u += 4
            continue
        if u == spill_off:
            w = L.spill[1]
            rng = (1, 1) if w == 1 or L.ret_type == "c" else (2, 2) if L.ret_type == "s" or w == 2 else (2, 4)
            info.append({"off": u, "var": None, "temp": True, "range": rng, "pref": rng[1]})
            u += 4
            continue
        if v is None:
            info.append({"off": u, "var": None, "unused": True, "range": (4, 4)})
            u += 4
            continue
        ws = v.widths()
        wide = v.widths("const", "store", "cmp", "rmw", "arith", "wval") & {4}
        if 1 in ws and not wide:
            rng = (1, 1)
        elif 1 in ws or v.widths("wval"):
            rng = (4, 4)
        elif 2 in ws:
            rng = (2, 4)
        else:
            rng = (2, 4) if not (v.widths("arith", "rmw", "cmp", "sx", "zx") or v.ptr) else (4, 4)
        info.append({"off": u, "var": v, "temp": False, "range": rng, "pref": 2 if 2 in ws else rng[1]})
        u += 4
    if L.frame and L.frame > deepest and not info:
        pass
    # Watcom orders the frame with a selection sort by size (swap-based, so not stable) over
    # the autos in declaration order followed by the return temp; slots are then assigned from
    # [ebp-4] down (probes: build/workers/lift/p/ao.c, c.c, d.c).  Pick sizes (short vs int,
    # narrow return) and a declaration order whose sort reproduces the original slot order.
    slots = [s for s in info if not s.get("switch")]
    options = []
    for s_ in slots:
        lo, hi = s_["range"]
        pref = s_.get("pref", hi)
        opts = sorted({z for z in (1, 2, 4) if lo <= z <= hi} | ({lo} if s_.get("agg") else set()),
                      key=lambda z: (z != pref, -z)) or [pref]
        options.append(opts)
    chosen, order, inner = None, None, []
    for nested in (False, True):
        for combo in itertools.islice(itertools.product(*options), 128):
            if any(x > y for x, y in zip(combo, combo[1:])):
                continue
            P = unsort([(k, z, s_.get("temp", False)) for k, (s_, z) in enumerate(zip(slots, combo))], nested,
                       param_sizes(L))
            if P is not None:
                chosen, (order, inner) = combo, P
                break
        if chosen is not None:
            break
    if chosen is None:
        chosen = [o[0] for o in options]
        order = [k for k, s_ in enumerate(slots) if not s_.get("temp")]
    for s_, z in zip(slots, chosen):
        s_["size"] = z
    decl = []
    for k in order:
        s_ = slots[k]
        v = s_["var"]
        if s_.get("temp"):
            continue
        if v is None:
            v = Var("l", f"unused_{s_['off']:x}", s_["off"])
            v.type = "i"
            decl.append(v)
            continue
        if not s_.get("agg"):
            resolve_local(v, s_["size"])
        decl.append(v)
    L.inner_locals = []
    for k in inner:       # autos of a nested block are sorted after the return temp
        s_ = slots[k]
        v = s_["var"] or Var("l", f"unused_{s_['off']:x}", s_["off"])
        if s_["var"] is None:
            v.type = "i"
        elif not s_.get("agg"):
            resolve_local(v, s_["size"])
        L.inner_locals.append(v)
    for s_ in slots:
        if s_.get("temp"):
            w = L.spill[1]
            if not getattr(L, "ret_narrow", False):
                L.ret_type = "c" if w == 1 else ("s" if s_["size"] == 2 else "i")
    L.decl_locals = decl


def multiset_permutations(items):
    """Distinct permutations of a sorted list, in lexicographic order."""
    a = list(items)
    while True:
        yield tuple(a)
        i = len(a) - 2
        while i >= 0 and a[i] >= a[i + 1]:
            i -= 1
        if i < 0:
            return
        j = len(a) - 1
        while a[j] <= a[i]:
            j -= 1
        a[i], a[j] = a[j], a[i]
        a[i + 1:] = reversed(a[i + 1:])


def shell_order(sizes):
    """Watcom's frame sort: a shell sort by size (gaps n//2, then (g+1)//2 down to 1) over
    [parameters, autos in declaration order, temps]; returns original indices in sorted order.
    (Fitted to 16 controlled probes, build/workers/lift/sorts.py.)"""
    A = list(range(len(sizes)))
    n, gap = len(A), len(A) // 2
    while gap > 0:
        for i in range(gap, n):
            t, j = A[i], i
            while j >= gap and sizes[A[j - gap]] > sizes[t]:
                A[j] = A[j - gap]
                j -= gap
            A[j] = t
        if gap == 1:
            break
        gap = (gap + 1) // 2
    return A


def unsort(final, nested=False, params=(), budget=4000):
    """Invert the frame sort.  `final` lists (id, size, is_temp) by slot (shallow first); `params`
    are the parameter sizes (they take part in the sort but get no slot).  Returns (outer autos in
    declaration order, inner-block autos) or None."""
    temps = [x for x in final if x[2]]
    autos = [x for x in final if not x[2]]
    np_, nt = len(params), len(temps)
    count = 0
    for pat in multiset_permutations(sorted(x[1] for x in autos)):
        count += 1
        if count > budget:
            return None
        for cut in (range(len(pat) + 1) if nested else [len(pat)]):
            sizes = list(params) + list(pat[:cut]) + [x[1] for x in temps] + list(pat[cut:])
            order = [k for k in shell_order(sizes) if k >= np_]
            if len(order) != len(final):
                continue
            P = [None] * len(sizes)
            ok = True
            for pos, item in zip(order, final):
                P[pos] = item
                if sizes[pos] != item[1]:
                    ok = False
            tpos = list(range(np_ + cut, np_ + cut + nt))
            if not ok or [P[k] for k in tpos] != temps:
                continue
            outer = [P[k][0] for k in range(np_, np_ + cut)]
            inner = [P[k][0] for k in range(np_ + cut + nt, len(sizes))]
            if temps or not inner:
                return outer, inner
    return None


def resolve_function(L):
    calls = L.prog.sigs[L.name]["calls"] if L.name in L.prog.sigs else []
    if calls and len({len(c) for c in calls}) == 1:
        L.nparams = max(L.nparams, len(calls[0]))    # unused trailing parameters still sort
    if L.spill:
        ws = {st[1].w for st in L.stmts if st[0] == "ret" and st[1] is not None and st[1].op != "k"}
        L.ret_type = "c" if L.spill[1] == 1 or ws == {1} else "s" if ws and max(ws) == 2 else "i"
        L.ret_narrow = bool(ws) and max(ws) < 4
    forms = defaultdict(Counter)
    for c in L.prog.sigs[L.name]["calls"] if L.name in L.prog.sigs else []:
        for k, f in enumerate(c):
            forms[k][f] += 1
    own = L.prog.own_defs.get(L.name)
    own = own["params"] if own and own.get("kind") == "func" else None
    for k, v in L.args.items():
        ws = v.widths()
        if own and k < len(own) and own[k] and own[k] != "p":
            v.type = own[k]         # the function's own proven definition
            continue
        if forms[k]["narrow16"] and not forms[k]["wide"]:
            size = 2
        elif forms[k]["narrow"] and not forms[k]["wide"]:
            size = 1 if 1 in ws else 2
        elif forms[k]["wide"] and not (ws - {4}) and not forms[k]["narrow"]:
            size = 4
        elif 1 in ws and 4 not in ws:
            size = 1
        elif 2 in ws and 1 not in ws and not (v.widths("arith", "rmw", "push", "cmp") & {4}) and not v.ptr:
            size = 2
        else:
            size = 4
        resolve_local(v, size)


def lift_program(prog, names, structured=True):
    for f in prog.man["functions"]:
        if f.get("kind", "c") != "c":
            continue
        L = Lift(prog, f)
        try:
            L.run()
        except Exception as ex:  # keep going: a draft is still useful
            L.unknown.append(f"lifter error: {ex!r}")
        prog.lifts[f["name"]] = L
    for L in prog.lifts.values():
        resolve_function(L)
    for L in prog.lifts.values():
        layout_locals(L)
    prog.resolve()
    out = {}
    for n in names:
        L = prog.lifts.get(n)
        if L is None:
            L = Lift(prog, prog.funcs[n])
            L.run()
            resolve_function(L)
            layout_locals(L)
        out[n] = Render(L, structured).source(), L
    return out


# ---------------------------------------------------------------------------------------------
# Structuring: fold the fixed -od layouts of goto code into structured statements.  Each rule
# rewrites text lines of the flat program; the rewritten form compiles to the same code.
def negate(c):
    """The condition that jumps when `c` does not: `if (c) goto L; S; L:` == `if (!c) { S }`."""
    if c.op == "cmp" and c.rel in NEG:
        n = E("cmp", c.a, c.b, rel=NEG[c.rel], uns=c.uns)
        return n
    return None


class Structurer:
    """Fold the fixed -od layouts of the flat goto program into if/else, while, for and do-while
    (probes b.c/e.c: each structured form compiles to exactly the same jumps).  Anything that does
    not fit a layout stays a goto; --refine keeps the goto form if structuring ever changes code."""
    SIMPLE = ("set", "opset", "incdec", "expr", "copy", "bfset")

    def __init__(self, stmts):
        self.stmts = stmts
        self.end = None
        self.refs = Counter()
        for st in stmts:
            if st[0] == "if" and st[2] is not None:
                self.refs[st[2]] += 1
            elif st[0] == "goto":
                self.refs[st[1]] += 1
            elif st[0] == "switch":
                for t in st[2]:
                    self.refs[t] += 1
                if st[3] is not None:
                    self.refs[st[3]] += 1

    def run(self):
        if any(st[0] == "switch" for st in self.stmts):
            return self.stmts        # case labels must stay at their statements
        return self.seq(list(self.stmts))

    @staticmethod
    def find(seq, kind, target, start):
        for j in range(start, len(seq)):
            if seq[j][0] == kind and seq[j][1] == target:
                return j
        return None

    def jumps(self, block, lab):
        """Top-level (not inside nested loops) references to `lab` in a structured block."""
        n = 0
        for st in block:
            if st[0] in ("goto",) and st[1] == lab or st[0] == "if" and st[2] == lab:
                n += 1
            elif st[0] == "ifthen":
                n += self.jumps(st[2], lab)
            elif st[0] == "ifelse":
                n += self.jumps(st[2], lab) + self.jumps(st[3], lab)
        return n

    def nested_refs(self, block, lab):
        n = 0
        for st in block:
            if st[0] in ("goto",) and st[1] == lab or st[0] == "if" and st[2] == lab:
                n += 1
            for x in st[1:]:
                if isinstance(x, list):
                    n += self.nested_refs(x, lab)
        return n

    def convert(self, block, lab, kind):
        out = []
        for st in block:
            if st[0] == "goto" and st[1] == lab:
                st = (kind,)
            elif st[0] == "if" and st[2] == lab:
                st = ("if" + kind, st[1])
            elif st[0] == "ifthen":
                st = ("ifthen", st[1], self.convert(st[2], lab, kind))
            elif st[0] == "ifelse":
                st = ("ifelse", st[1], self.convert(st[2], lab, kind), self.convert(st[3], lab, kind))
            out.append(st)
        return out

    def loop_body(self, body, lbreak, lcont):
        """Structure a loop body and turn its jumps to the loop exits into break/continue; None if
        a jump from a nested loop would need a goto into this loop's hidden positions."""
        body = self.seq(body)
        for lab, kind in ((lbreak, "break"), (lcont, "continue")):
            if lab is None:
                continue
            if self.nested_refs(body, lab) != self.jumps(body, lab):
                return None
            body = self.convert(body, lab, kind)
        return body

    def seq(self, seq):
        out = []
        i = 0
        while i < len(seq):
            r = self.at(seq, i)
            if r is None:
                out.append(seq[i])
                i += 1
            else:
                node, i = r
                if node[0] == "for" and out and out[-1][0] == "set" and out[-1][1].op == "v" and \
                        any(x.op == "v" and x.var is out[-1][1].var for x in node[1].walk()):
                    node = node + (out.pop(),)      # for (i = 0; ...)
                out.append(node)
        return out

    def at(self, seq, i):
        st = seq[i]
        n = len(seq)
        if st[0] == "label":
            top = st[1]
            # for: Ltop: if (c) goto Lbody; goto Lend; Lstep: step; goto Ltop; Lbody: body; goto Lstep; Lend:
            if i + 3 < n and seq[i + 1][0] == "if" and seq[i + 2] == ("ret", None) and seq[i + 3][0] == "label"                     and seq[-1] == ("goto", seq[i + 3][1]):
                # the same with the loop exit at the function end (`jmp epilogue` = return)
                seq = seq + [("label", "END")]
                n = len(seq)
                self.refs["END"] = 1
                seq[i + 2] = ("goto", "END")
            if i + 3 < n and seq[i + 1][0] == "if" and seq[i + 2][0] == "goto" and seq[i + 3][0] == "label":
                lbody, lend, lstep = seq[i + 1][2], seq[i + 2][1], seq[i + 3][1]
                j = self.find(seq, "goto", top, i + 4)
                if j is not None and all(x[0] in self.SIMPLE for x in seq[i + 4:j]) and j + 1 < n and \
                        seq[j + 1] == ("label", lbody) and self.refs[top] == 1 and self.refs[lbody] == 1:
                    k = self.find(seq, "label", lend, j + 2)
                    if k is not None and seq[k - 1] == ("goto", lstep):
                        body = seq[j + 2:k - 1]
                        c = seq[i + 1][1]
                        inner = [x for x in body]
                        nb = self.jumps_flat(inner, lend)
                        nc = self.jumps_flat(inner, lstep)
                        if self.refs[lend] == 1 + nb and self.refs[lstep] == 1 + nc and c.op == "cmp":
                            b = self.loop_body(inner, lend, lstep)
                            if b is not None:
                                return ("for", c, list(seq[i + 4:j]), b), k + 1
            # while: Ltop: if (c) goto Lend; body; [Lcont:] goto Ltop; Lend:
            if i + 1 < n and seq[i + 1][0] == "if" and seq[i + 1][1].op == "cmp":
                lend = seq[i + 1][2]
                j = self.find(seq, "goto", top, i + 2)
                if j is not None and j + 1 < n and seq[j + 1] == ("label", lend) and self.refs[top] == 1:
                    body = seq[i + 2:j]
                    lcont = None
                    if body and body[-1][0] == "label":
                        lcont = body[-1][1]
                        body = body[:-1]
                        if self.refs[lcont] != self.jumps_flat(body, lcont):
                            lcont = "bad"
                    c = negate(seq[i + 1][1])
                    if c is not None and lcont != "bad" and self.refs[lend] == 1 + self.jumps_flat(body, lend):
                        b = self.loop_body(body, lend, lcont)
                        if b is not None:
                            return ("while", c, b), j + 2
            # do-while: Ltop: body; [Lcont:] if (c) goto Ltop; [Lend:]
            j = next((k for k in range(i + 1, n) if seq[k][0] == "if" and seq[k][2] == top), None)
            if j is not None and self.refs[top] == 1 and seq[j][1].op == "cmp":
                body = seq[i + 1:j]
                lcont = None
                if body and body[-1][0] == "label":
                    lcont = body[-1][1]
                    body = body[:-1]
                lend = seq[j + 1][1] if j + 1 < n and seq[j + 1][0] == "label" else None
                okc = lcont is None or self.refs[lcont] == self.jumps_flat(body, lcont)
                okb = lend is None or self.refs[lend] == self.jumps_flat(body, lend)
                if okc and okb and not any(x[0] == "label" and self.refs[x[1]] and
                                           self.find(seq, "goto", x[1], j) is not None for x in body):
                    b = self.loop_body(body, lend, lcont)
                    if b is not None:
                        return ("dowhile", b, seq[j][1]), (j + 1 if lend is None else j + 2)
            return None
        if st[0] == "if" and st[2] is not None and st[1].op == "cmp" and st[2] == self.end:
            # `if (c) goto END` where END is the end of the enclosing if-block: a nested if
            c = negate(st[1])
            if c is not None and i + 1 < len(seq):    # `if (x) {}` would drop the jump
                return ("ifthen", c, self.seq(seq[i + 1:])), len(seq)
        if st[0] == "if" and st[2] is not None and st[1].op == "cmp":
            lab = st[2]
            j = self.find(seq, "label", lab, i + 1)
            if j is None or j == i + 1:
                return None
            c = negate(st[1])
            if c is None:
                return None
            if self.refs[lab] != 1:
                # several `if (x) goto L` inside the block: nested ifs (-od emits direct jumps for
                # `if (A) { if (B) {...} }`, a trampoline only for `&&`)
                if self.refs[lab] != self.nested_refs(seq[i:j], lab):
                    return None
                saved, self.end = self.end, lab
                inner = self.seq(seq[i + 1:j])
                self.end = saved
                if self.nested_refs(inner, lab):
                    return None
                return ("ifthen", c, inner), j + 1
            # if/else: if (c) goto Lelse; S1; goto Lend; Lelse: S2; Lend:
            if j - 1 > i and seq[j - 1][0] == "goto":
                lend = seq[j - 1][1]
                k = self.find(seq, "label", lend, j + 1)
                if k is not None and self.refs[lend] == 1:
                    return ("ifelse", c, self.seq(seq[i + 1:j - 1]), self.seq(seq[j + 1:k])), k + 1
            return ("ifthen", c, self.seq(seq[i + 1:j])), j + 1
        return None

    @staticmethod
    def jumps_flat(block, lab):
        return sum(1 for st in block if (st[0] == "goto" and st[1] == lab) or (st[0] == "if" and st[2] == lab))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("functions", nargs="*", help="manifest function names")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--all", action="store_true", help="all non-matching C functions")
    ap.add_argument("--controls", action="store_true", help="all matching functions")
    ap.add_argument("--range", nargs=2, metavar=("START", "END"), help="non-matching functions in [start,end)")
    ap.add_argument("--goto", action="store_true", help="no structuring (flat goto form)")
    ap.add_argument("--refine", action="store_true", help="check drafts and search commutative operand orders")
    ap.add_argument("-j", "--jobs", type=int, default=8)
    a = ap.parse_args()
    prog = Program()
    fns = [f for f in prog.man["functions"] if f.get("kind", "c") == "c"]
    selected = list(a.functions)
    if a.all:
        selected += [f["name"] for f in fns if f["status"] != "matching"]
    if a.controls:
        selected += [f["name"] for f in fns if f["status"] == "matching"]
    if a.range:
        lo, hi = (int(x, 0) for x in a.range)
        selected += [f["name"] for f in fns if f["status"] != "matching" and lo <= int(f["start"], 16) < hi]
    if not selected:
        ap.error("give functions, --all, --controls or --range")
    for n in selected:
        if n not in prog.funcs:
            ap.error(f"unknown function {n}")
    a.out.mkdir(parents=True, exist_ok=True)
    res = lift_program(prog, list(dict.fromkeys(selected)), not a.goto)
    for n, (text, L) in res.items():
        (a.out / f"{n}.c").write_text(text)
        if not a.refine:
            print(f"{n} {L.fn['start']}..{L.fn['end']} unsupported={len(L.unknown)}")
    if a.refine:
        from concurrent.futures import ThreadPoolExecutor
        with ThreadPoolExecutor(a.jobs) as ex:
            for line in ex.map(lambda item: refine(item[1][1], a.out, not a.goto), res.items()):
                print(line)


# ---------------------------------------------------------------------------------------------
# --refine: operand order of commutative operations is the one thing -od code does not reveal
# reliably (Watcom picks evaluation order and result register by its own cost rules).  For a
# draft that is not EXACT, try swapping the operands of commutative nodes emitted at or after the
# first differing instruction, one at a time, keeping swaps that improve the tools/check.py score.
COMMUTATIVE = {"+", "*", "&", "|", "^"}


def run_check(path, fn, tag):
    import subprocess
    js = ROOT / "build" / "lift-refine" / f"{fn['name']}.{tag}.json"
    js.parent.mkdir(parents=True, exist_ok=True)
    p = subprocess.run([sys.executable, str(ROOT / "tools" / "check.py"), str(path), fn["name"], "--at", fn["start"],
                        "--end", fn["end"], "--profile", fn.get("profile", "game-c"), "--json", str(js)],
                       capture_output=True, text=True)
    line = (p.stdout.strip().splitlines() or ["FAIL"])[0]
    if not js.exists() or not line.startswith(("EXACT", "DIFF")):
        return False, (-1, 0), None
    d = json.loads(js.read_text())
    diff = d.get("diff") or {}
    first = diff.get("first_diff_insn")
    score = (diff.get("equal_insns") or 0, -len(d.get("problems") or []))
    return line.startswith("EXACT"), score, int(first, 16) if first else None


def refine(L, outdir, structured, limit=40):
    path = outdir / f"{L.name}.c"
    exact, best, first = run_check(path, L.fn, "base")
    if exact:
        return f"{L.name} EXACT"
    tried = 0
    points = []
    for st, addr in zip(L.stmts, L.stmts.addr):
        if first is not None and addr < first - 0x40:
            continue
        for x in st[1:]:
            if isinstance(x, E):
                points += [n for n in x.walk() if n.op in COMMUTATIVE and n.a.op != "k" and n.b.op != "k"]
    seen = set()
    for n in points:
        if id(n) in seen or tried >= limit:
            continue
        seen.add(id(n))
        tried += 1
        n.flip = not n.flip
        path.write_text(Render(L, structured).source())
        exact, score, _ = run_check(path, L.fn, "try")
        if exact:
            return f"{L.name} EXACT after refine ({tried} tries)"
        if score > best:
            best = score
        else:
            n.flip = not n.flip
    path.write_text(Render(L, structured).source())
    return f"{L.name} DIFF (best {best[0]} equal insns, {tried} tries)"


if __name__ == "__main__":
    main()
