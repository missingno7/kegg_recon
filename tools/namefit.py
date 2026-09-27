"""Which global renames are chunking-neutral?  Fit readable names to the constraints instead of hiding them.

wcc386 cuts a code LEDATA when the pending EXTDEF names of a TU reach 192 bytes (docs/compiler-notes.md), so the
LENGTH of every symbol a C unit imports shapes that unit's object.  A rename that keeps the length is always
chunking-neutral; a symbol no other wcc386 unit imports is free.  Symbols mentioned in the byte-sensitive sources
(host-pinned profiles, T06/T08: their stale source-buffer padding contains source text) cannot be renamed at all.
_BSS order (hashpjw) is not checked here: check.py --all on the owning unit catches it (tools/bssorder.py to solve).

    python tools/namefit.py PLAN.json            # audit the plan's "renames" (plan units replace canonical sources)
    python tools/namefit.py OLD=NEW ...          # audit single renames against the canonical tree
    python tools/namefit.py --suggest NEW LEN    # equal-length spellings of a snake_case name
    python tools/namefit.py PLAN.json --compile  # recompile every importing C unit with the renames applied and
                                                 # name the rename that first changes its object layout

The length rule is sufficient, not necessary: most TUs never reach the 192-byte flush, so --compile is the real
pre-check (fast, parallel); `replan.py --sandbox` stays the gate.

Status per rename: FREE (no other C importer), OK (same length), LEN+d / LEN-d (importers listed; change the
spelling to the old length), FROZEN (named in a byte-sensitive source; keep the old name).
"""
from __future__ import annotations

import itertools
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

ABBR = {
    "number": "num", "count": "cnt", "index": "idx", "buffer": "buf", "pointer": "ptr", "palette": "pal",
    "sprite": "spr", "player": "plr", "position": "pos", "length": "len", "address": "addr", "previous": "prev",
    "current": "cur", "temporary": "tmp", "character": "chr", "string": "str", "screen": "scr", "counter": "ctr",
    "value": "val", "table": "tbl", "message": "msg", "maximum": "max", "minimum": "min", "source": "src",
    "destination": "dst", "offset": "ofs", "segment": "seg", "control": "ctl", "handler": "hdlr", "interrupt": "irq",
    "keyboard": "kbd", "initialize": "init", "initialise": "init", "graphics": "gfx", "animation": "anim",
    "background": "bg", "foreground": "fg", "sound": "snd", "sample": "smp", "frame": "frm", "level": "lvl",
    "bitmap": "bmp", "button": "btn", "config": "cfg", "configuration": "config", "directory": "dir",
    "error": "err", "file": "f", "image": "img", "memory": "mem", "vertical": "vert", "horizontal": "horz",
    "velocity": "vel", "enemy": "enm", "monster": "mon", "bonus": "bon", "status": "stat", "update": "upd",
}
LONG = {v: k for k, v in ABBR.items()}


def profiles():
    return json.loads((ROOT / "toolchain" / "toolchain.json").read_text())["profiles"]


def c_units(plan):
    """{unit id: (source text, is_frozen)} for every wcc386 unit, plan sources replacing canonical ones"""
    man = json.loads((ROOT / "manifest.json").read_text())
    pf = profiles()
    over = {u["id"]: ROOT / u["file"] for u in (plan or {}).get("units", [])}
    out = {}
    for u in man["units"]:
        p = pf.get(u.get("profile", "game-c"), {})
        if p.get("tool", "wcc386") != "wcc386":
            continue
        path = over.get(u["id"], ROOT / u["src"])
        out[u["id"]] = (path.read_text(encoding="latin-1"), bool(p.get("host")))
    return out


def defines(text, name):
    """does this TU define NAME at file scope (function body or non-extern object)?"""
    for line in text.splitlines():
        if re.match(r"\s*extern\b", line) or not re.search(r"\b%s\b" % re.escape(name), line):
            continue
        if line[:1].isspace() or line.lstrip().startswith(("#", "/*", "//")):
            continue
        if re.search(r"\b%s\s*\(" % re.escape(name), line) and not line.rstrip().endswith(";"):
            return True                                              # function definition header
        if re.search(r"\b%s\b\s*(\[[^]]*\]\s*)*(=|;|,)" % re.escape(name), line):
            return True                                              # object definition
    return False


def audit(renames, plan=None):
    units = c_units(plan)
    bad = 0
    for old, new in renames.items():
        rx = re.compile(r"\b%s\b" % re.escape(old))
        users = [uid for uid, (t, _) in units.items() if rx.search(t)]
        frozen = [uid for uid in users if units[uid][1]]
        importers = [uid for uid in users if not defines(units[uid][0], old)]
        d = len(new) - len(old)
        if frozen:
            st = "FROZEN"
        elif not importers:
            st = "FREE"
        elif d == 0:
            st = "OK"
        else:
            st = f"LEN{d:+d}"
        bad += st not in ("FREE", "OK")
        extra = f"  importers: {','.join(importers)}" if st.startswith("LEN") else \
            f"  in: {','.join(frozen)}" if frozen else ""
        print(f"{st:7} {old} ({len(old)}) -> {new} ({len(new)}){extra}")
    print(f"{len(renames)} renames, {bad} need attention")
    return 1 if bad else 0


