"""Synthesise, compile and verify whole translation units (TU = one .c file = one OMF object).

    python tools/tu.py build f_105c2 f_1065b f_107b6 [--literals] [--out X.c]
    python tools/tu.py build --range 0xe855 0xf6f8            # all manifest functions in [a, b)
    python tools/tu.py scan [--range A B] [--window N]         # merge probes over the matched runs
    python tools/tu.py evidence                                # data-layout anchors for TU boundaries
    python tools/tu.py verify build/workers/tu/proposal.json   # build every proposed TU -> build/tus.json

build: concatenates the functions' canonical sources (manifest `src`) in address order.  Top-level
declarations are merged and de-duplicated per symbol; declarations of one symbol with different
(normalised) types are CONFLICTS: every variant is tried and the one that keeps the whole TU EXACT is
chosen (report says which).  Tag/typedef conflicts that no single variant resolves are renamed per
function (reported as unresolved type questions).  The TU is compiled with the functions' common
profile and verified by `tools/check.py --all` over [first.start, last.end) -- code AND every data
segment the TU defines (strings in CONST must sit, contiguous, at the address its code references).
--literals turns `extern char g_XXXX[]` references to CONST (< 0x24C4) into string literals read from
KE.EXE, so the TU's own string pool is verified too (its layout is decisive TU evidence).

scan: over maximal runs of adjacent matching functions with one profile: greedily extends a TU while the
merged build stays EXACT, and probes each adjacent pair; a failing pair is a type conflict or a codegen
change (a boundary candidate).  Under -od, code of functions compiled together is identical whenever
declarations agree, so an EXACT merge alone does not prove grouping: the decisive evidence is data.

evidence: from KE.EXE alone -- CONST string pool split into blocks (strings are packed inside one object,
each object's CONST is dword aligned; data-initialiser literals precede code literals within an object),
shared/duplicated literals, _DATA pointers into CONST, and per-function _DATA/_BSS references.
Writes build/workers/tu/evidence.json.

Data-layout facts used (probes in build/workers/tu/probe, Watcom 10.0 GA -3s -od -s):
  * CONST: literals de-duplicated per object; literals of data initialisers first (definition order),
    then code literals in order of first use; no padding between literals; segment align dword.
  * _DATA: definition order, no padding between variables (even pointers); segment align dword.
  * _BSS: not in definition order (compiler symbol-table order); segment align dword.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import OrderedDict, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import le as lemod  # noqa: E402

WORK = ROOT / "build" / "workers" / "tu"
CONST_END = 0x24C4   # CONST/_DATA boundary in obj3 (build/tumap.json data_class_evidence, STRONG)
BSS_START = 0x886F   # end of initialised obj3 bytes (PROVEN, LE page map)

# ----------------------------------------------------------------------------------------- lexer
TOKEN_RX = re.compile(r"""
    (?P<ws>\s+)
  | (?P<com>/\*.*?\*/|//[^\n]*)
  | (?P<pp>(?<![^\n])[ \t]*\#[^\n]*)
  | (?P<str>"(?:\\.|[^"\\\n])*")
  | (?P<chr>'(?:\\.|[^'\\\n])*')
  | (?P<id>[A-Za-z_]\w*)
  | (?P<num>(?:0[xX][0-9a-fA-F]+|\d+\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?)[uUlLfF]*)
  | (?P<p>\.\.\.|->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|[{}()\[\];,.<>=!~?:+\-*/%&|^])
