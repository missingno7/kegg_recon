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
    data_status(m)
    image_status()
    print(f"matching: {nmatch}/{tot_fn} functions, {match}/{tot_b} bytes of inventoried game code "
          f"({100 * match / max(1, tot_b):.1f}%); code object is {code} bytes (runtime library + asm tracked separately)")


def data_status(m):
    seen = set()
    by = {}
    for u in m.get("units", []):
        for d in u.get("data", []):
            seg, rest = d.split("@")
            base, size = rest.split("+")
            key = (seg, base)
            if key not in seen:
                seen.add(key)
                by[seg] = by.get(seg, 0) + int(size, 16)
    init = int(m.get("runtime", {}).get("data_layout", {}).get("init_end", "0x886f"), 16)
    print(f"units: {len(m.get('units', []))}; verified initialised data: " +
          ", ".join(f"{k} {v} bytes" for k, v in sorted(by.items())) + f" (obj3 initialised size {init})")


def image_status():
    rep = ROOT / "build" / "image" / "canonical" / "report.json"
    if not rep.exists():
        print("image: run python tools/image.py --mode canonical")
        return
    r = json.loads(rep.read_text())
    b = r.get("bytes") or r.get("accounting") or {}
    print("whole image (last canonical link): " + ("IDENTICAL" if r.get("identical") else "NOT identical") +
          "; bytes by owner: " + ", ".join(f"{k} {v}" for k, v in b.items()))


if __name__ == "__main__":
    main()
