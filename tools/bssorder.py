"""Predict / solve Watcom C 10.0 (wcc386) _BSS layout order.

Rule (PROVEN by probes, build/workers/bss/; see docs/compiler-notes.md "_BSS order"):
  every uninitialised file-scope object (public or static) gets an index = the number of cfe
  symbol-table entries created before its FIRST declaration (extern or definition).  Objects are laid
  out without padding, sorted by
      (1) size rank, descending:  size%8==0 > size%4==0 > size%2==0 (size>2) > size==2 > odd (>1) > size==1
      (2) group, descending:      group = 0 if idx < 5 else 1 + (idx-5)//25
      (3) key, ascending:         key = hashpjw(name) % 241   (cfe identifier hash, case-sensitive, all chars)
      (4) idx, descending         (same group + same bucket: later declaration first)
  Symbol entries: each new ordinary identifier (variable, function, typedef, implicit function decl),
  each parameter of a function DEFINITION, each block-scope declaration, one extra entry for every
  non-void function definition, each distinct string literal.  Not counted: enum constants, tags,
  members, labels, macros, prototype parameter names, numeric constants.

Usage
  python tools/bssorder.py [--first N] NAME[:SIZE][@IDX] ...
        layout of these objects declared consecutively (idx = --first + position unless @IDX given)
  python tools/bssorder.py --solve [--chars CS] [--maxlen L] SLOT ...
        SLOT = PATTERN[:SIZE][@IDX] listed in the REQUIRED address order; PATTERN = alternatives
        separated by '|', '{}' = free suffix over CS (default 0-9a-z) of length 1..L (default 2).
        Prints the most preferred names (listed alternatives first, then shorter suffixes).
  python tools/bssorder.py --src FILE.c [--flags "-3s -d2 -s"]
        compile FILE.c, measure each _BSS object's symbol index (prefix probes), print actual vs
        predicted layout.
  python tools/bssorder.py --src FILE.c --want A,B,C [--rename A=PATTERN ...] [--verify]
        solve names for FILE.c's _BSS objects so that they lie in the address order A,B,C,...
        (objects not given --rename keep their names); --verify compiles the renamed TU.
"""
from __future__ import annotations

import argparse
import itertools
import re
import string
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

HASH_SIZE = 241
FIRST = 5      # size of the first index group
PER = 25       # size of every later group


def pjw(name: str) -> int:
    """cfe identifier hash (before % 241)."""
    b = name.encode("latin1")
    if not b:
        return 0
    h = b[0]
    if len(b) < 2:
        return h
    h = (h << 4) + b[1]
    i = 2
    while True:
        h &= 0xFFF
        if i >= len(b):
            break
        h = (h << 4) + b[i]
        i += 1
        h = (h ^ (h >> 12)) & 0xFFF
        if i >= len(b):
            break
        h = (h << 4) + b[i]
        i += 1
        h = h ^ (h >> 12)
    return h


def key(name: str) -> int:
    return pjw(name) % HASH_SIZE


def rank(size: int) -> int:
    if size % 8 == 0:
        return 5
    if size % 4 == 0:
        return 4
    if size % 2 == 0:
        return 3 if size > 2 else 2
    return 1 if size > 1 else 0


def group(idx: int) -> int:
    return 0 if idx < FIRST else 1 + (idx - FIRST) // PER


def sortkey(name, size, idx):
    return (-rank(size), -group(idx), key(name), -idx)


def layout(objs):
    """objs: [(name, size, idx)] -> [(off, name, size, idx)] in address order."""
    out, off = [], 0
    for n, s, i in sorted(objs, key=lambda o: sortkey(*o)):
        out.append((off, n, s, i))
        off += s
    return out


def before(a, b):
    """True if object a=(name,size,idx) is placed before b."""
    return sortkey(*a) < sortkey(*b)


# ----------------------------------------------------------------------------------------- solve

def expand(pattern, chars, maxlen):
    """Yield (tier, name): plain alternatives first (one tier each, in the given order), then free
    suffixes by length.  The solver takes the smallest usable key inside the best tier."""
    alts = pattern.split("|")
    t = 0
    for a in alts:
        if "{}" not in a:
            yield t, a
            t += 1
    for L in range(1, maxlen + 1):
        for a in alts:
            if "{}" in a:
                for tup in itertools.product(chars, repeat=L):
                    yield t, a.replace("{}", "".join(tup))
        t += 1


