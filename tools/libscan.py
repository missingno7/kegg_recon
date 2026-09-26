"""Locate library/object code segments inside the original image.

For every module code segment (class CODE) in the given .LIB/.OBJ files, search
the LE code object for the segment bytes with fixup fields masked.  Reports
exact masked hits and, for unmatched modules, the best partial anchor.

    python tools/libscan.py assets/KE.EXE C:/tools/watcom-10.0a/LIB386/DOS/CLIB3S.LIB [...]
          [--json OUT.json] [--obj N]

A masked hit is diagnostic evidence (library member identity), not a proof of
fixup targets; use the verifier for that.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import le as lemod  # noqa: E402
import omf  # noqa: E402


def masked_pattern(mod: omf.Module, si: int):
    seg = mod.segments[si]
    mask = bytearray(seg.covered)
    for f in mod.fixups:
        if f.seg == si:
            for k in range(f.size()):
                if f.off + k < len(mask):
                    mask[f.off + k] = 0
    return bytes(seg.data), bytes(mask)


def find_masked(hay: bytes, pat: bytes, mask: bytes, min_anchor=6):
    n = len(pat)
    if n == 0:
        return []
    # choose longest unmasked run as anchor
    best, cur, start = (0, 0), 0, 0
    for i, m in enumerate(mask):
        if m:
            if cur == 0:
                start = i
            cur += 1
            if cur > best[1]:
                best = (start, cur)
        else:
            cur = 0
    a0, alen = best
    if alen < min_anchor:
        return []
    anchor = pat[a0:a0 + alen]
    hits, p = [], hay.find(anchor)
    while p != -1:
        base = p - a0
        if base >= 0 and base + n <= len(hay):
            if all(not mask[i] or hay[base + i] == pat[i] for i in range(n)):
                hits.append(base)
        p = hay.find(anchor, p + 1)
    return hits


def scan(image, libs, obj_index=1):
    le = lemod.LE(image)
    code = le.object_bytes(le.objects[obj_index - 1])
    results = []
    for lib in libs:
        for m in omf.load(lib):
            for si, seg in enumerate(m.segments):
                if not seg or seg.cls.upper() not in ("CODE",) or seg.size == 0:
                    continue
                pat, mask = masked_pattern(m, si)
                hits = find_masked(code, pat, mask)
                results.append({"lib": Path(lib).name, "module": m.name, "seg": seg.name,
                                "size": seg.size, "hits": hits,
                                "publics": [n for n, s, o, _ in m.publics if s == si]})
    return results


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    out = None
    if "--json" in argv:
        out = argv[argv.index("--json") + 1]
        args.remove(out)
    res = scan(args[0], args[1:])
    hit = [r for r in res if r["hits"]]
    for r in sorted(hit, key=lambda r: r["hits"][0]):
        print(f"{r['hits'][0]:06x} +{r['size']:<5x} {r['lib']}:{r['module']} {' '.join(r['publics'][:4])}"
              + (f"  (x{len(r['hits'])})" if len(r["hits"]) > 1 else ""))
    print(f"# {len(hit)}/{len(res)} code segments found; {sum(r['size'] for r in hit)} bytes")
    if out:
        Path(out).write_text(json.dumps(res, indent=1))


if __name__ == "__main__":
    main(sys.argv)