""", re.S | re.X)

TYPE_KW = {"void", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
           "const", "volatile", "struct", "union", "enum", "extern", "static", "register", "auto",
           "typedef", "_Packed", "__near", "__far", "near", "far", "__cdecl", "_cdecl", "cdecl",
           "__interrupt", "interrupt", "__loadds", "__saveregs", "_Cdecl"}
C_KW = TYPE_KW | {"if", "else", "while", "for", "do", "switch", "case", "default", "break",
                  "continue", "return", "goto", "sizeof"}


class Tok:
    __slots__ = ("kind", "text", "start", "end")

    def __init__(self, kind, text, start, end):
        self.kind, self.text, self.start, self.end = kind, text, start, end

    def __repr__(self):
        return self.text


def lex(src: str):
    out, pos = [], 0
    while pos < len(src):
        m = TOKEN_RX.match(src, pos)
        if not m:
            raise SyntaxError(f"cannot lex at {pos}: {src[pos:pos + 20]!r}")
        kind = m.lastgroup
        if kind not in ("ws", "com"):
            out.append(Tok(kind, m.group(), m.start(), m.end()))
        pos = m.end()
    return out


def match_close(toks, i):
    """Index of the bracket closing toks[i]."""
    pairs = {"(": ")", "[": "]", "{": "}"}
    stack = []
    for j in range(i, len(toks)):
        t = toks[j].text
        if t in pairs:
            stack.append(pairs[t])
        elif t in (")", "]", "}"):
            if not stack or stack.pop() != t:
                raise SyntaxError(f"unbalanced {t}")
            if not stack:
                return j
    raise SyntaxError("unclosed bracket")


def split_commas(toks):
    parts, cur, depth = [], [], 0
    for t in toks:
        if t.text in "([{" and t.kind == "p":
            depth += 1
        elif t.text in ")]}" and t.kind == "p":
            depth -= 1
        if t.text == "," and depth == 0:
            parts.append(cur)
            cur = []
        else:
            cur.append(t)
    if cur or parts:
        parts.append(cur)
    return parts


# ------------------------------------------------------------------------------ top-level items
class Item:
    """kind: pp | func | decl.  For decl: sub = typedef | tag | var."""

    def __init__(self, kind, toks, text, src_name):
        self.kind, self.toks, self.text, self.src = kind, toks, text, src_name
        self.name = None       # function name
        self.header = None     # function header tokens (before the body)


def items_of(src: str, src_name: str):
    toks = lex(src)
    items, i = [], 0
    while i < len(toks):
        t = toks[i]
        if t.kind == "pp":
            items.append(Item("pp", [t], t.text.strip(), src_name))
            i += 1
            continue
        j = i
        while True:
            if j >= len(toks):
                raise SyntaxError(f"{src_name}: unterminated top-level item")
            tj = toks[j]
            if tj.text in ("(", "["):
                j = match_close(toks, j) + 1
                continue
            if tj.text == "{":
                head = toks[i:j]
                is_func = (head and head[-1].text == ")" and head[0].text != "typedef"
                           and not any(x.text == "=" for x in head))
                k = match_close(toks, j)
                if is_func:
                    it = Item("func", toks[i:k + 1], src[t.start:toks[k].end], src_name)
                    it.header = head
                    it.name = func_name(head)
                    items.append(it)
                    i = k + 1
                    break
                j = k + 1
                continue
            if tj.text == ";":
                items.append(Item("decl", toks[i:j + 1], src[t.start:tj.end], src_name))
                i = j + 1
                break
            j += 1
    return items


def func_name(head):
    for k, t in enumerate(head):
        if t.text == "(" and k and head[k - 1].kind == "id" and head[k - 1].text not in C_KW:
            return head[k - 1].text
    raise SyntaxError(f"no function name in {' '.join(x.text for x in head)}")


# ------------------------------------------------------------------------- declaration analysis
def is_typeword(t, typedefs):
    return t.kind == "id" and (t.text in TYPE_KW or t.text in typedefs)


def split_specs(toks, typedefs):
    """(specifier tokens, declarator tokens) of a declaration/parameter without the ';'."""
    k = 0
    while k < len(toks):
        t = toks[k]
        if t.text in ("struct", "union", "enum"):
            k += 1
            if k < len(toks) and toks[k].kind == "id":
                k += 1
            if k < len(toks) and toks[k].text == "{":
                k = match_close(toks, k) + 1
            continue
        if is_typeword(t, typedefs):
            k += 1
            continue
        break
    if k + 1 < len(toks) and toks[k].kind == "id" and toks[k].text not in C_KW and \
            (toks[k + 1].kind == "id" or toks[k + 1].text == "*"):
        k += 1  # a type name declared in a header (e.g. FILE) followed by a declarator
    return toks[:k], toks[k:]


def canon_specs(specs):
    words = [s.text for s in specs if s.text not in ("extern", "register", "auto")]
    quals = sorted(w for w in words if w in ("const", "volatile", "static"))
    rest = [w for w in words if w not in ("const", "volatile", "static")]
    base = sorted(rest)
    table = {("unsigned",): "unsigned int", ("int", "unsigned"): "unsigned int",
             ("signed",): "int", ("int", "signed"): "int",
             ("int", "short"): "short", ("short", "signed"): "short", ("int", "short", "signed"): "short",
             ("int", "short", "unsigned"): "unsigned short", ("short", "unsigned"): "unsigned short",
             ("int", "long"): "long", ("long", "signed"): "long", ("int", "long", "unsigned"): "unsigned long",
             ("long", "unsigned"): "unsigned long", ("char", "signed"): "signed char"}
    if all(w in ("int", "unsigned", "signed", "short", "long", "char") for w in base) and base:
        b = table.get(tuple(base), " ".join(rest))
    else:
        b = " ".join(rest)
    return " ".join(quals + [b])


class _Norm:
    def __init__(self, typedefs, drop_name):
        self.typedefs, self.drop, self.name = typedefs, drop_name, None

    def decl(self, toks):
        out, prev, i = [], None, 0
        while i < len(toks):
            t = toks[i]
            if t.text == "(":
                j = match_close(toks, i)
                inner = toks[i + 1:j]
                if prev is not None and (prev == "$" or prev in (")", "]")):
                    params = split_commas(inner)
                    out.append("(" + ",".join(norm_param(p, self.typedefs) for p in params) + ")")
                    prev = ")"
                else:
                    out.append("(" + self.decl(inner) + ")")
                    prev = ")"
                i = j + 1
                continue
            if t.kind == "id" and self.name is None and not is_typeword(t, self.typedefs) and t.text not in C_KW:
                self.name = t.text
                out.append("$")
                prev = "$"
            elif t.text == "[":
                j = match_close(toks, i)
                out.append("[" + "".join(x.text for x in toks[i + 1:j]) + "]")
                prev = "]"
                i = j + 1
                continue
            else:
                out.append(t.text)
                prev = t.text
            i += 1
        return " ".join(out)


def norm_param(toks, typedefs):
    if len(toks) == 1 and toks[0].text in ("void", "..."):
        return toks[0].text
    specs, dcl = split_specs(toks, typedefs)
    n = _Norm(typedefs, True)
    d = n.decl(dcl).replace("$", "").strip()
    return (canon_specs(specs) + (" " + d if d else "")).strip()


def norm_decl(specs, dcl, typedefs):
    """(name, canonical type string) of one declarator."""
    n = _Norm(typedefs, False)
    d = n.decl(dcl)
    return n.name, (canon_specs(specs) + " " + d).strip()


def tok_text(toks):
    s = ""
    for t in toks:
        if s and (s[-1].isalnum() or s[-1] == "_") and (t.text[0].isalnum() or t.text[0] == "_"):
            s += " "
        elif s and t.text not in (",", ";", ")", "]", "[", "(") and s[-1] not in ("(", "[", "*"):
            s += " "
        elif s and s[-1] == "," :
            s += " "
        s += t.text
    return s


# --------------------------------------------------------------------------------- manifest etc.
def manifest():
    return json.loads((ROOT / "manifest.json").read_text())


def functions(man=None):
    man = man or manifest()
    fs = [f for f in man["functions"] if f.get("object", 1) == 1]
    return sorted(fs, key=lambda f: int(f["start"], 16))


_ORIG = {}


def obj3():
    if "d" not in _ORIG:
        L = lemod.LE(ROOT / "assets" / "KE.EXE")
        _ORIG["L"] = L
        _ORIG["d"] = L.object_bytes(L.objects[2])
    return _ORIG["d"]


def c_literal(addr):
    d = obj3()
    end = d.index(0, addr)
    s = '"'
    for b in d[addr:end]:
        c = chr(b)
        if c == '"' or c == "\\":
            s += "\\" + c
        elif c == "\n":
            s += "\\n"
        elif c == "\t":
            s += "\\t"
        elif c == "\r":
            s += "\\r"
        elif 0x20 <= b < 0x7F and not (c == "?" and s.endswith("?")):
            s += c
        else:
            s += f"\\{b:03o}"
    return s + '"'


# ------------------------------------------------------------------------------------ synthesis
class TU:
    def __init__(self, funcs, literals=False, choices=None, rename=None, defs=()):
        self.funcs = funcs                      # manifest entries, address order
        self.defs = list(defs)                  # the TU's own data definitions (C text, definition order)
        self.literals = literals
        self.choices = choices or {}            # symbol -> chosen variant index
        self.rename = rename or set()           # typedef/tag names to rename per function
        self.conflicts = OrderedDict()          # symbol -> [ {type, text, users} ]
        self.notes = []
        self.lit_map = {}
        self._parse()

    def _parse(self):
        self.pp, self.typedef_items, self.symdecls, self.bodies = [], OrderedDict(), OrderedDict(), []
        typedefs = {"size_t", "FILE", "va_list", "wchar_t", "ptrdiff_t"}
        per_func = []
        cache = {}
        for f in self.funcs:
            if not f.get("src") or not f["src"].endswith(".c"):
                raise SystemExit(f"{f['name']}: no C source in manifest (status {f.get('status')})")
            p = ROOT / f["src"]
            if p not in cache:
                cache[p] = items_of(p.read_text(), f["src"])
            its = cache[p]
            body = [it for it in its if it.kind == "func" and it.name == f["name"]]
            if len(body) != 1:
                raise SystemExit(f"{f['name']}: {len(body)} definitions in {f['src']}")
            for it in its:  # typedef names first so declarations parse
                if it.kind == "decl" and it.toks[0].text == "typedef":
                    for part in split_commas(split_specs(it.toks[1:-1], typedefs)[1]):
                        n, _ = norm_decl([], part, typedefs)
                        if n:
                            typedefs.add(n)
            per_func.append((f, its, body[0]))
        self.typedefs = typedefs
        for f, its, body in per_func:
            fname = f["name"]
            for it in its:
                if it.kind == "pp":
                    if it.text not in self.pp:
                        self.pp.append(it.text)
                elif it.kind == "decl":
                    self._decl(it, fname)
                elif it.kind == "func":
                    # other bodies in a shared source file become prototypes (they are not in this TU)
                    specs, dcl = split_specs(it.header, typedefs)
                    n, ty = norm_decl(specs, dcl, typedefs)
                    txt = tok_text(it.header) + ";"
                    self._add_sym(n, ty, txt, fname, defining=(it is body), declared=(it is not body))
            self.bodies.append((f, body))

    def _add_sym(self, name, ty, text, user, defining=False, declared=True):
        """Record one declaration of `name`; a definition in this TU counts for type checks but is
        emitted as a prototype only if some source declared the symbol separately."""
        vs = self.symdecls.setdefault(name, [])
        for v in vs:
            if v["type"] == ty:
                if user not in v["users"]:
                    v["users"].append(user)
                v["defining"] |= defining
                v["declared"] |= declared
                return
        vs.append({"type": ty, "text": text, "users": [user], "defining": defining, "declared": declared})

    def _decl(self, it, user):
        toks = it.toks[:-1]
        if toks[0].text == "typedef" or (any(t.text == "{" for t in toks) and
                                          toks[0].text in ("struct", "union", "enum")):
            key = self._tag_key(toks)
            text = tok_text(it.toks)
            vs = self.typedef_items.setdefault(key, [])
            for v in vs:
                if v["text"] == text:
                    if user not in v["users"]:
                        v["users"].append(user)
                    return
            vs.append({"text": text, "users": [user], "toks": it.toks})
            return
        specs, rest = split_specs(toks, self.typedefs)
        if not rest:  # "struct X;" forward declaration
            key = tok_text(toks)
            self.typedef_items.setdefault(key, [{"text": key + ";", "users": [user], "toks": it.toks}])
            return
        spec_txt = tok_text(specs)
        for part in split_commas(rest):
            n, ty = norm_decl(specs, part, self.typedefs)
            if n is None:
                continue
            self._add_sym(n, ty, (spec_txt + " " + tok_text(part)).strip() + ";", user)

    def _tag_key(self, toks):
        if toks[0].text == "typedef":
            specs, rest = split_specs(toks[1:], set())
            names = [norm_decl([], p, set())[0] for p in split_commas(rest)] if rest else []
            return "typedef " + ",".join(n for n in names if n)
        return f"{toks[0].text} {toks[1].text}"

    # ------------------------------------------------------------------------------ rendering
    def render(self):
        out = [f"/* TU {self.funcs[0]['name']}..{self.funcs[-1]['name']} "
               f"[{self.funcs[0]['start']}, {self.funcs[-1]['end']}): generated by tools/tu.py */"]
        out += self.pp
        renamed = {}
        for key, vs in self.typedef_items.items():
            if len(vs) > 1:
                self.conflicts.setdefault(key, [{"type": v["text"], "users": v["users"]} for v in vs])
            name = key.split(" ", 1)[1]
            if len(vs) > 1 and key in self.rename and re.fullmatch(r"\w+", name):
                for k, v in enumerate(vs):
                    new = f"{name}__{k}"
                    for u in v["users"]:
                        renamed[(u, name)] = new
                    out.append(re.sub(rf"\b{re.escape(name)}\b", new, v["text"]))
            else:
                out.append(vs[self.choices.get(key, 0)]["text"])
        lit_syms = {}
        if self.literals:
            for name, vs in self.symdecls.items():
                m = re.fullmatch(r"g_([0-9a-f]+)", name)
                if m and len(vs) == 1 and re.fullmatch(r"(const )?char \$ \[\]", vs[0]["type"]):
                    a = int(m.group(1), 16)
                    if 4 <= a < CONST_END:
                        lit_syms[name] = c_literal(a)
        self.lit_map = lit_syms
        for name, vs in self.symdecls.items():
            if name in lit_syms:
                continue
            if len(vs) > 1:
                self.conflicts[name] = [{"type": v["type"], "text": v["text"], "users": v["users"]} for v in vs]
            if not any(v["declared"] for v in vs):
                continue
            v = vs[self.choices.get(name, self._default_choice(vs))]
            out.append(v["text"])
        if self.defs:
            out.append("")
            out += self.defs
        for f, body in self.bodies:
            text = body.text
            for (u, name), new in renamed.items():
                if u == f["name"]:
                    text = re.sub(rf"\b{re.escape(name)}\b", new, text)
            if lit_syms:
                text = re.sub(r"\bg_[0-9a-f]+\b", lambda m: lit_syms.get(m.group(), m.group()), text)
            out.append("")
            out.append(text)
        return "\n".join(out) + "\n"

    @staticmethod
    def _default_choice(vs):
        for k, v in enumerate(vs):   # the definition's own type, else the most used variant
            if v["defining"]:
                return k
        return max(range(len(vs)), key=lambda k: len(vs[k]["users"]))


# --------------------------------------------------------------------------------- verification
def run_check(path: Path, profile, start, end, tag):
    js = WORK / "check" / f"{tag}.json"
    js.parent.mkdir(parents=True, exist_ok=True)
    if js.exists():
        js.unlink()
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "check.py"), str(path), "--all",
                        "--at", hex(start), "--end", hex(end), "--profile", profile, "--json", str(js)],
                       capture_output=True, text=True)
    if not js.exists():
        err = (r.stdout + r.stderr).strip().splitlines()
        errs = [l for l in err if "Error!" in l]
        return {"verdict": "COMPILE-ERROR", "problems": (errs or err[-6:])[:12], "score": 10 ** 6 + len(errs)}
    res = json.loads(js.read_text())
    return {"verdict": res["verdict"], "problems": res["problems"], "data": res.get("data", []),
            "new_bindings": res.get("new_bindings", {}), "diff": res.get("diff"),
            "score": 0 if res["verdict"] == "EXACT" else len(res["problems"]) + res.get("byte_diffs", 0)}


def build(funcs, out: Path | None = None, literals=False, resolve=True, tag=None, quiet=False, defs=()):
    """Synthesise + verify one TU; resolve declaration conflicts by trying every variant."""
    profs = {f.get("profile") for f in funcs}
    if len(profs) != 1:
        return {"verdict": "PROFILE-MIX", "profiles": sorted(profs), "functions": [f["name"] for f in funcs]}
    profile = profs.pop()
    start, end = int(funcs[0]["start"], 16), int(funcs[-1]["end"], 16)
    for a, b in zip(funcs, funcs[1:]):
        if a["end"] != b["start"]:
            return {"verdict": "NOT-CONTIGUOUS", "gap": [a["name"], b["name"]]}
    tag = tag or f"tu_{start:x}_{end:x}"
    out = out or WORK / "src" / f"{tag}.c"
    out.parent.mkdir(parents=True, exist_ok=True)
    tu = TU(funcs, literals=literals, defs=defs)
    out.write_text(tu.render())
    res = run_check(out, profile, start, end, tag)
    resolved = {}
    if res["verdict"] != "EXACT" and tu.conflicts and resolve:
        # Try variant combinations of the conflicting symbols (all of them when <= 64, else one symbol
        # at a time).  The first EXACT combination wins; otherwise the first one that compiles is kept
        # and the TU is reported with its unresolved type questions.
        syms = list(tu.conflicts)
        sizes = [len(tu.conflicts[s]) for s in syms]
        total = 1
        for n in sizes:
            total *= n
        tried = []

        def attempt(choices, rename=frozenset()):
            t2 = TU(funcs, literals=literals, choices=choices, rename=set(rename), defs=defs)
            out.write_text(t2.render())
            r2 = run_check(out, profile, start, end, tag)
            tried.append((dict(choices), set(rename), r2["verdict"]))
            return r2

        import itertools
        best = None
        if total <= 64:
            for combo in itertools.product(*[range(n) for n in sizes]):
                ch = dict(zip(syms, combo))
                r2 = attempt(ch)
                if r2["verdict"] == "EXACT":
                    best = (ch, set())
                    break
                if best is None or r2["score"] < best[2]:
                    best = (ch, set(), r2["score"])
        else:  # hill-climb: change one symbol at a time while the score (errors / problems) drops
            cur = {s: TU._default_choice(tu.symdecls[s]) if s in tu.symdecls else 0 for s in syms}
            cs = attempt(cur)["score"]
            # starting points: every function's own view of all conflicting symbols
            for u in [f["name"] for f in funcs]:
                view = dict(cur)
                for s in syms:
                    for k, v in enumerate(tu.conflicts[s]):
                        if u in v["users"]:
                            view[s] = k
                if view != cur:
                    sc = attempt(view)["score"]
                    if sc < cs:
                        cur, cs = view, sc
            for _ in range(3):
                improved = False
                for s, n in zip(syms, sizes):
                    for k in range(n):
                        if k == cur[s] or cs == 0:
                            continue
                        sc = attempt({**cur, s: k})["score"]
                        if sc < cs:
                            cur, cs, improved = {**cur, s: k}, sc, True
                if cs == 0 or not improved:
                    break
            best = (cur, set(), cs)
        best = best[:2]
        tags = {s for s in syms if s.startswith("typedef ") or s.split(" ")[0] in ("struct", "union", "enum")}
        if (best is None or tried[-1][2] != "EXACT") and tags:
            base = best[0] if best else {}
            if attempt(base, tags)["verdict"] != "COMPILE-ERROR":
                best = (base, tags)
        if best is None:
            best = ({}, set())
        for s in syms:
            if s in best[1]:
                resolved[s] = "renamed per function (unresolved type question)"
            elif s in best[0]:
                resolved[s] = tu.conflicts[s][best[0][s]]["type"]
        tu = TU(funcs, literals=literals, choices=best[0], rename=best[1], defs=defs)
        out.write_text(tu.render())
        res = run_check(out, profile, start, end, tag)
        res["attempts"] = len(tried) + 1
    res.update({"tag": tag, "source": str(out.relative_to(ROOT)).replace("\\", "/"), "profile": profile,
                "range": [hex(start), hex(end)], "functions": [f["name"] for f in funcs],
                "conflicts": {k: [{"type": v["type"], "users": v["users"]} for v in vs]
                              for k, vs in tu.conflicts.items()},
                "resolved": resolved, "literals": tu.lit_map})
    if not quiet:
        print(f"{res['verdict']} {tag} {len(funcs)} functions {hex(start)}..{hex(end)} profile {profile}"
              f" conflicts {len(tu.conflicts)} literals {len(tu.lit_map)} -> {res['source']}")
        for p in res.get("problems", [])[:8]:
            print("  -", p)
        for k, vs in res["conflicts"].items():
            print(f"  conflict {k}: " + " | ".join(f"{v['type']} ({','.join(v['users'][:3])})" for v in vs)
                  + (f"  => {resolved[k]}" if k in resolved else ""))
        for d in res.get("data", []):
            print(f"  data {d}")
    return res


def select(args, man=None):
    fs = functions(man)
    if args.range:
        a, b = (int(x, 16) for x in args.range)
        sel = [f for f in fs if a <= int(f["start"], 16) < b]
    else:
        by = {f["name"]: f for f in fs}
        sel = [by[n] for n in args.funcs]
        sel.sort(key=lambda f: int(f["start"], 16))
    return sel


def matched_runs(fs, lo=0, hi=1 << 32):
    runs, cur = [], []
    for f in fs:
        ok = (f.get("status") == "matching" and f.get("kind", "c").startswith("c") and
              str(f.get("src", "")).endswith(".c") and lo <= int(f["start"], 16) < hi)
        if ok and cur and cur[-1]["end"] == f["start"] and cur[-1].get("profile") == f.get("profile"):
            cur.append(f)
        else:
            if cur:
                runs.append(cur)
            cur = [f] if ok else []
    if cur:
        runs.append(cur)
    return runs


def scan(args):
    fs = functions()
    lo, hi = (int(x, 16) for x in args.range) if args.range else (0, 1 << 32)
    runs = matched_runs(fs, lo, hi)
    report = []
    for run in runs:
        ent = {"run": [f["name"] for f in run], "range": [run[0]["start"], run[-1]["end"]],
               "profile": run[0].get("profile"), "pairs": [], "segments": []}
        if len(run) == 1:
            r = build(run, literals=args.literals, quiet=True, tag=f"scan_{run[0]['name']}")
            ent["segments"].append({"functions": ent["run"], "verdict": r["verdict"]})
            report.append(ent)
            print(f"run {run[0]['name']} (1): {r['verdict']}", flush=True)
            continue
        for a, b in zip(run, run[1:]):
            r = build([a, b], literals=args.literals, quiet=True, tag=f"pair_{a['name']}")
            ent["pairs"].append({"pair": [a["name"], b["name"]], "verdict": r["verdict"],
                                 "conflicts": r.get("conflicts"), "resolved": r.get("resolved"),
                                 "problems": r.get("problems", [])[:4]})
        # greedy maximal EXACT windows (window limit optional)
        i = 0
        while i < len(run):
            j = i + 1
            last = build(run[i:j], literals=args.literals, quiet=True, tag=f"seg_{run[i]['name']}")
            while j < len(run) and (not args.window or j - i < args.window):
                r = build(run[i:j + 1], literals=args.literals, quiet=True, tag=f"seg_{run[i]['name']}")
                if r["verdict"] != "EXACT":
                    break
                last, j = r, j + 1
            ent["segments"].append({"functions": [f["name"] for f in run[i:j]],
                                    "range": [run[i]["start"], run[j - 1]["end"]], "verdict": last["verdict"],
                                    "conflicts": last.get("conflicts"), "resolved": last.get("resolved"),
                                    "stopped_by": run[j]["name"] if j < len(run) else None})
            i = j
        print(f"run {run[0]['name']}..{run[-1]['name']} ({len(run)}): "
              + " / ".join(f"{s['functions'][0]}..{s['functions'][-1]}[{len(s['functions'])}] {s['verdict']}"
                           for s in ent["segments"]), flush=True)
        report.append(ent)
    out = Path(args.out) if args.out else WORK / "scan.json"
    out.write_text(json.dumps(report, indent=1))
    print("json:", out)


# ------------------------------------------------------------------------------------ evidence
def data_refs(fs):
    """function name -> sorted obj3 target offsets referenced from its code (LE fixups)."""
    obj3()
    L = _ORIG["L"]
    starts = [int(f["start"], 16) for f in fs]
    import bisect
    refs = defaultdict(set)
    dptr = {}
    for obj, off, typ, tgt in L.resolved_fixups():
        if tgt.get("obj") != 3 or "off" not in tgt:
            continue
        if obj == 1:
            i = bisect.bisect_right(starts, off) - 1
            if i >= 0 and off < int(fs[i]["end"], 16):
                refs[fs[i]["name"]].add(tgt["off"])
        elif obj == 3:
            dptr[off] = tgt["off"]
    return {k: sorted(v) for k, v in refs.items()}, dptr


def const_blocks(refs, dptr):
    """Split CONST into strings; group packed strings into blocks (one object each at most)."""
    d = obj3()
    users = defaultdict(list)
    for n, rs in refs.items():
        for a in rs:
            if a < CONST_END:
                users[a].append(n)
    for site, a in dptr.items():
        if a < CONST_END:
            users[a].append(f"D{site:x}")
    strings, p = [], 4
    while p < CONST_END:
        if d[p] == 0:
            q = p
            while q < CONST_END and d[q] == 0:
                q += 1
            refd = [a for a in range(p, q) if users.get(a)]
            k = p
            for a in refd:                              # referenced empty literals ("")
                if a > k:
                    strings.append({"at": k, "zeros": a - k, "users": []})
                strings.append({"at": a, "len": 1, "text": "", "users": users[a], "inner_users": []})
                k = a + 1
            if q > k:
                strings.append({"at": k, "zeros": q - k, "users": []})
            p = q
            continue
        q = d.index(0, p)
        inner = sorted({u for a in range(p + 1, q + 1) for u in users.get(a, [])})
        strings.append({"at": p, "len": q + 1 - p, "text": d[p:q].decode("latin-1")[:40],
                        "users": users.get(p, []), "inner_users": inner})
        p = q + 1
    # Block = literals packed without padding.  A new block (object) starts
    #  * after zero padding that ends on a dword boundary, or
    #  * at a data-initialiser literal that follows a code-only literal (data literals come first).
    blocks, cur, why = [], None, None
    for s in strings:
        if "zeros" in s:
            if cur and (s["at"] + s["zeros"]) % 4 == 0:
                cur["end"] = s["at"]
                blocks.append(cur)
                cur, why = None, "dword padding"
            continue
        is_data = any(u.startswith("D") for u in s["users"])
        if cur is not None and is_data and cur["code_seen"] and s["at"] % 4 == 0:
            cur["end"] = s["at"]
            blocks.append(cur)
            cur, why = None, "data-initialiser literal after code literal"
        elif cur is not None and is_data and cur["code_seen"]:
            cur.setdefault("contradictions", []).append(f"data literal {s['at']:#x} after code literal, unaligned")
        if cur is None:
            cur = {"start": s["at"], "items": [], "code_seen": False, "why": why}
        cur["items"].append(s)
        if s["users"] and not is_data:
            cur["code_seen"] = True
    if cur:
        cur["end"] = CONST_END
        blocks.append(cur)
    out = []
    for b in blocks:
        code = [u for it in b["items"] for u in it["users"] + it.get("inner_users", []) if not u.startswith("D")]
        data = [u for it in b["items"] for u in it["users"] + it.get("inner_users", []) if u.startswith("D")]
        out.append({"range": [hex(b["start"]), hex(b["end"])], "strings": len(b["items"]),
                    "code_users": list(OrderedDict.fromkeys(code)),
                    "data_pointers": [f"D{x:x}" for x in sorted({int(u[1:], 16) for u in data})],
                    "first": b["items"][0].get("text", ""), "aligned_start": b["start"] % 4 == 0,
                    "split_by": b["why"], "contradictions": b.get("contradictions", [])})
    return strings, out


def evidence(args):
    fs = functions()
    refs, dptr = data_refs(fs)
    strings, blocks = const_blocks(refs, dptr)
    lit_users = defaultdict(set)
    for n, rs in refs.items():
        for a in rs:
            if a < CONST_END:
                lit_users[a].add(n)
    shared = {hex(a): sorted(u) for a, u in lit_users.items() if len(u) > 1}
    d = obj3()
    texts = defaultdict(list)
    for s in strings:
        if "len" in s:
            texts[d[s["at"]:s["at"] + s["len"]]].append(hex(s["at"]))
    dup = {k.decode("latin-1"): v for k, v in texts.items() if len(v) > 1}
    cls = lambda a: "CONST" if a < CONST_END else ("_DATA" if a < BSS_START else "_BSS")
    per_func = {}
    for f in fs:
        rs = refs.get(f["name"], [])
        per_func[f["name"]] = {c: [hex(a) for a in rs if cls(a) == c] for c in ("CONST", "_DATA", "_BSS")}
    users = defaultdict(set)
    for n, rs in refs.items():
        for a in rs:
            users[a].add(n)
    private = defaultdict(list)
    for a, u in users.items():
        if len(u) == 1:
            private[next(iter(u))].append(a)
    ev = {"const_end": hex(CONST_END), "const_blocks": blocks, "shared_code_literals": shared,
          "duplicated_literals": dup,
          "data_pointers_to_const": {hex(s): hex(t) for s, t in sorted(dptr.items()) if t < CONST_END},
          "functions": per_func,
          "data_users": {hex(a): sorted(u, key=lambda n: int(n.split("_")[1], 16))
                         for a, u in sorted(users.items())}}
    out = Path(args.out) if args.out else WORK / "evidence.json"
    out.write_text(json.dumps(ev, indent=1))
    for b in blocks:
        print(f"CONST {b['range'][0]}..{b['range'][1]} {b['strings']:3d} str  code {','.join(b['code_users']) or '-'}"
              f"  dataptrs {len(b['data_pointers'])}  {b['first']!r}"[:150])
    print("shared literals:", shared)
    print("duplicated literals:", dup)
    print("json:", out)


# ------------------------------------------------------------------------------------- verify
def verify(args):
    prop = json.loads(Path(args.proposal).read_text())
    man = manifest()
    by = {f["name"]: f for f in functions(man)}
    out = []
    for tu in prop:
        members = [by[n] for n in tu["functions"]]
        members.sort(key=lambda f: int(f["start"], 16))
        ent = {"id": tu["id"], "functions": [f["name"] for f in members],
               "range": [members[0]["start"], members[-1]["end"]],
               "profile": sorted({f.get("profile") for f in members}),
               "evidence": tu.get("evidence", []), "data": tu.get("data", {}),
               "grouping": tu.get("grouping", "HYPOTHESIS"), "builds": []}
        runs = matched_runs(members)
        unmatched = [f["name"] for f in members if f.get("status") != "matching"]
        for run in runs:
            r = build(run, literals=tu.get("literals", True), tag=f"{tu['id']}_{run[0]['name']}")
            ent["builds"].append({k: r.get(k) for k in ("functions", "range", "verdict", "source", "conflicts",
                                                          "resolved", "literals", "data", "problems")})
        verdicts = [b["verdict"] for b in ent["builds"]]
        confl = any(b.get("conflicts") for b in ent["builds"])
        if not unmatched and len(runs) == 1 and verdicts == ["EXACT"]:
            ent["status"] = "conflict" if any(v.startswith("renamed") for b in ent["builds"]
                                               for v in (b.get("resolved") or {}).values()) else "exact"
        elif any(v != "EXACT" for v in verdicts):
            ent["status"] = "conflict"
        else:
            ent["status"] = "partial"
        ent["type_conflicts"] = confl
        ent["unmatched"] = unmatched
        if len(ent["profile"]) == 1:
            ent["profile"] = ent["profile"][0]
        out.append(ent)
        print(f"{ent['id']:8s} {ent['status']:8s} {ent['range'][0]}..{ent['range'][1]} "
              f"{len(members)} fn, {len(unmatched)} unmatched, builds {verdicts}")
    dst = Path(args.out) if args.out else ROOT / "build" / "tus.json"
    dst.write_text(json.dumps(out, indent=1))
    print("json:", dst)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("funcs", nargs="*")
    b.add_argument("--range", nargs=2)
    b.add_argument("--out")
    b.add_argument("--literals", action="store_true")
    b.add_argument("--no-resolve", action="store_true")
    b.add_argument("--defs", help="file with the TU's own data definitions (C, definition order)")
    s = sub.add_parser("scan")
    s.add_argument("--range", nargs=2)
    s.add_argument("--window", type=int)
    s.add_argument("--literals", action="store_true")
    s.add_argument("--out")
    e = sub.add_parser("evidence")
    e.add_argument("--out")
    v = sub.add_parser("verify")
    v.add_argument("proposal")
    v.add_argument("--out")
    a = ap.parse_args(argv[1:])
    if a.cmd == "build":
        fs = select(a)
        if not fs:
            raise SystemExit("no functions selected")
        defs = Path(a.defs).read_text().splitlines() if a.defs else ()
        r = build(fs, Path(a.out) if a.out else None, a.literals, not a.no_resolve, defs=defs)
        return 0 if r["verdict"] == "EXACT" else 1
    if a.cmd == "scan":
        scan(a)
    elif a.cmd == "evidence":
        evidence(a)
    elif a.cmd == "verify":
        verify(a)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
