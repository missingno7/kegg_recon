"""Apply an object plan atomically: a set of translation units (+ symbol renames) replacing current units.

    python tools/replan.py PLAN.json [--dry-run]

PLAN.json = {"units": [{id, file, dest, range: [start, end], profile, place: [...]}, ...],
             "renames": {old: new}, "symbols": {name: "obj:off"}}
(as produced by the object-structure work, build/workers/objects/promote/promote_plan.json).

Transaction (single writer, under promote.py's lock):
  1. snapshot src/, asm/, manifest.json;
  2. apply renames to every canonical source that is not replaced by a plan unit, to manifest function names and
     to manifest symbols;
  3. install the plan units; every existing unit overlapping a plan unit is removed; members of a removed unit
     that no plan unit covers keep that unit's old file as a multi-function source (verified per function);
  4. `validate.py` must pass AND `image.py --mode canonical` must reproduce KE.EXE; otherwise everything is
     restored from the snapshot.
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import promote as P  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


def sha(p: Path):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def lf(b: bytes):
    return b.replace(bytes([13, 10]), bytes([10]))


def main(argv):
    plan = json.loads(Path(argv[1]).read_text())
    dry = "--dry-run" in argv
    renames = plan.get("renames", {})
    if isinstance(renames, list):
        renames = dict(renames)
    units = plan["units"]
    rng = lambda u: (int(u["range"][0], 16), int(u["range"][1], 16))
    plan_dests = {u["dest"] for u in units}
    with P.Lock():
        man = P.load_manifest()
        stamp = time.strftime("%Y%m%d-%H%M%S")
        snap = ROOT / "build" / "promote" / f"replan-{stamp}"
        for d in ("src", "asm"):
            shutil.copytree(ROOT / d, snap / d)
        shutil.copyfile(P.MAN, snap / "manifest.json")

        def restore(msg):
            for d in ("src", "asm"):
                shutil.rmtree(ROOT / d)
                shutil.copytree(snap / d, ROOT / d)
            shutil.copyfile(snap / "manifest.json", P.MAN)
            print("ROLLED BACK:", msg)
            return 1

        # 1. renames in sources that stay
        rx = re.compile(r"\b(" + "|".join(map(re.escape, sorted(renames, key=len, reverse=True))) + r")\b") if renames else None
        touched = []
        if rx:
            for p in [*ROOT.glob("src/*.c"), *ROOT.glob("asm/*.asm")]:
                rel = p.relative_to(ROOT).as_posix()
                if rel in plan_dests:
                    continue
                s = p.read_text(errors="replace")
                t = rx.sub(lambda m: renames[m.group(1)], s)
                if t != s:
                    p.write_text(t, newline="\n")
                    touched.append(rel)
        # 2. manifest names and symbols
        for f in man["functions"]:
            if f["name"] in renames:
                f["name"] = renames[f["name"]]
        syms = man.get("symbols", {})
        for old, new in renames.items():
            if old in syms:
                syms[new] = syms.pop(old)
        syms.update(plan.get("symbols", {}) if isinstance(plan.get("symbols"), dict) else dict(plan.get("symbols", [])))
        # 3. units
        covered = lambda a: any(rng(u)[0] <= a < rng(u)[1] for u in units)
        old_units = man.get("units", [])
        removed = [u for u in old_units if any(int(u["start"], 16) < rng(p)[1] and rng(p)[0] < int(u["end"], 16) for p in units)]
        keep_units = [u for u in old_units if u not in removed]
        removed_ids = {u["id"] for u in removed}
        leftovers = {}
        for f in man["functions"]:
            if f.get("unit") in removed_ids and not covered(int(f["start"], 16)):
                f.pop("unit", None)  # stays matching, verified per function from the old unit file
                leftovers.setdefault(f["src"], []).append(f["name"])
        new_units = []
        for u in units:
            s0, e0 = rng(u)
            dest = ROOT / u["dest"]
            dest.parent.mkdir(exist_ok=True)
            dest.write_bytes(lf((ROOT / u["file"]).read_bytes()))
            h = sha(dest)
            members = [f for f in man["functions"] if f.get("object", 1) == 1 and s0 <= int(f["start"], 16) < e0]
            for f in members:
                f.update({"status": "matching", "src": u["dest"], "unit": u["id"], "profile": u.get("profile", "game-c"),
                          "src_sha256": h})
                for k in ("draft", "mismatch", "note"):
                    f.pop(k, None)
            new_units.append({"id": u["id"], "src": u["dest"], "start": u["range"][0], "end": u["range"][1],
                              "profile": u.get("profile", "game-c"), "src_sha256": h,
                              **({"place": u["place"]} if u.get("place") else {}),
                              "note": u.get("note", "object plan (build/workers/objects)")})
        man["units"] = sorted(keep_units + new_units, key=lambda u: int(u["start"], 16))
        # refresh hashes of every source (renames changed some)
        for f in man["functions"]:
            if f.get("status") == "matching" and (ROOT / f["src"]).exists():
                f["src_sha256"] = sha(ROOT / f["src"])
        for u in man["units"]:
            u["src_sha256"] = sha(ROOT / u["src"])
        # delete files no longer referenced
        refs = {f["src"] for f in man["functions"] if f.get("src")} | {u["src"] for u in man["units"]}
        gone = [p.relative_to(ROOT).as_posix() for p in [*ROOT.glob("src/*.c"), *ROOT.glob("asm/*.asm")]
                if p.relative_to(ROOT).as_posix() not in refs]
        for g in gone:
            (ROOT / g).unlink()
        P.save_manifest(man)
        print(f"plan: {len(new_units)} units in, {len(removed)} replaced ({', '.join(sorted(removed_ids))}); "
              f"{sum(map(len, leftovers.values()))} leftover functions keep old files; renamed in {len(touched)} files; "
              f"{len(gone)} files retired")
        if dry:
            return restore("dry run")
        v = subprocess.run([sys.executable, str(ROOT / "tools" / "validate.py"), "--quiet", "-j", "12"],
                           capture_output=True, text=True)
        print(v.stdout.strip()[:3000])
        if v.returncode != 0:
            return restore("validate failed")
        im = subprocess.run([sys.executable, str(ROOT / "tools" / "image.py"), "--mode", "canonical"],
                            capture_output=True, text=True)
        print(im.stdout.strip()[:2000])
        if im.returncode != 0:
            return restore("whole image not identical")
    print("PLAN APPLIED")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
