"""Restructure lifter goto code into if/else/while/do/for/switch (wcc386 -d2 jump layouts).

    python tools/structure.py src/u_08585.c --out build/workers/struct/src/u_08585.c [--func NAME]... [--no-verify]

Model (PROVEN by probes; docs/compiler-notes.md "Control-flow layouts", evidence and probes in
build/workers/struct/RULES.md): -d2 emits every construct from a fixed template and then
only (a) drops unreachable code, (b) drops a jump to the next instruction (a jcc leaves its compare), (c) turns
`jcc L1; jmp L2; L1:` into `j!cc L2` when nothing else jumps to the jmp.  Conditions (FJ = jump when false,
TJ = jump when true, Lx fresh labels):
    FJ(a&&b,F) = FJ(a,L1) TJ(b,L2) L1: jmp F L2:      FJ(a||b,F) = TJ(a,L1) FJ(b,F) L1:
    TJ(a&&b,T) = FJ(a,L1) TJ(b,T) L1:                 TJ(a||b,T) = TJ(a,L1) FJ(b,L2) L1: jmp T L2:
    if: FJ(c,E) S E:   if/else: FJ(c,Le) S1 jmp E Le: S2 E:   while: T: FJ(c,E) S C: jmp T E:
    do: T: S C: TJ(c,T) E:   for: I T: TJ(c,B) jmp E N: step jmp T B: S jmp N E:   return: jmp <epilogue>
    while (1): T: S C: jmp T E:   for (;;): jmp B N: step B: S jmp N E:   break/continue: jmp E / jmp C (for: N)
    switch: store temp; compare tree on value ranges (split at (n-1)//2) or jump table; case bodies in source order
The tool lowers a function (goto form and any structured statements) to that jump program, parses the program
back with the templates (largest structures first, unmatched jumps stay gotos; `v = e;` + a compare tree on the
temp v becomes `switch (e)`), checks that the new text lowers to the same program, and finally keeps a function
only if `tools/check.py` says the whole unit is still EXACT (first with `X != 0` printed as `X`, then literally).
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ------------------------------------------------------------------------------------------ tokens
TOK = re.compile(r"""(?P<ws>\s+)|(?P<com>/\*.*?\*/|//[^\n]*)|(?P<str>"(?:\\.|[^"\\\n])*")|(?P<chr>'(?:\\.|[^'\\\n])*')
 |(?P<id>[A-Za-z_]\w*)|(?P<num>\.?\d(?:[eE][+-]|[\w.])*)
 |(?P<op>->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|\.\.\.|[{}()\[\];,:?.~!<>=+\-*/%&|^#])""",
                 re.S | re.X)
KEYWORDS = {"if", "else", "while", "do", "for", "switch", "case", "default", "goto", "break", "continue", "return",
            "sizeof"}
TYPEWORDS = {"int", "char", "short", "long", "unsigned", "signed", "void", "struct", "union", "enum", "float",
             "double", "const", "volatile", "register", "static", "extern", "typedef", "auto"}
REL = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}
BINOPS = {"+", "-", "*", "/", "%", "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "&", "^", "|", "&&", "||", "?", ":",
          "=", ",", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="}


class Unsupported(Exception):
    pass


def tokenize(text, base=0):
    out, i = [], 0
    while i < len(text):
        m = TOK.match(text, i)
        if not m:
            raise Unsupported(f"cannot tokenize at {text[i:i + 20]!r}")
        if m.lastgroup != "ws":
            out.append((m.lastgroup, m.group(), base + m.start(), base + m.end()))
        i = m.end()
    return out


def top_ops(toks):
    """Binary operators at paren depth 0 (unary uses excluded)."""
    ops, depth, prev = [], 0, None
    for t in toks:
        s = t[1]
        if s in "([":
            depth += 1
        elif s in ")]":
            depth -= 1
        elif depth == 0 and t[0] == "op" and s in BINOPS:
            if prev is not None and (prev[0] != "op" or prev[1] in (")", "]", "++", "--")):
                ops.append(s)
        prev = t
    return ops


def wrapped(toks):
    """True if toks is one parenthesised group."""
    if not toks or toks[0][1] != "(" or toks[-1][1] != ")":
        return False
    depth = 0
    for k, t in enumerate(toks):
        depth += t[1] in "([" and t[0] == "op"
        depth -= t[1] in ")]" and t[0] == "op"
        if depth == 0 and k < len(toks) - 1:
            return False
    return True


def text_of(toks):
    out = ""
    for k, t in enumerate(toks):
        if k and (t[2] > toks[k - 1][3]):
            out += " "
        out += t[1]
    return out


def neg(text):
    """The condition that is true exactly when `text` is false (an involution on the forms it produces)."""
    toks = tokenize(text)
    if wrapped(toks):
        return neg(text_of(toks[1:-1]))
    ops = top_ops(toks)
    rel = [o for o in ops if o in REL]
    if len(rel) == 1 and not set(ops) & {"&&", "||", "?", ":", ",", "&", "|", "^"} and not any(
            o.endswith("=") and o not in REL for o in ops):
        depth, out = 0, []
        for t in toks:
            depth += t[1] in "([" and t[0] == "op"
            depth -= t[1] in ")]" and t[0] == "op"
            out.append((t[0], REL[t[1]] if depth == 0 and t[1] == rel[0] else t[1], t[2], t[3]))
        return text_of(out)
    if toks[0][1] == "!" and not top_ops(toks[1:]):
        rest = toks[1:]
        return text_of(rest[1:-1] if wrapped(rest) else rest)
    if not ops:
        return "!" + text
    return "!(" + text + ")"


def simplify(text):
    """`X != 0` -> `X`, `X == 0` -> `!X` (code-identical: probe cz.c)."""
    toks = tokenize(text)
    while wrapped(toks):
        toks = toks[1:-1]
    ops = top_ops(toks)
    if len(toks) > 2 and ops == [toks[-2][1]] and toks[-2][1] in ("==", "!=") and toks[-1][1] == "0":
        x = toks[:-2]
        while wrapped(x):
            x = x[1:-1]
        return text_of(x) if toks[-2][1] == "!=" else neg(text_of(x))
    return text_of(toks)


SIMPLIFY = [True]


def norm(text):
    toks = tokenize(text)
    while wrapped(toks):
        toks = toks[1:-1]
    return "".join(t[1] for t in toks)


# ------------------------------------------------------------------------------------------ conditions
def parse_cond(toks):
    """-> ('or'|'and', a, b) | ('not', x) | ('atom', text) | ('true',)"""
    while wrapped(toks):
        toks = toks[1:-1]
    ops = top_ops(toks)
    if "?" not in ops and not any(o in ("=", ",") or (o.endswith("=") and o not in REL) for o in ops):
        for op in ("||", "&&"):
            if op in ops:
                depth, cut = 0, None
                for k, t in enumerate(toks):
                    depth += t[1] in "([" and t[0] == "op"
                    depth -= t[1] in ")]" and t[0] == "op"
                    if depth == 0 and t[1] == op:
                        cut = k
                return ("or" if op == "||" else "and", parse_cond(toks[:cut]), parse_cond(toks[cut + 1:]))
        if toks[0][1] == "!" and wrapped(toks[1:]):
            inner = parse_cond(toks[2:-1])
            if inner[0] != "atom":
                return ("not", inner)
    if len(toks) == 1 and toks[0][0] == "num" and toks[0][1] not in ("0", "0x0"):
        return ("true",)
    return ("atom", text_of(toks))


def cond_text(c, ctx=None):
    k = c[0]
    if k == "atom":
        t = simplify(c[1]) if SIMPLIFY[0] else c[1]
        return f"({t})" if ctx and set(top_ops(tokenize(t))) & {"&&", "||", "?", "=", ","} else t
    if k == "true":
        return "1"
    if k == "not":
        return "!(" + cond_text(c[1]) + ")"
    a, b = cond_text(c[1], k), cond_text(c[2], k)
    if k == "and":
        a = f"({a})" if c[1][0] == "or" else a
        b = f"({b})" if c[2][0] in ("or", "and") else b
        return f"{a} && {b}"
    a = f"({a})" if c[1][0] == "and" else a
    b = f"({b})" if c[2][0] in ("or", "and") else b
    return f"{a} || {b}"


# ------------------------------------------------------------------------------------------ statements
class Parser:
    def __init__(self, toks, src, typedefs):
        self.t, self.i, self.src, self.typedefs, self.hoisted = toks, 0, src, typedefs, []

    def peek(self, k=0):
        j = self.i
        while j < len(self.t) and self.t[j][0] == "com":
            j += 1
        for _ in range(k):
            j += 1
            while j < len(self.t) and self.t[j][0] == "com":
                j += 1
        return self.t[j] if j < len(self.t) else ("eof", "", 0, 0)

    def comments(self):
        out = []
        while self.i < len(self.t) and self.t[self.i][0] == "com":
            out.append(self.t[self.i][1])
            self.i += 1
        return out

    def take(self, s=None):
        t = self.t[self.i]
        if t[0] == "com":
            raise Unsupported("comment inside a statement")
        if s is not None and t[1] != s:
            raise Unsupported(f"expected {s!r} got {t[1]!r}")
        self.i += 1
        return t

    def until(self, stop):
        """Tokens up to (not incl.) `stop` at depth 0; consumes the stop token."""
        out, depth = [], 0
        while True:
            t = self.t[self.i]
            self.i += 1
            if t[0] == "com":
                raise Unsupported("comment inside a statement")
            if depth == 0 and t[1] == stop and t[0] == "op":
                return out
            depth += t[1] in "([{" and t[0] == "op"
            depth -= t[1] in ")]}" and t[0] == "op"
            out.append(t)

    def paren(self):
        self.take("(")
        return self.until(")")

    def is_decl(self):
        a, b, c = self.peek(), self.peek(1), self.peek(2)
        if a[1] in TYPEWORDS or a[1] in self.typedefs:
            return True
        return a[0] == "id" and a[1] not in KEYWORDS and b[0] == "id" and b[1] not in KEYWORDS

    def block_body(self, top=False):
        """Statements up to the matching '}' (consumed). Returns (decl_text, stmts)."""
        decl_start = self.i
        stmts = []
        while self.peek()[1] != "}" and self.is_decl():
            if self.t[self.i][0] == "com":
                raise Unsupported("comment before a declaration")
            d = self.until(";")
            if not top:     # hoisted to the function block (same declaration order); `T x = e;` -> `x = e;`
                ops = top_ops(d)
                if "," in ops or any(t[1] == "{" for t in d):
                    raise Unsupported("complex nested declaration")
                k = next((n for n, t in enumerate(d) if t[1] == "="), len(d))
                name = [t[1] for t in d[:k] if t[0] == "id"][-1]
                self.hoisted.append((name, text_of(d[:k])))
                if k < len(d):
                    stmts.append(("simple", f"{name} = {text_of(d[k + 1:])}"))
        decls = self.t[decl_start:self.i] if top else []
        while True:
            com = self.comments()
            stmts += [("comment", c) for c in com]
            if self.t[self.i][1] == "}":
                self.i += 1
                return decls, stmts
            stmts.append(self.stmt())

    def stmt(self):
        if self.t[self.i][0] == "com":
            raise Unsupported("comment before a nested statement")
        t = self.peek()
        s = t[1]
        if s == "{":
            self.take()
            return ("block", self.block_body()[1])
        if s == ";":
            self.take()
            return ("empty",)
        if t[0] == "id" and self.peek(1)[1] == ":" and s not in KEYWORDS:
            self.take(), self.take()
            return ("label", s)
        if s == "if":
            self.take()
            c = parse_cond(self.paren())
            a = self.stmt()
            if self.peek()[1] == "else":
                self.take()
                return ("if", c, a, self.stmt())
            return ("if", c, a, None)
        if s == "while":
            self.take()
            c = parse_cond(self.paren())
            return ("while", c, self.stmt())
        if s == "do":
            self.take()
            body = self.stmt()
            self.take("while")
            c = parse_cond(self.paren())
            self.take(";")
            return ("do", body, c)
        if s == "for":
            self.take(), self.take("(")
            init, cond, step = self.until(";"), self.until(";"), self.until(")")
            return ("for", text_of(init) or None, parse_cond(cond) if cond else None, text_of(step) or None,
                    self.stmt())
        if s == "switch":
            self.take()
            hdr = self.paren()
            self.take("{")
            return ("switch", text_of(hdr), self.block_body()[1])
        if s == "case":
            self.take()
            return ("case", text_of(self.until(":")))
        if s == "default" and self.peek(1)[1] == ":":
            self.take(), self.take()
            return ("case", None)
        if s in ("case", "default", "else"):
            raise Unsupported(f"stray {s}")
        if s == "goto":
            self.take()
            name = self.take()[1]
            self.take(";")
            return ("goto", name)
        if s in ("break", "continue"):
            self.take(), self.take(";")
            return (s,)
        if s == "return":
            self.take()
            e = self.until(";")
            return ("return", text_of(e) if e else None)
        e = self.until(";")
        return ("simple", self.src[e[0][2]:e[-1][3]] if e else "")


# ------------------------------------------------------------------------------------------ lowering
class Op:
    __slots__ = ("k", "c", "s", "tgt", "text", "pre", "has", "cases", "end", "var")

    def __init__(self, k, c=None, s=None, tgt=None, text=None, has=()):
        self.k, self.c, self.s, self.tgt, self.text, self.pre, self.has = k, c, s, tgt, text, [], set(has)
        self.cases, self.end = None, None

    def key(self):
        c = None if self.c is None else norm(simplify(self.c if self.s else neg(self.c)))
        cases = self.cases and tuple((x and norm(x), p) for x, p in self.cases)
        return (self.k, c, self.tgt, None if self.text is None else re.sub(r"\s+", "", self.text), cases, self.end)


RET = "$RET"


class Lower:
    """Structured/goto statements -> jump program: list of Op and ('L', name) items."""

    def __init__(self):
        self.out, self.n, self.pending = [], 0, []

    def new(self):
        self.n += 1
        return f"$L{self.n}"

    def emit(self, op):
        op.pre, self.pending = self.pending, []
        self.out.append(op)

    def label(self, name):
        self.out.append(("L", name))

    def fj(self, c, F):
        k = c[0]
        if k == "atom":
            self.emit(Op("J", c[1], False, F))
        elif k == "true":
            pass
        elif k == "not":
            self.tj(c[1], F)
        elif k == "and":
            l1, l2 = self.new(), self.new()
            self.fj(c[1], l1), self.tj(c[2], l2), self.label(l1), self.emit(Op("G", tgt=F)), self.label(l2)
        else:
            l1 = self.new()
            self.tj(c[1], l1), self.fj(c[2], F), self.label(l1)

    def tj(self, c, T):
        k = c[0]
        if k == "atom":
            self.emit(Op("J", c[1], True, T))
        elif k == "true":
            self.emit(Op("G", tgt=T))
        elif k == "not":
            self.fj(c[1], T)
        elif k == "and":
            l1 = self.new()
            self.fj(c[1], l1), self.tj(c[2], T), self.label(l1)
        else:
            l1, l2 = self.new(), self.new()
            self.tj(c[1], l1), self.fj(c[2], l2), self.label(l1), self.emit(Op("G", tgt=T)), self.label(l2)

    def stmt(self, s, brk=None, cont=None):
        k = s[0]
        if k == "simple":
            self.emit(Op("S", text=s[1]))
        elif k == "switch":
            e = self.new()
            op = Op("SW", text=s[1])
            op.cases, op.end = [], e
            self.emit(op)
            for x in s[2]:
                if x[0] == "case":
                    lab = self.new()
                    op.cases.append((x[1], lab))
                    self.label(lab)
                else:
                    self.stmt(x, e, cont)
            op.has = {lab for _, lab in op.cases} | ({e} if None not in [c for c, _ in op.cases] else set())
            self.label(e)
        elif k == "case":
            raise Unsupported("case label below the top level of its switch")
        elif k == "comment":
            self.pending.append(s[1])
        elif k == "empty":
            pass
        elif k == "label":
            self.label(s[1])
        elif k == "goto":
            self.emit(Op("G", tgt=s[1]))
        elif k in ("break", "continue"):
            t = brk if k == "break" else cont
            if t is None:
                raise Unsupported(f"{k} outside a loop")
            self.emit(Op("G", tgt=t))
        elif k == "return":
            if s[1] is not None:
                self.emit(Op("RS", text=s[1]))
            self.emit(Op("G", tgt=RET))
        elif k == "block":
            for x in s[1]:
                self.stmt(x, brk, cont)
        elif k == "if":
            if s[3] is None:
                e = self.new()
                self.fj(s[1], e), self.stmt(s[2], brk, cont), self.label(e)
            else:
                le, e = self.new(), self.new()
                self.fj(s[1], le), self.stmt(s[2], brk, cont), self.emit(Op("G", tgt=e))
                self.label(le), self.stmt(s[3], brk, cont), self.label(e)
        elif k == "while":
            top, c, e = self.new(), self.new(), self.new()
            self.label(top), self.fj(s[1], e), self.stmt(s[2], e, c)
            self.label(c), self.emit(Op("G", tgt=top)), self.label(e)
        elif k == "do":
            top, c, e = self.new(), self.new(), self.new()
            self.label(top), self.stmt(s[1], e, c), self.label(c), self.tj(s[2], top), self.label(e)
        elif k == "for":
            _, init, cond, step, body = s
            top, b, st, e = self.new(), self.new(), self.new(), self.new()
            if init:
                self.emit(Op("S", text=init))
            if cond is not None and cond[0] != "true":
                self.label(top), self.tj(cond, b), self.emit(Op("G", tgt=e)), self.label(st)
                if step:
                    self.emit(Op("S", text=step))
                self.emit(Op("G", tgt=top))
            else:
                self.emit(Op("G", tgt=b)), self.label(st)
                if step:
                    self.emit(Op("S", text=step))
            self.label(b), self.stmt(body, e, st), self.emit(Op("G", tgt=st)), self.label(e)
        else:
            raise Unsupported(k)


def optimise(items):
    """The -d2 clean-ups: unreachable code, jumps to the next instruction, jcc over an unlabelled jmp."""
    items = list(items)
    changed = True
    while changed:
        changed = False
        refs = set()
        for it in items:
            if isinstance(it, Op):
                if it.tgt is not None:
                    refs.add(it.tgt)
                refs |= it.has
        out, dead = [], False
        for it in items:
            if isinstance(it, tuple):
                if it[1] in refs or it[1] == RET:
                    dead = False
                out.append(it)
            elif dead:
                changed = True
            else:
                out.append(it)
                dead = it.k in ("G", "SW")     # a switch dispatch always jumps
        items = out

        def next_labels(k):
            labs = set()
            while k < len(items) and isinstance(items[k], tuple):
                labs.add(items[k][1])
                k += 1
            return labs, k

        for k, it in enumerate(items):
            if isinstance(it, Op) and it.k in ("G", "J"):
                labs, _ = next_labels(k + 1)
                if it.tgt in labs:
                    if it.k == "G":
                        items[k] = ("L", "$gone")
                    else:
                        op = Op("K", it.c, it.s)
                        op.pre = it.pre
                        items[k] = op
                    changed = True
                    break
                between, g = next_labels(k + 1)
                if it.k == "J" and g < len(items) and items[g].k == "G" and not between & refs:
                    labs, _ = next_labels(g + 1)
                    if it.tgt in labs:
                        op = Op("J", it.c, not it.s, items[g].tgt)
                        op.pre = it.pre + items[g].pre
                        items[k:g + 1] = [op]
                        changed = True
                        break
        items = [it for it in items if not (isinstance(it, tuple) and it[1] == "$gone")]
    return items


class Program:
    """Flat jump program: ops with integer targets; names[pos] = source label names at that position."""

    def __init__(self, items):
        self.ops, pos, self.names = [], {}, {}
        for it in items:
            if isinstance(it, tuple):
                pos[it[1]] = len(self.ops)
                self.names.setdefault(len(self.ops), []).append(it[1])
            else:
                self.ops.append(it)
        self.ret = len(self.ops)
        pos[RET] = self.ret
        for op in self.ops:
            if op.tgt is not None:
                if op.tgt not in pos:
                    raise Unsupported(f"undefined label {op.tgt}")
                op.tgt = pos[op.tgt]
            op.has = {pos[h] for h in op.has}
            if op.k == "SW":
                op.cases, op.end = [(c, pos[lab]) for c, lab in op.cases], pos[op.end]

    def signature(self):
        return [op.key() for op in self.ops]


# ------------------------------------------------------------------------------------------ lifted switches
FLIP = {"<": ">", ">": "<", "<=": ">=", ">=": "<=", "==": "==", "!=": "!="}
NEGREL = {"<": ">=", ">=": "<", ">": "<=", "<=": ">", "==": "!=", "!=": "=="}
TEST = {"<": int.__lt__, "<=": int.__le__, ">": int.__gt__, ">=": int.__ge__, "==": int.__eq__, "!=": int.__ne__}


def var_cmp(text, var):
    """`var REL const` (either order) -> (rel, const, literal) or None."""
    toks = tokenize(text)
    while wrapped(toks):
        toks = toks[1:-1]
    if len(toks) != 3 or toks[1][1] not in REL or toks[0][1] != var and toks[2][1] != var:
        return None
    a, r, b = toks
    rel, lit = (r[1], b[1]) if a[1] == var else (FLIP[r[1]], a[1])
    try:
        return rel, int(lit.rstrip("uUlL"), 0), lit
    except ValueError:
        return None


def dispatch(ranges, D, var):
    """wcc386 compare-tree dispatch for sorted (lo, hi, target) ranges (probes sw3.c): split at (n-1)//2,
    `cmp lo; jb Llow; cmp hi; jbe T; <upper>; Llow: <lower>`; one-value leaf `cmp v; je T; jmp D`."""
    items, n = [], [0]

    def node(rs, lo, hi):
        if not rs:
            items.append(Op("G", tgt=D))
            return
        if len(rs) == 1 and rs[0][0] == rs[0][1]:
            items.extend([Op("J", f"{var} == {rs[0][0]}", True, rs[0][2]), Op("G", tgt=D)])
            return
        k = (len(rs) - 1) // 2
        a, b, t = rs[k]
        n[0] += 1
        low = f"$sw{n[0]}"
        if lo is None or a > lo:
            items.append(Op("J", f"{var} < {a}", True, low))
        items.append(Op("J", f"{var} <= {b}", True, t))
        node(rs[k + 1:], b + 1, hi)
        items.append(("L", low))
        node(rs[:k], lo, a - 1)
    node(ranges, None, None)
    return items


def recover_switches(prog, names):
    """`v = e;` + a compare tree on the block-scope temp v  ->  one switch op (v was the switch temp)."""
    ops, done = prog.ops, {}
    for i, op in enumerate(ops):
        m = op.k == "S" and re.fullmatch(r"\s*(\w+)\s*=\s*([^=].*)", op.text, re.S)
        if not m or m.group(1) not in names:
            continue
        var, expr = m.groups()
        if re.search(rf"\b{var}\b", expr):
            continue
        r = i + 1
        while r < len(ops) and (ops[r].k == "G" or ops[r].k == "J" and var_cmp(ops[r].c, var)):
            r += 1
        for end in range(r, i + 2, -1):
            sw = match_tree(prog, i, end, var, expr)
            if sw:
                done[i] = (end, sw)
                break
    if not done:
        return prog, set()
    items = []
    lab = lambda x: f"@{x}"
    skip = set()
    for i, (end, sw) in done.items():
        skip |= set(range(i + 1, end))
    for pos in range(len(ops) + 1):
        if pos not in skip:
            items.append(("L", lab(pos)))
            items += [("L", nm) for nm in prog.names.get(pos, []) if nm != RET]
        if pos == len(ops) or pos in skip:
            continue
        o = done[pos][1] if pos in done else ops[pos]
        c = Op(o.k, o.c, o.s, None if o.tgt is None else lab(o.tgt), o.text, {lab(h) for h in o.has})
        c.pre = o.pre
        if o.k == "SW":
            c.cases, c.end = [(x, lab(t)) for x, t in o.cases], lab(o.end)
        items.append(c)
    items.append(("L", RET))
    return Program(items), {sw.var for _, sw in done.values()}


def match_tree(prog, i, end, var, expr):
    ops = prog.ops
    inside = set(range(i + 1, end))
    if any(o.tgt in inside - {i + 1} or o.tgt == i + 1 for k, o in enumerate(ops) if k not in inside and k != i):
        return None
    for k, o in enumerate(ops):
        if k not in inside and k != i and (re.search(rf"\b{var}\b", (o.text or "") + (o.c or ""))):
            return None
    consts = sorted({var_cmp(o.c, var)[1] for o in ops[i + 1:end] if o.k == "J"})
    if not consts or consts[0] < 0 or consts[-1] - consts[0] > 4096:
        return None

    def run(v):
        p, steps = i + 1, 0
        while i < p < end and steps < 1000:
            o, steps = ops[p], steps + 1
            if o.k == "G":
                p = o.tgt
            else:
                rel, c, _ = var_cmp(o.c, var)
                p = o.tgt if TEST[rel](v, c) == o.s else p + 1
        return p if not i < p < end else None
    D = run(consts[-1] + 1)
    if D is None or D in inside or run(consts[0] - 1) not in (D, None) and consts[0] > 0:
        return None
    cases = [(v, run(v)) for v in range(max(consts[0] - 1, 0), consts[-1] + 2)]
    if any(t is None for _, t in cases):
        return None
    cases = [(v, t) for v, t in cases if t != D]
    if not cases or any(t in inside or t > D for _, t in cases):
        return None
    ranges = []
    for v, t in cases:
        if ranges and ranges[-1][1] == v - 1 and ranges[-1][2] == t:
            ranges[-1] = (ranges[-1][0], v, t)
        else:
            ranges.append((v, v, t))
    model = dispatch([(a, b, f"@{t}") for a, b, t in ranges], f"@{D}", var) + [("L", f"@{end}")]
    model = [x for x in optimise(model) if x != ("L", f"@{end}")]
    pos, k = {}, 0
    for x in model:
        if isinstance(x, tuple):
            pos[x[1]] = i + 1 + k
        else:
            k += 1
    got = [(o.k, o.s and var_cmp(o.c, var)[:2] or o.k == "J" and (NEGREL[var_cmp(o.c, var)[0]], var_cmp(o.c, var)[1]),
            o.tgt) for o in ops[i + 1:end]]
    want = []
    for x in model:
        if isinstance(x, tuple):
            continue
        t = int(x.tgt[1:]) if x.tgt.startswith("@") else pos[x.tgt]
        want.append((x.k, x.s and var_cmp(x.c, var)[:2] or x.k == "J" and (NEGREL[var_cmp(x.c, var)[0]],
                                                                             var_cmp(x.c, var)[1]), t))
    if got != want:
        return None
    hexa = any(var_cmp(o.c, var)[2].lower().startswith("0x") for o in ops[i + 1:end] if o.k == "J")
    sw = Op("SW", text=expr.strip(), has={t for _, t in cases} | {D})
    sw.cases = [(hex(v) if hexa else str(v), t) for v, t in sorted(cases, key=lambda c: (c[1], c[0]))]
    sw.end, sw.var = D, var
    sw.pre = [c for o in ops[i:end] for c in o.pre]
    return sw


def lower_body(stmts):
    lw = Lower()
    for s in stmts:
        lw.stmt(s)
    lw.label(RET)
    if lw.pending:
        raise Unsupported("trailing comment")
    return Program(optimise(lw.out))


# ------------------------------------------------------------------------------------------ structuring
class Structurer:
    MAXC = 24

    def __init__(self, prog, void, forbid):
        self.p, self.ops, self.void, self.forbid = prog, prog.ops, void, forbid
        self.memo = {}

    def jk(self, k):
        return k < len(self.ops) and self.ops[k].k in ("J", "G")

    # conditions: fj(i, F, t) = cond whose FJ code with false-exit F is exactly ops[i:t] (true-exit t)
    def fj(self, i, F, t):
        key = ("f", i, F, t)
        if key not in self.memo:
            self.memo[key] = None
            self.memo[key] = self._fj(i, F, t)
        return self.memo[key]

    def tj(self, i, T, t):
        key = ("t", i, T, t)
        if key not in self.memo:
            self.memo[key] = None
            self.memo[key] = self._tj(i, T, t)
        return self.memo[key]

    def _fj(self, i, F, t):
        o = self.ops
        if t == i + 1 and o[i].k == "J" and o[i].tgt == F:
            return ("atom", o[i].c if not o[i].s else neg(o[i].c))
        for m in range(i + 1, t):
            x = self.tj(i, t, m)
            if x:
                y = self.fj(m, F, t)
                if y:
                    return ("or", x, y)
        if t - 1 > i and o[t - 1].k == "G" and o[t - 1].tgt == F:
            for m in range(i + 1, t - 1):
                x = self.fj(i, t - 1, m)
                if x:
                    y = self.tj(m, t, t - 1)
                    if y:
                        return ("and", x, y)
        if t > i + 1:
            x = self.tj(i, F, t)
            if x and x[0] != "atom":
                return ("not", x)
        return None

    def _tj(self, i, T, t):
        o = self.ops
        if t == i + 1 and o[i].k == "J" and o[i].tgt == T:
            return ("atom", o[i].c if o[i].s else neg(o[i].c))
        for m in range(i + 1, t):
            x = self.fj(i, t, m)
            if x:
                y = self.tj(m, T, t)
                if y:
                    return ("and", x, y)
        if t - 1 > i and o[t - 1].k == "G" and o[t - 1].tgt == T:
            for m in range(i + 1, t - 1):
                x = self.tj(i, t - 1, m)
                if x:
                    y = self.fj(m, t, t - 1)
                    if y:
                        return ("or", x, y)
        if t > i + 1:
            x = self.fj(i, T, t)
            if x and x[0] != "atom":
                return ("not", x)
        return None

    def cond_run(self, i, j):
        t = i
        while t < j and t - i < self.MAXC and self.jk(t):
            t += 1
        return t

    # statements: seq(i, j) covers ops[i:j]; nodes are dicts with kind, pos, end and sub-blocks
    def seq(self, i, j, ctx):
        out = []
        while i < j:
            node = None
            for f in (self.try_switch, self.try_for, self.try_while, self.try_do, self.try_forever, self.try_if):
                if (f.__name__, i) in self.forbid:
                    continue
                n = f(i, j, ctx)
                if n and (node is None or n["end"] > node["end"]):
                    node = n
            if node is None:
                node = self.raw(i, j, ctx)
            if node["k"] == "for" and out and out[-1]["k"] == "S" and out[-1]["end"] == node["pos"] and \
                    ("init", out[-1]["pos"]) not in self.forbid and "=" in out[-1]["text"] and not node["init"]:
                s = out.pop()
                node.update(init=s["text"], pos=s["pos"], hidden=node["hidden"] | {node["pos"]})
            out.append(node)
            i = node["end"]
        return {"stmts": out, "pos": out[0]["pos"] if out else i, "end": j}

    def blk(self, i, j, ctx):
        b = self.seq(i, j, ctx)
        b["pos"] = i
        return b

    def backjumps(self, i, j):
        return [k for k in range(i, j) if self.ops[k].k in ("J", "G") and self.ops[k].tgt == i]

    def loop_body(self, lo, k, ctx):
        """Loop body ops[lo:k] before the back jump at k.  A conditional back jump is `if (c) break;` at the end of
        the body: its jcc over the back jmp was inverted (`jcc E; jmp top; E:` -> `j!cc top`)."""
        b = self.blk(lo, k, ctx)
        op = self.ops[k]
        if op.k == "J":
            b["stmts"].append(dict(k="jump", c=("atom", neg(op.c) if op.s else op.c), what="break", tgt=ctx["brk"],
                                   pos=k, end=k + 1, hidden=set()))
            b["end"] = None     # no label can sit on the vanished back jmp
        return b

    def try_while(self, i, j, ctx):
        for k in reversed(self.backjumps(i, j)):
            if k == i:
                continue
            e = k + 1
            for t in range(self.cond_run(i, k), i, -1):
                c = self.fj(i, e, t)
                if c:
                    return dict(k="while", pos=i, end=e, c=c, body=self.loop_body(t, k, dict(ctx, brk=e, cont=k)),
                                hidden=set(range(i + 1, t)))
        return None

    def try_forever(self, i, j, ctx):
        for k in reversed(self.backjumps(i, j)):
            if self.ops[k].k == "G" and k > i:
                inner = [x for x in range(i, k) if self.ops[x].tgt == k]
                if inner:
                    return dict(k="while1", pos=i, end=k + 1, body=self.blk(i, k, dict(ctx, brk=k + 1, cont=k)),
                                hidden=set())
                return dict(k="forever", pos=i, end=k + 1, body=self.blk(i, k, dict(ctx, brk=k + 1, cont=i)),
                            hidden=set())
        return None

    def try_do(self, i, j, ctx):
        for k in reversed(self.backjumps(i, j)):
            e = k + 1
            if k == i:
                continue
            for c in range(i + 1, k + 1):
                if all(self.jk(x) for x in range(c, e)) and e - c <= self.MAXC:
                    cond = self.tj(c, i, e)
                    if cond:
                        return dict(k="do", pos=i, end=e, c=cond, body=self.blk(i, c, dict(ctx, brk=e, cont=c)),
                                    hidden=set(range(c + 1, e)))
        return None

    def try_for(self, i, j, ctx):
        o = self.ops
        for t in range(self.cond_run(i, j) - 1, i, -1):     # o[t] = jmp E (inside the J/G run)
            if o[t].k != "G":
                continue
            E = o[t].tgt
            s = t + 1
            while s < j and o[s].k == "S":
                s += 1
            if not (s < j and o[s].k == "G" and o[s].tgt == i and E is not None and s + 1 <= E - 1 < j and
                    o[E - 1].k in ("G", "J") and o[E - 1].tgt == t + 1):
                continue
            c = self.tj(i, s + 1, t)
            if c:
                step = ", ".join(o[x].text for x in range(t + 1, s)) or None
                return dict(k="for", pos=i, end=E, c=c, init=None, step=step,
                            body=self.loop_body(s + 1, E - 1, dict(ctx, brk=E, cont=t + 1)),
                            hidden=set(range(i + 1, s + 1)))
        return None

    def try_switch(self, i, j, ctx):
        op = self.ops[i]
        if op.k != "SW" or not i < op.end <= j:
            return None
        order = [p for _, p in op.cases]
        poss = sorted(set(order))
        if order != sorted(order) or not poss or poss[0] != i + 1 or poss[-1] > op.end:
            return None
        segs = []
        for k, p in enumerate(poss):
            q = poss[k + 1] if k + 1 < len(poss) else op.end
            segs.append(([c for c, x in op.cases if x == p], self.blk(p, q, dict(ctx, brk=op.end))))
        return dict(k="switch", pos=i, end=op.end, text=op.text, segs=segs, hidden=set())

    def try_if(self, i, j, ctx):
        o = self.ops
        if o[i].k != "J":
            return None
        for t in range(self.cond_run(i, j), i, -1):
            outs = {o[x].tgt for x in range(i, t) if not (i < o[x].tgt <= t)}
            if len(outs) != 1:
                continue
            F = outs.pop()
            if not (t < F <= j):
                continue
            c = self.fj(i, F, t)
            if not c:
                continue
            hid = set(range(i + 1, t))
            g = o[F - 1]
            if F - 1 > t and g.k == "G" and F < g.tgt <= j and not (
                    g.tgt == self.p.ret and (o[F - 2].k == "RS" or self.void)):
                return dict(k="if", pos=i, end=g.tgt, c=c, then=self.blk(t, F - 1, ctx),
                            els=self.blk(F, g.tgt, ctx), hidden=hid)
            return dict(k="if", pos=i, end=F, c=c, then=self.blk(t, F, ctx), els=None, hidden=hid)
        return None

    def raw(self, i, j, ctx):
        op = self.ops[i]
        n = dict(pos=i, end=i + 1, hidden=set())
        if op.k == "S":
            return dict(n, k="S", text=op.text)
        if op.k == "K":
            return dict(n, k="K", c=("atom", op.c if op.s else neg(op.c)))
        if op.k == "SW":
            raise Unsupported("switch layout not recognised")
        if op.k == "RS":
            if i + 1 < len(self.ops) and self.ops[i + 1].k == "G" and self.ops[i + 1].tgt == self.p.ret and i + 1 < j:
                return dict(n, k="R", text=op.text, end=i + 2)
            if i + 1 == self.p.ret:
                return dict(n, k="R", text=op.text)
            raise Unsupported("return value without a jump to the epilogue")
        tgt = op.tgt
        if tgt == ctx.get("brk"):
            what = "break"
        elif tgt == ctx.get("cont"):
            what = "continue"
        elif tgt == self.p.ret and self.void:
            what = "return"
        else:
            what = "goto"
        c = None
        if op.k == "J":
            c = ("atom", op.c if op.s else neg(op.c))
        return dict(n, k="jump", c=c, what=what, tgt=tgt)


# ------------------------------------------------------------------------------------------ printing
def children(n):
    return [n[k] for k in ("then", "els", "body") if n.get(k)] + [b for _, b in n.get("segs", [])]


def walk_slots(b, depth, out):
    """Every place a label can be printed: (code position, depth, is-block-end, order, block id, node)."""
    for n in b["stmts"]:
        out.append((n["pos"], depth, 0, len(out), id(b), n))
        for c in children(n):
            walk_slots(c, depth + 1, out)
    if b["end"] is not None:
        out.append((b["end"], depth, 1, len(out), id(b), None))


def hidden_owner(b, p):
    """Innermost node whose hidden positions contain p."""
    for n in b["stmts"]:
        for c in children(n):
            r = hidden_owner(c, p)
            if r:
                return r
        if p in n["hidden"]:
            return n
    return None


def mentions(n, name):
    rx = re.compile(r"\b" + re.escape(name) + r"\b")
    own = [n.get("text"), n.get("init"), n.get("step"), n.get("c") and cond_text(n["c"])]
    return any(t and rx.search(t) for t in own) or any(mentions(x, name) for c in children(n) for x in c["stmts"])


def wrap(b, name, decl):
    """Put `decl` in a block around the smallest statement range that uses `name` (merging overlapping ranges)."""
    idx = [k for k, n in enumerate(b["stmts"]) if mentions(n, name)]
    if not idx:
        idx = [0] if b["stmts"] else []
    if len(idx) == 1:       # only one statement uses it: descend if only one of its blocks does
        inner = [c for c in children(b["stmts"][idx[0]]) if any(mentions(x, name) for x in c["stmts"])]
        own = dict(b["stmts"][idx[0]], then=None, els=None, body=None, segs=[])
        if len(inner) == 1 and not mentions(own, name):
            return wrap(inner[0], name, decl)
    if not idx:
        raise Unsupported(f"nowhere to declare {name}")
    lo, hi = idx[0], idx[-1]
    ws = b.setdefault("wrap", {})
    decls = [decl]
    for k in list(ws):
        a, z, d = ws[k]
        if a <= hi and lo <= z:
            lo, hi, decls = min(a, lo), max(z, hi), d + decls
            del ws[k]
    ws[lo] = (lo, hi, decls)


def jumps(b):
    for n in b["stmts"]:
        if n["k"] == "jump" and n["what"] == "goto":
            yield n["tgt"]
        for c in children(n):
            yield from jumps(c)


class Printer:
    def __init__(self, prog, tree, needed):
        self.prog, self.lines = prog, []
        slots = []
        walk_slots(tree, 0, slots)
        self.at = {}     # (block id, index or 'end') -> list of label/comment lines
        self.missing = []
        self.gen = {}
        for p in sorted(needed):
            cands = [s for s in slots if s[0] == p]
            if not cands:
                self.missing.append(p)
                continue
            best = min(cands, key=lambda s: (s[1], s[2], s[3]))
            self.at.setdefault((best[4], id(best[5]) if best[5] is not None else "end"), []).append(p)

    def name(self, p):
        for n in self.prog.names.get(p, []):
            if not n.startswith(("$", "@")):
                return n
        return self.gen.setdefault(p, f"L_s{len(self.gen) + 1}")

    def marks(self, b, key, ind):
        for p in self.at.get((id(b), key), []):
            for c in self.prog.ops[p].pre if p < len(self.prog.ops) else []:
                self.lines.append("    " * ind + c)
            if p in self.labels:
                self.lines.append(f"{self.name(p)}:;")

    def block(self, b, ind):
        wraps = b.get("wrap", {})
        for k, n in enumerate(b["stmts"]):
            if k in wraps:      # nested-block autos keep their block (they get their own frame slots)
                lo, hi, decls = wraps[k]
                self.lines.append("    " * ind + "{")
                self.lines.extend("    " * (ind + 1) + d + ";" for d in decls)
                ind += 1
            self.marks(b, id(n), ind)
            self.node(n, ind)
            if any(w[1] == k for w in wraps.values()):
                ind -= 1
                self.lines.append("    " * ind + "}")
        self.marks(b, "end", ind)

    def node(self, n, ind):
        pad = "    " * ind
        k = n["k"]
        L = self.lines
        if k == "S":
            L.append(f"{pad}{n['text']}" + ("" if n["text"].endswith("}") else ";"))
        elif k == "R":
            L.append(f"{pad}return {n['text']};")
        elif k == "K":
            L.append(f"{pad}if ({cond_text(n['c'])}) {{")
            L.append(f"{pad}}}")
        elif k == "jump":
            j = n["what"] + ";" if n["what"] != "goto" else f"goto {self.name(n['tgt'])};"
            L.append(f"{pad}if ({cond_text(n['c'])}) {j}" if n["c"] else f"{pad}{j}")
        elif k == "if" and n["els"] is None and len(n["then"]["stmts"]) == 1 and \
                n["then"]["stmts"][0]["k"] == "jump" and not n["then"]["stmts"][0]["c"] and \
                not any(key[0] == id(n["then"]) for key in self.at):     # if (c) goto/break/continue/return;
            j = n["then"]["stmts"][0]
            L.append(f"{pad}if ({cond_text(n['c'])}) " + (j["what"] + ";" if j["what"] != "goto"
                                                            else f"goto {self.name(j['tgt'])};"))
        elif k == "if":
            L.append(f"{pad}if ({cond_text(n['c'])}) {{")
            self.block(n["then"], ind + 1)
            e = n["els"]
            while e is not None:
                only = e["stmts"][0] if len(e["stmts"]) == 1 else None
                if only and only["k"] == "if" and not self.at.get((id(e), id(only))) and \
                        not self.at.get((id(e), "end")) and not e.get("wrap"):
                    L.append(f"{pad}}} else if ({cond_text(only['c'])}) {{")
                    self.block(only["then"], ind + 1)
                    e = only["els"]
                    continue
                L.append(f"{pad}}} else {{")
                self.block(e, ind + 1)
                break
            L.append(f"{pad}}}")
        elif k in ("while", "while1", "forever"):
            head = {"while": lambda: f"while ({cond_text(n['c'])})", "while1": lambda: "while (1)",
                    "forever": lambda: "for (;;)"}[k]()
            L.append(f"{pad}{head} {{")
            self.block(n["body"], ind + 1)
            L.append(f"{pad}}}")
        elif k == "switch":
            L.append(f"{pad}switch ({n['text']}) {{")
            for labels, b in n["segs"]:
                L.extend(f"{pad}case {c}:" if c is not None else f"{pad}default:" for c in labels)
                if not b["stmts"] and not self.at.get((id(b), "end")):
                    L.append(f"{pad}    break;")
                self.block(b, ind + 1)
            L.append(f"{pad}}}")
        elif k == "do":
            L.append(f"{pad}do {{")
            self.block(n["body"], ind + 1)
            L.append(f"{pad}}} while ({cond_text(n['c'])});")
        elif k == "for":
            L.append(f"{pad}for ({n['init'] or ''}; {cond_text(n['c'])}; {n['step'] or ''}) {{")
            self.block(n["body"], ind + 1)
            L.append(f"{pad}}}")


# ------------------------------------------------------------------------------------------ driver
class Function:
    def __init__(self, src, typedefs, head_start, lbrace, rbrace, name):
        self.src, self.name, self.lbrace, self.rbrace = src, name, lbrace, rbrace
        self.head = src[head_start:lbrace]
        self.void = bool(re.match(r"\s*(static\s+)?void\s+[\w\s]*\(", self.head)) and "*" not in self.head.split("(")[0]
        self.body = src[lbrace + 1:rbrace]
        self.gotos = len(re.findall(r"\bgoto\b", self.body))
        self.typedefs = typedefs

    def parse(self):
        toks = tokenize(self.body, self.lbrace + 1)
        p = Parser(toks, self.src, self.typedefs)
        p.t = toks + [("op", "}", self.rbrace, self.rbrace + 1)]
        decls, stmts = p.block_body(top=True)
        decl_text = self.src[self.lbrace + 1:decls[-1][3]].strip("\n") if decls else ""
        self.decl_end = decls[-1][3] if decls else self.lbrace + 1
        names = [n for n, _ in p.hoisted]
        if len(set(names)) < len(names) or any(re.search(rf"\b{n}\b", decl_text) for n in names):
            raise Unsupported("hoisted declaration clashes")
        return decl_text, stmts, p.hoisted

    def restructure(self, simple=True):
        SIMPLIFY[0] = simple
        decl_text, stmts, hoisted = self.parse()
        top = {m[1]: m[0] for m in re.findall(r"^([ \t]*(?:[A-Za-z_]\w*[ \t*]+)+?(\w+)[ \t]*;[ \t]*\n)",
                                               decl_text + "\n", re.M)}
        prog, dropped = recover_switches(lower_body(stmts), {n for n, _ in hoisted} | set(top))
        hoisted = [(n, d) for n, d in hoisted if n not in dropped]
        for n in dropped & set(top):     # a function-level switch temp: the switch allocates it itself
            decl_text = (decl_text + "\n").replace(top[n], "", 1).rstrip("\n")
        sig = prog.signature()
        forbid = set()
        for _ in range(40):
            st = Structurer(prog, self.void, forbid)
            tree = st.seq(0, prog.ret, {})
            tree["pos"] = 0
            needed_goto = set(jumps(tree))
            needed = needed_goto | {p for p, op in enumerate(prog.ops) if op.pre}
            pr = Printer(prog, tree, needed)
            if not pr.missing:
                break
            forbid0 = set(forbid)
            for p in pr.missing:
                owner = hidden_owner(tree, p)
                if owner is None:
                    raise Unsupported(f"no slot for position {p}")
                kind = {"while": "try_while", "do": "try_do", "for": "try_for", "while1": "try_forever",
                        "forever": "try_forever", "if": "try_if", "switch": "try_switch"}[owner["k"]]
                if owner["k"] == "for" and owner.get("init") and p == owner["pos"] + 1:
                    forbid.add(("init", owner["pos"]))
                else:
                    forbid.add((kind, owner["pos"] + (1 if owner["k"] == "for" and owner.get("init") else 0)))
            if forbid == forbid0:
                raise Unsupported(f"no slot for positions {pr.missing}")
        else:
            raise Unsupported("label placement did not converge")
        pr.labels = needed_goto
        for name, decl in hoisted:
            wrap(tree, name, decl)
        pr.block(tree, 1)
        gap = "\n" if decl_text and re.match(r"[ \t]*\r?\n[ \t]*\r?\n", self.src[self.decl_end:]) else ""
        body = "\n" + (decl_text + "\n" + gap if decl_text else "") + "\n".join(pr.lines) + "\n"
        # self-check: the new text must lower to the same jump program
        new = Function.__new__(Function)
        new.src = self.src[:self.lbrace + 1] + body + self.src[self.rbrace:]
        new.lbrace, new.rbrace, new.typedefs, new.void = self.lbrace, self.lbrace + 1 + len(body), self.typedefs, self.void
        new.body = body
        d2, s2, h2 = new.parse()
        sig2 = lower_body(s2).signature()
        if sig2 != sig:
            k = next((x for x in range(min(len(sig), len(sig2))) if sig[x] != sig2[x]), min(len(sig), len(sig2)))
            raise Unsupported(f"model mismatch at op {k}: {sig[k] if k < len(sig) else None} vs "
                              f"{sig2[k] if k < len(sig2) else None}")
        return body


def functions(src):
    toks = tokenize(src)
    typedefs = set(re.findall(r"\btypedef\b[^;{]*?(?:\{[^{}]*\}\s*)?\**\s*(\w+)\s*(?:\[[^\]]*\])?\s*;", src))
    typedefs |= set(re.findall(r"\btypedef\b[^;]*\(\s*\*\s*(\w+)\s*\)", src))
    out, depth, last_end = [], 0, 0
    k = 0
    while k < len(toks):
        t = toks[k]
        if t[0] == "com":
            k += 1
            continue
        if t[1] == "#" and depth == 0:
            nl = src.find("\n", t[2])
            last_end = nl
            while k < len(toks) and toks[k][2] < nl:
                k += 1
            continue
        if t[1] == "{" and depth == 0:
            prev = next(x for x in reversed(toks[:k]) if x[0] != "com")
            d, m = 1, k + 1
            while d:
                d += toks[m][1] == "{" and toks[m][0] == "op"
                d -= toks[m][1] == "}" and toks[m][0] == "op"
                m += 1
            if prev[1] == ")":
                head = src[last_end:t[2]]
                name = re.findall(r"(\w+)\s*\(", head)
                hs = last_end + len(head) - len(head.lstrip())
                out.append(Function(src, typedefs, hs, t[2], toks[m - 1][2], name[0] if name else "?"))
            k = m
            last_end = toks[m - 1][3]
            continue
        if t[1] == ";" and depth == 0:
            last_end = t[3]
        k += 1
    return out


def unit_record(path):
    man = json.loads((ROOT / "manifest.json").read_text())
    rel = Path(path).resolve()
    for u in man["units"]:
        if (ROOT / u["src"]).resolve().name == rel.name:
            return u
    raise SystemExit(f"no manifest unit for {path}")


def gate(text, unit, work):
    """tools/check.py on the whole unit -> True if EXACT."""
    f = work / Path(unit["src"]).name
    f.write_text(text, newline="\n")
    args = [sys.executable, str(ROOT / "tools" / "check.py"), str(f), "--all", "--at", unit["start"], "--end",
            unit["end"], "--profile", unit.get("profile", "game-c"), "--json", str(work / "gate.json")]
    for pl in unit.get("place") or []:
        args += ["--place", pl]
    r = subprocess.run(args, capture_output=True, text=True)
    return r.returncode == 0 and r.stdout.startswith("EXACT"), r.stdout.strip().splitlines()[0] if r.stdout else r.stderr[-300:]


def apply(src, funcs, bodies):
    out, last = [], 0
    for f in funcs:
        if f.name in bodies:
            out += [src[last:f.lbrace + 1], bodies[f.name]]
            last = f.rbrace
    out.append(src[last:])
    return "".join(out)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source")
    ap.add_argument("--out", required=True)
    ap.add_argument("--func", action="append", help="only these functions (default: every function with a goto)")
    ap.add_argument("--no-verify", action="store_true", help="skip the check.py gate (model check only)")
    ap.add_argument("--all-funcs", action="store_true", help="also rewrite functions without gotos")
    ap.add_argument("--log")
    a = ap.parse_args(argv[1:])
    src = Path(a.source).read_text()
    funcs = functions(src)
    unit = None if a.no_verify else unit_record(a.source)
    log = []
    bodies = {}
    for f in funcs:
        if a.func and f.name not in a.func:
            continue
        if not a.func and not a.all_funcs and not f.gotos and not re.search(r"^\s*\w+:;", f.body, re.M):
            continue
        try:
            bodies[f.name] = (f.restructure(True), f.restructure(False))
            log.append((f.name, "model-ok", ""))
        except Unsupported as e:
            log.append((f.name, "skip", str(e)))
        except RecursionError:
            log.append((f.name, "skip", "recursion"))
    kept = {n: b[0] for n, b in bodies.items()}
    if unit and bodies:
        work = Path(tempfile.mkdtemp(prefix="struct-", dir=ROOT / "build" / "tmp"))
        ok, line = gate(src, unit, work)
        if not ok:
            raise SystemExit(f"original unit is not EXACT: {line}")
        ok, line = gate(apply(src, funcs, kept), unit, work)
        if not ok:
            kept = {}
            for name, variants in bodies.items():
                for body in dict.fromkeys(variants):   # simplified conditions first, then the literal ones
                    good, line = gate(apply(src, funcs, {**kept, name: body}), unit, work)
                    if good:
                        kept[name] = body
                        break
                else:
                    log = [(n, "gate-diff", line) if n == name else (n, s, m) for n, s, m in log]
            ok, line = gate(apply(src, funcs, kept), unit, work)
            assert ok, line
    out = apply(src, funcs, kept)
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    Path(a.out).write_text(out, newline="\n")
    before = sum(f.gotos for f in funcs)
    after = len(re.findall(r"\bgoto\b", out)) - len(re.findall(r"\bgoto\b", "".join(
        c for c in re.findall(r"/\*.*?\*/|//[^\n]*", out, re.S))))
    for n, s, m in log:
        if s == "model-ok" and n not in kept:
            s = "gate-diff"
        print(f"  {n:32} {s:9} {m}")
    print(f"{Path(a.source).name}: gotos {before} -> {after}; {len(kept)}/{len(log)} functions rewritten"
          + ("" if unit else " (not gate-verified)"))
    if a.log:
        Path(a.log).write_text(json.dumps({"unit": a.source, "before": before, "after": after, "functions": log},
                                          indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