def solve(slots, chars="0123456789abcdefghijklmnopqrstuvwxyz", maxlen=2, taken=()):
    """slots: [(pattern, size, idx)] in required address order. Returns [names] or raises."""
    # objects of different (rank, group) are ordered by those alone: check feasibility first
    cls = [(-rank(s), -group(i)) for _, s, i in slots]
    for k in range(len(slots) - 1):
        if cls[k] > cls[k + 1]:
            raise ValueError(f"impossible regardless of names: {slots[k][0]} (size {slots[k][1]}, idx {slots[k][2]}) "
                             f"must precede {slots[k+1][0]} (size {slots[k+1][1]}, idx {slots[k+1][2]}) but its "
                             f"size rank/index group sorts it later; change sizes or declaration order")
    cands = []
    used = set(taken)
    for p, s, i in slots:
        seen = set()
        c = []
        for t, nm in expand(p, chars, maxlen):
            if nm not in used and nm not in seen:
                seen.add(nm)
                c.append((t, nm))
        if not c:
            raise ValueError(f"no candidates for {p}")
        cands.append(c)
    n = len(slots)

    def ok_pair(ka, ia, kb, ib):          # a before b within the same class
        return ka < kb or (ka == kb and ia > ib)

    # backward: best[k] = largest key usable at slot k with a feasible suffix (per class run)
    INF = 10 ** 9
    best = [None] * n
    for k in range(n - 1, -1, -1):
        same_next = k + 1 < n and cls[k] == cls[k + 1]
        feas = [key(c) for _, c in cands[k]
                if not same_next or ok_pair(key(c), slots[k][2], best[k + 1], slots[k + 1][2])]
        if not feas:
            raise ValueError(f"no name for slot {slots[k][0]} fits before the following slots")
        best[k] = max(feas)
    names, prev = [], None
    chosen = set()
    for k in range(n):
        same_prev = k > 0 and cls[k] == cls[k - 1]
        same_next = k + 1 < n and cls[k] == cls[k + 1]
        fits = []
        for t, c in cands[k]:
            if c in chosen:
                continue
            kc = key(c)
            if same_prev and not ok_pair(prev, slots[k - 1][2], kc, slots[k][2]):
                continue
            if same_next and not ok_pair(kc, slots[k][2], best[k + 1], slots[k + 1][2]):
                continue
            fits.append((t, kc, c))
        if not fits:
            raise ValueError(f"greedy failed at slot {slots[k][0]}")
        t, kc, c = min(fits)
        names.append(c)
        chosen.add(c)
        prev = kc
    return names


# ----------------------------------------------------------------------------------------- source

def toplevel_chunks(src: str):
    """Split C source into top-level chunks. Returns [(start, end, kind)] with kind in
    'pp' (preprocessor line), 'decl' (ends with ';'), 'func' (function definition)."""
    i, n = 0, len(src)
    chunks = []
    start = 0
    depth = 0
    last_sig = ""
    func_body = False
    at_line_start = True
    while i < n:
        c = src[i]
        if c == "\n":
            at_line_start = True
            i += 1
            continue
        if c in " \t\r\f\v":
            i += 1
            continue
        if at_line_start and c == "#" and depth == 0:
            # preprocessor line (with continuations)
            j = i
            while j < n:
                e = src.find("\n", j)
                if e < 0:
                    e = n
                if src[e - 1:e] == "\\":
                    j = e + 1
                    continue
                break
            if not strip_comments(src[start:i]).strip():
                chunks.append((start, e, "pp"))
                start = e
            i = e
            continue
        at_line_start = False
        if src.startswith("/*", i):
            e = src.find("*/", i + 2)
            i = n if e < 0 else e + 2
            continue
        if src.startswith("//", i):
            e = src.find("\n", i)
            i = n if e < 0 else e
            continue
        if c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            i = j + 1
            last_sig = c
            continue
        if c in "([{":
            if c == "{" and depth == 0:
                func_body = last_sig == ")" or re.search(r"\)\s*$", re.sub(r"/\*.*?\*/", "", src[start:i], flags=re.S)) is not None
            depth += 1
        elif c in ")]}":
            depth -= 1
            if c == "}" and depth == 0 and func_body:
                chunks.append((start, i + 1, "func"))
                start = i + 1
                func_body = False
                last_sig = c
                i += 1
                continue
        elif c == ";" and depth == 0:
            chunks.append((start, i + 1, "decl"))
            start = i + 1
        last_sig = c
        i += 1
    return chunks


def strip_comments(s):
    return re.sub(r"/\*.*?\*/|//[^\n]*", " ", s, flags=re.S)


STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"(?:\s*"(?:\\.|[^"\\])*")*')


def estimate_func_entries(text, seen_strings, seen_names):
    """Rough count of symbol entries added by a function definition (only used to resolve the
    mod-25 ambiguity of the probe measurement)."""
    t = strip_comments(text)
    brace = t.index("{")
    head, body = t[:brace], t[brace:]
    m = re.search(r"([A-Za-z_]\w*)\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*$", head.strip())
    cnt = 0
    if m:
        if m.group(1) not in seen_names:
            cnt += 1
            seen_names.add(m.group(1))
        params = m.group(2).strip()
        if params and params != "void":
            cnt += params.count(",") + 1
        ret = head[:m.start(1)]
        if not re.fullmatch(r"\s*(extern\s+|static\s+)?void\s*(__\w+\s*)*", ret):
            cnt += 1
    for s in STRING_RE.findall(body):
        if s not in seen_strings:
            seen_strings.add(s)
            cnt += 1
    types = r"(?:unsigned|signed|int|char|short|long|float|double|void|struct|union|enum|static|register|extern|const|volatile)"
    for stmt in re.findall(r"(?<=[{;}])\s*(" + types + r"\b[^;{}()]*(?:\([^;{}]*\))?[^;{}]*);", body):
        cnt += 1 + stmt.count(",")
    return cnt


class Prober:
    def __init__(self, flags):
        from dosrun import run
        self.run = run
        self.flags = flags
        self.dir = Path(tempfile.mkdtemp(prefix="bssorder_"))
        self.n = 0

    def compile(self, text):
        import omf
        self.n += 1
        name = f"p{self.n}"
        (self.dir / f"{name}.c").write_text(text, encoding="latin1")
        r = self.run("wcc386", list(self.flags) + [f"{name}.c"], cwd=self.dir)
        if r.rc:
            raise RuntimeError(r.out)
        return omf.load(self.dir / f"{name}.obj")[0]

    def count_mod(self, prefix, pnames):
        """(number of symbol entries created by `prefix`) mod 25, by appending int probes with
        strictly increasing keys (one group boundary becomes visible)."""
        text = prefix + "\n" + "".join(f"int {p};\n" for p in pnames)
        m = self.compile(text)
        pos = bss_order(m)
        o = [n for _, n, _ in pos if n in pnames]
        # probes are ascending in key; each new ascending run in the output is an earlier group
        runs = [[o[0]]]
        for x in o[1:]:
            if pnames.index(x) > pnames.index(runs[-1][-1]):
                runs[-1].append(x)
            else:
                runs.append([x])
        runs.reverse()   # definition order
        if len(runs) == 1:
            raise RuntimeError("no group boundary seen")
        first = len(runs[0])        # probes before the first boundary
        # boundary indices are 5 + 25k: count + first == 5 (mod 25)
        return (FIRST - first) % PER


def bss_order(m):
    bi = [s for s in range(1, len(m.segments)) if m.segments[s] and m.segments[s].name == "_BSS"]
    if not bi:
        return []
    bi = bi[0]
    size = m.segments[bi].size
    pubs = sorted((o, n) for n, s, o, _ in m.publics if s == bi)
    out = []
    for k, (o, n) in enumerate(pubs):
        out.append((o, n, (pubs[k + 1][0] if k + 1 < len(pubs) else size) - o))
    return out


def probe_names(src, n=53):
    """n unused identifiers with distinct keys, in ascending key order."""
    for prefix in ("bssprobe", "bssprobe_q", "bssprobe_zz"):
        byk = {}
        for k in range(5000):
            nm = f"{prefix}{k}"
            byk.setdefault(key(nm), nm)
            if len(byk) >= n:
                break
        names = [byk[k] for k in sorted(byk)][:n]
        if not any(re.search(r"%s" % re.escape(p), src) for p in names):
            return names
    raise RuntimeError("cannot find unused probe names")


def measure(path, flags, verbose=False):
    """Compile the TU, return [(off, name, size, idx)] of its _BSS publics with measured idx."""
    src = Path(path).read_text(encoding="latin1")
    pr = Prober(flags)
    import shutil
    m = pr.compile(src)
    actual = bss_order(m)
    names = [n for _, n, _ in actual]
    if not names:
        return [], pr
    chunks = toplevel_chunks(src)
    ident = re.compile(r"[A-Za-z_]\w*")
    # first declaring chunk of each object (+ its declarator position inside the chunk)
    first = {}
    for ci, (s, e, kind) in enumerate(chunks):
        if kind != "decl":
            continue
        text = strip_comments(src[s:e])
        text = re.sub(r"\{[^{}]*\}", "{}", text)       # struct bodies / initialisers
        text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
        decls = [d for d in re.split(r",(?![^()]*\))", text.rstrip(";"))]
        for pos, d in enumerate(decls):
            d0 = re.sub(r"\[[^\]]*\]|=.*", "", d, flags=re.S)
            ids = ident.findall(d0)
            if ids and ids[-1] in names and ids[-1] not in first:
                first[ids[-1]] = (ci, pos)
    missing = [n for n in names if n not in first]
    if missing:
        raise RuntimeError(f"cannot find the declarations of {missing}")
    pnames = probe_names(src)
    need = sorted({ci for ci, _ in first.values()})
    lo, hi = need[0], need[-1]
    # measure count mod 25 at every chunk boundary in [lo, hi] and unwrap with estimates
    seen_s, seen_n = set(), set()
    for s, e, kind in chunks[:lo]:
        seen_s.update(STRING_RE.findall(strip_comments(src[s:e])))
    absolute = {}
    prev_mod = pr.count_mod(src[:chunks[lo][0]], pnames)
    cur = prev_mod
    absolute[lo] = cur
    for ci in range(lo, hi):
        s, e, kind = chunks[ci]
        mod = pr.count_mod(src[:e], pnames)
        d = (mod - prev_mod) % PER
        if kind == "func":
            est = estimate_func_entries(src[s:e], seen_s, seen_n)
            while d + PER <= est + PER // 2:
                d += PER
        else:
            seen_s.update(STRING_RE.findall(strip_comments(src[s:e])))
        cur += d
        prev_mod = mod
        absolute[ci + 1] = cur
        if verbose:
            print(f"  chunk {ci} {kind:4} +{d:<3} -> {cur}", file=sys.stderr)
    res = []
    for off, n, size in actual:
        ci, pos = first[n]
        res.append((off, n, size, absolute[ci] + pos))
    return res, pr


# ----------------------------------------------------------------------------------------- CLI

def parse_obj(spec, default_idx):
    m = re.fullmatch(r"(.+?)(?::(\d+))?(?:@(\d+))?", spec)
    name, size, idx = m.group(1), int(m.group(2) or 4), m.group(3)
    return name, size, int(idx) if idx is not None else default_idx


def print_layout(rows, actual=None):
    print(f"{'off':>5} {'size':>5} {'rank':>4} {'grp':>3} {'key':>3} {'idx':>4}  name")
    for k, (off, n, s, i) in enumerate(rows):
        mark = ""
        if actual is not None:
            mark = "" if actual[k][1] == n else f"   <-- actual: {actual[k][1]}"
        print(f"{off:>5x} {s:>5} {rank(s):>4} {group(i):>3} {key(n):>3} {i:>4}  {n}{mark}")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("objs", nargs="*")
    ap.add_argument("--first", type=int, default=0, help="symbol entries before the first object")
    ap.add_argument("--solve", action="store_true")
    ap.add_argument("--chars", default="0123456789abcdefghijklmnopqrstuvwxyz")
    ap.add_argument("--maxlen", type=int, default=2)
    ap.add_argument("--src")
    ap.add_argument("--flags", default="-3s -d2 -s")
    ap.add_argument("--want", help="required address order (comma separated current names)")
    ap.add_argument("--rename", action="append", default=[], metavar="NAME=PATTERN")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args(argv)

    if a.src:
        rows, pr = measure(a.src, a.flags.split(), a.verbose)
        objs = [(n, s, i) for _, n, s, i in rows]
        pred = layout(objs)
        print(f"{a.src}: actual _BSS layout (idx measured)")
        print_layout(rows)
        same = [p[1] for p in pred] == [r[1] for r in rows]
        print("prediction:", "MATCHES the compiler" if same else "DIFFERS")
        if not same:
            print_layout(pred, rows)
        if a.want:
            want = a.want.split(",")
            info = {n: (s, i) for n, s, i in objs}
            if sorted(want) != sorted(info):
                sys.exit(f"--want must list exactly the _BSS objects: {sorted(info)}")
            ren = dict(r.split("=", 1) for r in a.rename)
            slots = [(ren.get(n, n), *info[n]) for n in want]
            src_text = Path(a.src).read_text(encoding="latin1")
            taken = set(re.findall(r"[A-Za-z_]\w*", src_text)) - set(want)
            names = solve(slots, a.chars, a.maxlen, taken)
            print("\nsolution (address order):")
            for n, new in zip(want, names):
                print(f"  {n:20} -> {new:20} key {key(new)}")
            if a.verify:
                new_src = src_text
                mp = {n: new for n, new in zip(want, names) if n != new}
                if mp:
                    new_src = re.sub(r"\b(" + "|".join(map(re.escape, mp)) + r")\b",
                                     lambda m: mp[m.group(1)], src_text)
                got = [n for _, n, _ in bss_order(pr.compile(new_src))]
                print("verify:", "OK" if got == names else f"FAILED, compiler gives {got}")
        return
    if a.solve:
        slots = [parse_obj(s, a.first + k) for k, s in enumerate(a.objs)]
        names = solve(slots, a.chars, a.maxlen)
        objs = [(nm, s, i) for nm, (_, s, i) in zip(names, slots)]
        rows = layout(objs)
        print_layout(rows)
        assert [r[1] for r in rows] == names
        return
    objs = [parse_obj(s, a.first + k) for k, s in enumerate(a.objs)]
    print_layout(layout(objs))


if __name__ == "__main__":
    main()
