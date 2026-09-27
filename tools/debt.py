"""Readability-debt inventory of the canonical sources (per unit, deterministic; reads text only).

    python tools/debt.py [--json build/debt.json] [--unit ID ...] [--list KIND]

Columns: addr = address-derived identifiers (f_9d40, g_dDb8, x_dd40_xbukycw, ...), ploc = placeholder locals/params
(v_10, a0, L_86a7 labels excluded), goto, raw = dereferences of casted pointer arithmetic (`*(int *)(p + 0x74)`),
cast = pointer casts, hex = hex literals outside initialiser lines, hw = direct DOS/hardware touch points (port I/O,
int386/intdos, DPMI, BIOS/VGA addresses), PINNED = host-pinned -ot source, editable only under the offset rule (docs/compiler-notes.md).
--list KIND prints the distinct items of one kind per unit (addr, ploc, raw, hw).
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RX = {
    "addr": re.compile(r"\b[a-zA-Z]_[0-9a-fA-F]{3,5}(?:_[a-zA-Z0-9]+)?\b"),
    "ploc": re.compile(r"\b(?:v_[0-9a-f]+|a[0-9]{1,2}|local_[0-9a-f]+|var_[0-9a-f]+|arg_[0-9a-f]+)\b"),
    "goto": re.compile(r"\bgoto\b"),
    "raw": re.compile(r"\*\s*\(\s*(?:(?:unsigned|signed)\s+)?(?:char|short|int|long)\s*\*\s*\)\s*\(?[^;()]*\+\s*(?:0x[0-9a-fA-F]+|[0-9]+)"),
    "cast": re.compile(r"\(\s*(?:(?:unsigned|signed)\s+)?(?:char|short|int|long|void)\s*\*+\s*\)"),
    "hex": re.compile(r"\b0x[0-9a-fA-F]+\b"),
    "hw": re.compile(r"\b(?:inp|outp|inpw|outpw|_inp|_outp|int386x?|intdos|intr|_dos_\w+|_bios_\w+|segread|FP_SEG|FP_OFF|"
                     r"in\s+al|out\s+dx|int\s+[0-9a-fA-F]+h?|cli|sti)\b|0x(?:a0000|b8000|3c[4-9a-f]|3d[4-a]|3da|20|21|a0|a1|"
                     r"4[0-3]|60|61|64|2[2-9a-f]|20[0-7])\b", re.I),
}
ASM_ADDR = re.compile(r"\b(?:[a-zA-Z]_[0-9a-fA-F]{3,5}(?:_[a-z]+)?|L_[0-9a-fA-F]{3,5})\b")


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def measure(path: Path, is_asm: bool):
    text = path.read_text(encoding="latin-1")
    if is_asm:
        body = re.sub(r";[^\n]*", "", text)
        items = {"addr": sorted(set(ASM_ADDR.findall(body)))}
        hw = re.findall(r"^\s*(?:in|out|int|cli|sti|rep\s+outs\w*|rep\s+ins\w*)\b[^\n]*", body, re.I | re.M)
        items["hw"] = sorted(set(h.strip() for h in hw))
        return {"lines": text.count("\n"), "addr": len(items["addr"]), "hw": len(hw)}, items
    body = strip_comments(text)
    code_lines = [l for l in body.splitlines() if not re.match(r"\s*[{,]?\s*(0x[0-9a-fA-F]+\s*,\s*)+", l)]
    code = "\n".join(code_lines)
    items = {
        "addr": sorted(set(RX["addr"].findall(body))),
        "ploc": sorted(set(RX["ploc"].findall(body))),
        "raw": RX["raw"].findall(code),
        "hw": RX["hw"].findall(code),
    }
    row = {"lines": text.count("\n"), "addr": len(items["addr"]), "ploc": len(items["ploc"]),
           "goto": len(RX["goto"].findall(body)), "raw": len(items["raw"]), "cast": len(RX["cast"].findall(code)),
           "hex": len(RX["hex"].findall(code)), "hw": len(items["hw"])}
    return row, items


def main(argv):
    man = json.loads((ROOT / "manifest.json").read_text())
    prof = json.loads((ROOT / "toolchain" / "toolchain.json").read_text())["profiles"]
    only = set()
    if "--unit" in argv:
        only = {a for a in argv[argv.index("--unit") + 1:] if not a.startswith("--")}
    kind = argv[argv.index("--list") + 1] if "--list" in argv else None
    rows, seen = [], set()
    srcs = [(u["id"], u["src"], u.get("profile", "game-c")) for u in man["units"]]
    srcs += [(f["name"], f["src"], f.get("profile", "")) for f in man["functions"]
             if f.get("src") and f["src"] not in {s for _, s, _ in srcs}]
    for uid, src, pf in srcs:
        if src in seen or (only and uid not in only):
            continue
        seen.add(src)
        p = prof.get(pf, {})
        is_asm = p.get("tool", "wcc386") != "wcc386"
        row, items = measure(ROOT / src, is_asm)
        row.update(id=uid, src=src, kind="asm" if is_asm else "c", frozen=bool(p.get("host")) and not is_asm)
        rows.append(row)
        if kind and items.get(kind):
            print(f"{uid} {src}: " + ", ".join(items[kind] if kind != "raw" else items[kind][:40]))
    if kind:
        return 0
    cols = ["lines", "addr", "ploc", "goto", "raw", "cast", "hex", "hw"]
    print(f"{'unit':14} {'src':28} " + " ".join(f"{c:>5}" for c in cols))
    tot = {c: 0 for c in cols}
    for r in rows:
        print(f"{r['id']:14} {r['src']:28} " + " ".join(f"{r.get(c, ''):>5}" for c in cols)
              + ("  PINNED(offset rule)" if r["frozen"] else "") + ("  asm" if r["kind"] == "asm" else ""))
        for c in cols:
            tot[c] += r.get(c, 0) or 0
    print(f"{'TOTAL':14} {'':28} " + " ".join(f"{tot[c]:>5}" for c in cols))
    if "--json" in argv:
        out = ROOT / argv[argv.index("--json") + 1]
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"rows": rows, "total": tot}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
