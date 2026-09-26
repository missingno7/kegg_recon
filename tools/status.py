"""Progress summary from manifest.json (the only status authority).

    python tools/status.py
"""
import collections
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    m = json.loads((ROOT / "manifest.json").read_text())
    by = collections.defaultdict(lambda: [0, 0])
    for f in m["functions"]:
        n = int(f["end"], 16) - int(f["start"], 16)
        k = (f["kind"], f["status"])
        by[k][0] += 1
        by[k][1] += n
    code = int(m["le"]["objects"][0]["vsize"], 16)
    tot_fn = sum(v[0] for v in by.values())
    tot_b = sum(v[1] for v in by.values())
    for (kind, st), (n, b) in sorted(by.items()):
        print(f"{kind:12} {st:11} {n:4} functions {b:7} bytes")
    match = sum(v[1] for (k, s), v in by.items() if s == "matching")
    nmatch = sum(v[0] for (k, s), v in by.items() if s == "matching")
    print(f"matching: {nmatch}/{tot_fn} functions, {match}/{tot_b} bytes of inventoried game code "
          f"({100 * match / max(1, tot_b):.1f}%); code object is {code} bytes (runtime library + asm tracked separately)")


if __name__ == "__main__":
    main()
