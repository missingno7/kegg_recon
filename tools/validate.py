"""Regression gate: freshly rebuild and re-verify every 'matching' function in manifest.json.

    python tools/validate.py [--quiet] [-j N] [--host dosbox]

Also checks manifest consistency: sorted non-overlapping extents, source hashes, unique names.
Exit status 0 only if everything that is claimed still verifies EXACT.
"""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main(argv):
    quiet = "--quiet" in argv
    jobs = int(argv[argv.index("-j") + 1]) if "-j" in argv else 8
    host = argv[argv.index("--host") + 1] if "--host" in argv else "nt"
    man = json.loads((ROOT / "manifest.json").read_text())
    fns = man.get("functions", [])
    errors = []
    names = [f["name"] for f in fns]
    if len(set(names)) != len(names):
        errors.append("duplicate function names")
    prev = (0, -1)
    for f in sorted(fns, key=lambda f: (f.get("object", 1), int(f["start"], 16))):
        s, e, ob = int(f["start"], 16), int(f["end"], 16), f.get("object", 1)
        if ob == prev[0] and s < prev[1]:
            errors.append(f"overlap at {f['name']} {f['start']}")
        prev = (ob, e)
    todo = [f for f in fns if f.get("status") == "matching" and not f.get("unit")]
    units = man.get("units", [])
    for u in units:
        if hashlib.sha256((ROOT / u["src"]).read_bytes()).hexdigest() != u.get("src_sha256"):
            errors.append(f"unit {u['id']}: source changed since promotion")
    for f in todo:
        src = ROOT / f["src"]
        if not src.exists():
            errors.append(f"{f['name']}: missing {f['src']}")
        elif hashlib.sha256(src.read_bytes()).hexdigest() != f.get("src_sha256"):
            errors.append(f"{f['name']}: source changed since promotion")

    def one(f):
        out = ROOT / "build" / "validate" / f"{f['name']}.json"
        cmd = [sys.executable, str(ROOT / "tools" / "check.py"), str(ROOT / f["src"]), f["name"],
               "--at", f["start"], "--end", f["end"], "--profile", f.get("profile", "game-c"), "--json", str(out), "--host", host]
        p = subprocess.run(cmd, capture_output=True, text=True)
        return f, p

    def unit(u):
        out = ROOT / "build" / "validate" / f"unit_{u['id']}.json"
        cmd = [sys.executable, str(ROOT / "tools" / "check.py"), str(ROOT / u["src"]), "--all", "--at", u["start"],
               "--end", u["end"], "--profile", u.get("profile", "game-c"), "--json", str(out), "--host", host,
               *[a for pl in u.get("place", []) for a in ("--place", pl)]]
        return u, subprocess.run(cmd, capture_output=True, text=True)

    uok = 0
    with ThreadPoolExecutor(jobs) as ex:
        for u, p in ex.map(unit, units):
            if p.returncode == 0 and p.stdout.startswith("EXACT"):
                uok += 1
            else:
                errors.append(f"unit {u['id']}: {p.stdout.strip().splitlines()[0] if p.stdout.strip() else p.stderr[:200]}")
    ok = 0
    with ThreadPoolExecutor(jobs) as ex:
        for f, p in ex.map(one, todo):
            if p.returncode == 0 and p.stdout.startswith("EXACT"):
                ok += 1
            else:
                errors.append(f"{f['name']}: {p.stdout.strip().splitlines()[0] if p.stdout.strip() else p.stderr.strip()[:200]}")
    code_bytes = sum(int(f["end"], 16) - int(f["start"], 16) for f in fns if f.get("status") == "matching")
    print(f"validate: {uok}/{len(units)} units + {ok}/{len(todo)} separate functions EXACT; "
          f"{sum(1 for f in fns if f.get('status') == 'matching')} functions, {code_bytes} bytes; {len(errors)} error(s)")
    for e in errors[: (20 if quiet else 200)]:
        print("  -", e)
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
