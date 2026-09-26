"""Check every candidate in worker directories in one run; optionally promote the EXACT ones.

    python tools/harvest.py build/workers/g3                 # table of verdicts for build/workers/g3/f_*.c
    python tools/harvest.py build/workers/* --promote         # supervisor: promote all EXACT, one validation
    python tools/harvest.py build/workers/* --status          # also write build/harvest.json (best per function)

A candidate is a file named after a manifest function (f_HEX.c).  Checks run in parallel (one process).
Only the supervisor passes --promote (single writer).
"""
from __future__ import annotations

import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main(argv):
    promote = "--promote" in argv
    status = "--status" in argv
    jobs = int(argv[argv.index("-j") + 1]) if "-j" in argv else 12
    dirs = [Path(a).resolve() for a in argv[1:] if not a.startswith("-") and not a.isdigit()]
    man = json.loads((ROOT / "manifest.json").read_text())
    fns = {f["name"]: f for f in man["functions"]}
    cands = []
    for d in dirs:
        for c in sorted(d.glob("f_*.c")):
            f = fns.get(c.stem)
            if f and f.get("status") != "matching":
                cands.append((c, f))

    def one(item):
        c, f = item
        out = ROOT / "build" / "harvest" / c.parent.name / (c.stem + ".json")
        out.parent.mkdir(parents=True, exist_ok=True)
        p = subprocess.run([sys.executable, str(ROOT / "tools" / "check.py"), str(c), f["name"], "--at", f["start"],
                            "--end", f["end"], "--profile", f.get("profile", "game-c"), "--json", str(out)],
                           capture_output=True, text=True)
        line = (p.stdout.strip().splitlines() or [p.stderr.strip()[:120] or "ERROR"])[0]
        res = json.loads(out.read_text()) if out.exists() and p.stdout.startswith(("EXACT", "DIFF")) else None
        return c, f, line, res

    results = []
    with ThreadPoolExecutor(jobs) as ex:
        results = list(ex.map(one, cands))
    exact = [(c, f) for c, f, line, _ in results if line.startswith("EXACT")]
    for c, f, line, _ in sorted(results, key=lambda r: int(r[1]["start"], 16)):
        print(f"{c.parent.name:10} {line[:110]}")
    print(f"# {len(exact)}/{len(results)} EXACT")
    if status:
        best = {}
        for c, f, line, res in results:
            score = 0
            if res:
                d = res.get("diff") or {}
                score = 1.0 if line.startswith("EXACT") else (d.get("equal_insns") or 0) / max(1, d.get("orig_insns") or 1)
            if f["name"] not in best or score > best[f["name"]]["score"]:
                best[f["name"]] = {"score": round(score, 3), "path": str(c.relative_to(ROOT)), "line": line}
        (ROOT / "build" / "harvest.json").write_text(json.dumps(best, indent=1))
    if promote and exact:
        sys.path.insert(0, str(ROOT / "tools"))
        import promote as pr
        done = []
        for c, f in exact:
            if pr.main([None, str(c), f["name"], "--no-validate"]) == 0:
                done.append(f["name"])
        v = subprocess.run([sys.executable, str(ROOT / "tools" / "validate.py"), "--quiet"], capture_output=True, text=True)
        print(v.stdout.strip())
        print(f"promoted {len(done)}")
        return v.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