def layout(obj):
    """object-layout signature: LEDATA chunks and fixup positions in record order (names excluded)"""
    import omf
    m = omf.load(obj)
    m = m[0] if isinstance(m, list) else m
    dbg = lambda si: m.segments[si].cls.startswith("DEB")      # -d2 records: names inside, ignored by WLINK
    return ([(m.segments[si].name, off, n) for si, off, n in m.chunks if not dbg(si)],
            [(m.segments[f.seg].name, f.off, f.loc) for f in m.fixups if not dbg(f.seg)])


def compile_layout(text, profile, tag):
    import check
    d = ROOT / "build" / "namefit" / tag
    d.mkdir(parents=True, exist_ok=True)
    src = d / "unit.c"
    src.write_bytes(text.encode("latin-1"))
    obj, _ = check.compile_candidate(src, profile, d)
    return layout(obj)


def sub(text, renames):
    if not renames:
        return text
    rx = re.compile(r"\b(" + "|".join(map(re.escape, sorted(renames, key=len, reverse=True))) + r")\b")
    return rx.sub(lambda m: renames[m.group(1)], text)


def compile_audit(renames, plan):
    """per importing unit: does the renamed TU keep its object layout?  bisect the culprit if not."""
    from concurrent.futures import ThreadPoolExecutor
    sys.path.insert(0, str(ROOT / "tools"))
    man = json.loads((ROOT / "manifest.json").read_text())
    prof = {u["id"]: u.get("profile", "game-c") for u in man["units"]}
    units = c_units(plan)
    own = {u["id"] for u in (plan or {}).get("units", [])}
    frozen_names = {o for o in renames if any(f and re.search(r"\b%s\b" % re.escape(o), t) for t, f in units.values())}
    for o in sorted(frozen_names):
        print(f"FROZEN  {o}: named in a byte-sensitive source; keep it (a `#pragma aux {renames[o]} \"{o}\"` alias is allowed)")
    renames = {o: n for o, n in renames.items() if o not in frozen_names}
    jobs = []
    for uid, (text, frozen) in units.items():
        rel = {o: n for o, n in renames.items() if re.search(r"\b%s\b" % re.escape(o), text) and not defines(text, o)}
        if rel and not frozen:
            jobs.append((uid, text, rel))

    def one(job):
        uid, text, rel = job
        base = compile_layout(text, prof[uid], f"{uid}-base")
        if compile_layout(sub(text, rel), prof[uid], f"{uid}-all") == base:
            return uid, None, rel
        order = sorted(rel, key=lambda o: re.search(r"\b%s\b" % re.escape(o), text).start())
        lo, hi = 0, len(order)          # smallest prefix that changes the layout
        while hi - lo > 1:
            mid = (lo + hi) // 2
            same = compile_layout(sub(text, {o: rel[o] for o in order[:mid]}), prof[uid], f"{uid}-b") == base
            lo, hi = (mid, hi) if same else (lo, mid)
        return uid, order[hi - 1], rel

    bad = 0
    with ThreadPoolExecutor(6) as ex:
        for uid, culprit, rel in ex.map(one, jobs):
            if culprit is None:
                print(f"OK      {uid}: {len(rel)} imported renames keep the layout")
            else:
                bad += 1
                d = len(rel[culprit]) - len(culprit)
                print(f"LAYOUT  {uid}: first break at {culprit} -> {rel[culprit]} ({d:+d} chars); "
                      f"shorten/lengthen it or earlier imported names of {uid}{' (own plan unit)' if uid in own else ''}")
    print(f"{len(jobs)} importing units compiled, {bad} change layout")
    return 1 if bad else 0


def variants(name):
    """spellings of a snake_case name: per-word long/short forms, optional dropped vowels, plural toggle"""
    words = name.split("_")
    opts = []
    for w in words:
        s = {w}
        if w in ABBR:
            s.add(ABBR[w])
        if w in LONG:
            s.add(LONG[w])
        if len(w) > 3:
            s.add(w[0] + re.sub(r"[aeiou]", "", w[1:]))
        s.add(w + "s" if not w.endswith("s") else w[:-1])
        opts.append(sorted(s, key=lambda x: (x != w, len(x))))
    for combo in itertools.islice(itertools.product(*opts), 200000):
        yield "_".join(combo)


def suggest(name, n):
    seen, out = set(), []
    for v in variants(name):
        if len(v) == n and v not in seen:
            seen.add(v)
            out.append(v)
    # also: drop or add one short connective word
    for extra in ("of", "the", "all", "cur", "num", "tbl", "ptr", "buf"):
        for v in (f"{extra}_{name}", f"{name}_{extra}"):
            if len(v) == n and v not in seen:
                seen.add(v)
                out.append(v)
    print("\n".join(out[:25]) if out else f"no equal-length spelling of {name} ({len(name)}) at {n}; rephrase")


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    if argv[0] == "--suggest":
        suggest(argv[1], int(argv[2]))
        return 0
    if argv[0].endswith(".json"):
        plan = json.loads(Path(argv[0]).read_text())
        if "--compile" in argv:
            return compile_audit(plan.get("renames", {}), plan)
        return audit(plan.get("renames", {}), plan)
    pairs = dict(a.split("=", 1) for a in argv if "=" in a)
    return compile_audit(pairs, None) if "--compile" in argv else audit(pairs)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
