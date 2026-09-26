"""Context packets for matching work: everything a worker needs about a function, in one file.

    python tools/context.py f_46e7                      # print packet
    python tools/context.py f_46e7 f_4602 --out DIR     # write DIR/<name>.ctx.txt for each
    python tools/context.py --range 0x9640 0xc886 --out DIR   # every non-matching function in the range

Packet: manifest entry, annotated disassembly, declarations already proven in src/ for every symbol the
function references (types are the main lever for exact matches), how matched callers call it, the
current best draft, and the library functions it calls.
"""
from __future__ import annotations

import io
import json
import re
import struct
import sys
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import show  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


def top_level_decls(text):
    """Top-level declarations (externs, prototypes, typedefs, struct defs) of a C file; bodies dropped."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = "\n".join(ln for ln in text.splitlines() if not ln.lstrip().startswith("#"))
    out, cur, depth, i = [], [], 0, 0
    while i < len(text):
        ch = text[i]
        if ch == "{":
            # function body if the statement so far looks like a definition header
            head = "".join(cur).strip()
            if depth == 0 and head.endswith(")"):
                d = 1; i += 1
                while i < len(text) and d:
                    d += {"{": 1, "}": -1}.get(text[i], 0); i += 1
                cur = []
                continue
            depth += 1
        elif ch == "}":
            depth -= 1
        cur.append(ch)
        if ch == ";" and depth == 0:
            stmt = " ".join("".join(cur).split())
            if stmt != ";":
                out.append(stmt)
            cur = []
        i += 1
    return out


def load_decls():
    """symbol -> set of top-level declarations found in canonical sources."""
    decls = {}
    for src in sorted((ROOT / "src").glob("*.c")):
        for stmt in top_level_decls(src.read_text(errors="replace")):
            for name in set(re.findall(r"([A-Za-z_]\w*)", stmt)):
                decls.setdefault(name, set()).add(stmt)
    return decls


def symbols_in(dis_text):
    return sorted(set(re.findall(r"\b([fg]_[0-9a-f]+)\b", dis_text)) |
                  set(re.findall(r"fix->3:([0-9a-f]+)", dis_text) and
                      ["g_" + x for x in re.findall(r"fix->3:([0-9a-f]+)", dis_text)]))


def packet(name, man, decls, harvest):
    fn = next(f for f in man["functions"] if f["name"] == name)
    buf = io.StringIO()
    with redirect_stdout(buf):
        show.main([None, fn["start"], fn["end"]])
    dis = buf.getvalue()
    out = [f"# {name}  {fn['start']}..{fn['end']}  kind={fn['kind']} profile={fn.get('profile')} status={fn['status']}"]
    if fn.get("note"):
        out.append(f"# note: {fn['note']}")
    out.append(dis.rstrip())
    # library calls: bindings of names to addresses from manifest symbols
    addr2name = {v: k for k, v in man.get("symbols", {}).items() if not re.match(r"[fg]_[0-9a-f]+$", k)}
    calls = set(re.findall(r"call\s+0x([0-9a-f]+)", dis))
    libs = [f"{addr2name[f'1:{c}']} (0x{c})" for c in sorted(calls) if f"1:{c}" in addr2name]
    if libs:
        out.append("# library/runtime calls: " + ", ".join(libs))
    syms = symbols_in(dis)
    known = [s for s in syms if s in decls]
    if known:
        out.append("# declarations proven in src/ (reuse them):")
        seen = set()
        for s in known:
            for d in sorted(decls[s]):
                if d not in seen:
                    seen.add(d)
                    out.append(d)
    unknown = [s for s in syms if s not in decls]
    if unknown:
        out.append("# symbols without a proven declaration yet: " + " ".join(unknown))
    # matched callers: show the call line from their source
    code = show.load()[1]
    start = int(fn["start"], 16)
    call_sites = [i for i in range(len(code) - 5) if code[i] == 0xE8
                  and i + 5 + struct.unpack_from("<i", code, i + 1)[0] == start]
    callers = []
    for f in man["functions"]:
        s, e = int(f["start"], 16), int(f["end"], 16)
        if any(s <= c < e for c in call_sites):
            callers.append(f)
    for f in callers:
        if f.get("status") == "matching":
            txt = (ROOT / f["src"]).read_text(errors="replace")
            lines = [ln.strip() for ln in txt.splitlines() if re.search(rf"\b{name}\s*\(", ln) and not ln.rstrip().endswith(";") or
                     (re.search(rf"\b{name}\s*\(", ln) and "=" in ln or re.search(rf"^\s+{name}\s*\(", ln))]
            out.append(f"# matched caller {f['name']} calls it as: " + " | ".join(lines[:3]))
    other = [f["name"] for f in callers if f.get("status") != "matching"]
    if other:
        out.append("# other callers: " + " ".join(other))
    best = harvest.get(name)
    if best and best["score"] < 1:
        out.append(f"# best draft so far: {best['path']} ({best['line'][:90]})")
    if fn.get("draft"):
        out.append(f"# recorded draft: {fn['draft']} — {fn.get('mismatch', '')}")
    return "\n".join(out) + "\n"


def main(argv):
    man = json.loads((ROOT / "manifest.json").read_text())
    hv = ROOT / "build" / "harvest.json"
    harvest = json.loads(hv.read_text()) if hv.exists() else {}
    decls = load_decls()
    args = argv[1:]
    outdir = None
    if "--out" in args:
        i = args.index("--out"); outdir = Path(args[i + 1]); del args[i:i + 2]
    if args and args[0] == "--range":
        lo, hi = int(args[1], 16), int(args[2], 16)
        names = [f["name"] for f in man["functions"] if lo <= int(f["start"], 16) < hi and f["status"] != "matching"]
    else:
        names = args
    for n in names:
        p = packet(n, man, decls, harvest)
        if outdir:
            outdir.mkdir(parents=True, exist_ok=True)
            (outdir / f"{n}.ctx.txt").write_text(p)
        else:
            sys.stdout.write(p)
    if outdir:
        print(f"wrote {len(names)} packets to {outdir}")


if __name__ == "__main__":
    main(sys.argv)
