"""Promote a verified candidate into canonical source (single writer), or record a draft.

    python tools/promote.py CAND.c FUNC [--verify-only]          # must be EXACT; writes src/FUNC.c + manifest
    python tools/promote.py CAND.c FUNC --draft "short note"     # not exact: keep best draft + mismatch summary
    python tools/promote.py CAND.asm a_0 --as asm/irq.asm         # several routines sharing one module file
    python tools/promote.py --batch DIR                           # every DIR/f_HEX.c that is EXACT, one validation

FUNC must be a manifest function (its start/end are the authority for the extent).  The candidate is
frozen (copied) and freshly compiled; nothing previously tested is trusted.  Extra gates:
  * address-named symbols f_HEX / g_HEX must bind to 1:HEX / 3:HEX (names encode original addresses);
  * bindings must not contradict manifest symbols;
  * after publishing, every other matching function is re-verified (tools/validate.py) and the publish is
    rolled back if anything regresses; a --unit promotion additionally requires the whole-image gate
    (tools/validate.py --image: one WLINK run == KE.EXE, zero raw debt) or it is rolled back.  Overlap with
    existing units is checked before the destination file is written.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dosrun  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
MAN = ROOT / "manifest.json"
LOCK = ROOT / "build" / "promote.lock"
NAME_RX = re.compile(r"^([fg])_([0-9a-f]+)$")


def load_manifest():
    return json.loads(MAN.read_text())


def save_manifest(m):
    tmp = MAN.with_suffix(".tmp")
    with open(tmp, "w", newline="\n") as fh:
        fh.write(json.dumps(m, indent=1) + "\n")
    os.replace(tmp, MAN)


def run_check(src: Path, func: str, entry, jpath: Path):
    cmd = [sys.executable, str(ROOT / "tools" / "check.py"), str(src), func, "--at", entry["start"],
           "--end", entry["end"], "--profile", entry.get("profile", "game-c"), "--json", str(jpath)]
    p = subprocess.run(cmd, capture_output=True, text=True)
    res = json.loads(jpath.read_text()) if jpath.exists() else None
    return p, res


def name_gate(res):
    bad = []
    for k, v in res.get("bindings", {}).items():
        m = NAME_RX.match(k)
        if m:
            want = f"{1 if m.group(1) == 'f' else 3}:{int(m.group(2), 16):x}"
            if v != want:
                bad.append(f"{k} binds to {v}, name says {want}")
    return bad


class Lock:
    def __enter__(self):
        LOCK.parent.mkdir(parents=True, exist_ok=True)
        for _ in range(600):
            try:
                self.fd = os.open(LOCK, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                os.write(self.fd, str(os.getpid()).encode())
                return self
            except FileExistsError:
                time.sleep(0.5)
        raise SystemExit(f"promotion lock busy: {LOCK} (delete it if no promotion is running)")

    def __exit__(self, *a):
        os.close(self.fd)
        LOCK.unlink(missing_ok=True)


def batch(d: Path):
    man = load_manifest()
    todo = {f["name"]: f for f in man["functions"] if f.get("status") != "matching"}
    done = []
    for c in sorted(Path(d).glob("f_*.c")):
        if c.stem in todo:
            rc = main([None, str(c), c.stem, "--no-validate"])
            if rc == 0:
                done.append(c.stem)
    v = subprocess.run([sys.executable, str(ROOT / "tools" / "validate.py"), "--quiet"], capture_output=True, text=True)
    print(v.stdout.strip())
    print(f"batch: promoted {len(done)}: {' '.join(done)}")
    return v.returncode


def promote_unit(cand: Path, uid: str, dest_rel: str, start: str, end: str, profile: str, note: str, places=()):
    """A whole translation unit: one source file, verified by one --all check over [start, end) including the
    unit's own data segments. Every manifest function inside the range becomes 'matching' with src = the unit."""
    man = load_manifest()
    s0, e0 = int(start, 16), int(end, 16)
    members = [f for f in man["functions"] if f.get("object", 1) == 1 and s0 <= int(f["start"], 16) < e0]
    data_only = s0 == e0   # data-only unit: empty code range = its link position; data verified via --place
    if data_only and not places:
        raise SystemExit("a data-only unit (empty range) needs --place for its data segments")
    if not data_only and (not members or int(members[0]["start"], 16) != s0 or int(members[-1]["end"], 16) > e0):
        raise SystemExit("range does not start at a manifest function or cuts one")
    stamp = time.strftime("%Y%m%d-%H%M%S")
    frozen_dir = ROOT / "build" / "promote" / f"{uid}-{stamp}"
    frozen_dir.mkdir(parents=True, exist_ok=True)
    frozen = frozen_dir / Path(dest_rel).name
    prof_ = dosrun.config()["profiles"].get(profile, {})
    verbatim = bool(prof_.get("host")) and prof_.get("tool", "wcc386") == "wcc386"
    raw = cand.read_bytes()
    # host-pinned profiles (e.g. -ot literal padding = stale source-buffer bytes) depend on the exact file bytes,
    # CRLF included: keep them verbatim (and mark the file -text in .gitattributes)
    frozen.write_bytes(raw if verbatim else raw.replace(bytes([13, 10]), bytes([10])))
    jpath = frozen_dir / "result.json"
    placeargs = [a for pl in places for a in ("--place", pl)]
    p = subprocess.run([sys.executable, str(ROOT / "tools" / "check.py"), str(frozen), "--all", "--at", start,
                        "--end", end, "--profile", profile, "--json", str(jpath), *placeargs], capture_output=True, text=True)
    print(p.stdout.strip())
    res = json.loads(jpath.read_text()) if jpath.exists() else None
    if not res or res["verdict"] != "EXACT":
        print("NOT PROMOTED")
        return 1
    names = [x["name"] for x in res.get("symbols", [])]
    missing = [f["name"] for f in members if f["name"] not in names]
    if missing:
        print("NOT PROMOTED: unit does not define", missing)
        return 1
    with Lock():
        man = load_manifest()
        backup = MAN.read_bytes()
        # every overlap constraint is checked before anything is written
        overl = lambda u: (int(u["start"], 16) < e0 and s0 < int(u["end"], 16)) or \
            (data_only and (u["start"], u["end"]) == (start, end) and u["src"] == dest_rel)
        superseded = [u for u in man.setdefault("units", []) if u["id"] != uid and overl(u)]
        for u in superseded:  # a new unit covering an older one replaces it; members outside the new range revert
            us, ue = int(u["start"], 16), int(u["end"], 16)
            if us < s0 or ue > e0:
                print(f"NOT PROMOTED: unit {uid} partially overlaps unit {u['id']} ({u['start']}..{u['end']}); "
                      f"promote a unit that covers it completely")
                return 1
        dest = ROOT / dest_rel
        old_dest = dest.read_bytes() if dest.exists() else None
        dest.parent.mkdir(exist_ok=True)
        shutil.copyfile(frozen, dest)
        sha = hashlib.sha256(dest.read_bytes()).hexdigest()
        old = set()
        for f in man["functions"]:
            if f.get("object", 1) == 1 and s0 <= int(f["start"], 16) < e0:
                if f.get("src") and f.get("src") != dest_rel:
                    old.add(f["src"])
                f.update({"status": "matching", "src": dest_rel, "unit": uid, "profile": profile, "src_sha256": sha})
                for k in ("draft", "mismatch", "note"):
                    f.pop(k, None)
        for u in superseded:
            old.add(u["src"])
            print(f"superseding unit {u['id']}")
        units = [u for u in man["units"] if u["id"] != uid and not overl(u)]
        units.append({"id": uid, "src": dest_rel, "start": start, "end": end, "profile": profile,
                      "src_sha256": sha, "data": [f"{d['seg']}@{d['base']}+{d['size']:#x}" for d in res.get("data", [])],
                      **({"place": list(places)} if places else {}),
                      **({"note": note} if note else {})})
        man["units"] = sorted(units, key=lambda u: (int(u["start"], 16), int(u["end"], 16)))
        for k, v in res["bindings"].items():
            if not k.startswith(("seg:", "grp:", "sel:")) and k not in man.get("symbols", {}):
                man.setdefault("symbols", {})[k] = v
        save_manifest(man)
        # a unit changes object boundaries / LEDATA chunking: only the whole-image link decides (validate --image)
        v = subprocess.run([sys.executable, str(ROOT / "tools" / "validate.py"), "--quiet", "--image"],
                           capture_output=True, text=True)
        if v.returncode != 0:
            MAN.write_bytes(backup)
            if old_dest is None:
                dest.unlink(missing_ok=True)
            else:
                dest.write_bytes(old_dest)
            print("ROLLED BACK: regression or whole-image gate failed (validate.py --image)\n" + v.stdout[-2000:])
            return 1
        print(v.stdout.strip()[-1000:])
        still = {f.get("src") for f in man["functions"]}
        for o in old:
            if o not in still and (ROOT / o).exists():
                (ROOT / o).unlink()  # superseded per-function file (kept in git history)
    print(f"PROMOTED unit {uid} ({len(members)} functions) -> {dest_rel}")
    return 0


def main(argv):
    args = argv[1:]
    if args and args[0] == "--batch":
        return batch(Path(args[1]))
    if args and args[0] == "--unit":
        # --unit ID CAND DEST --range START END [--profile P] [--note TEXT]
        uid, cand, dest = args[1], Path(args[2]).resolve(), args[3]
        i = args.index("--range")
        prof = args[args.index("--profile") + 1] if "--profile" in args else "game-c"
        note = args[args.index("--note") + 1] if "--note" in args else ""
        places = [args[j + 1] for j, a in enumerate(args) if a == "--place"]
        return promote_unit(cand, uid, dest, args[i + 1], args[i + 2], prof, note, places)
    no_validate = "--no-validate" in args
    verify_only = "--verify-only" in args
    draft = None
    as_path = None
    if "--as" in args:
        i = args.index("--as"); as_path = args[i + 1]; del args[i:i + 2]
    if "--draft" in args:
        i = args.index("--draft"); draft = args[i + 1]; del args[i:i + 2]
    args = [a for a in args if not a.startswith("--")]
    cand, func = Path(args[0]).resolve(), args[1]
    man = load_manifest()
    entry = next((f for f in man["functions"] if f.get("name") == func), None)
    if entry is None:
        raise SystemExit(f"{func} is not a manifest function")
    stamp = time.strftime("%Y%m%d-%H%M%S")
    frozen_dir = ROOT / "build" / "promote" / f"{func}-{stamp}"
    frozen_dir.mkdir(parents=True, exist_ok=True)
    frozen = frozen_dir / f"{func}{cand.suffix}"
    frozen.write_bytes(cand.read_bytes().replace(bytes([13, 10]), bytes([10])))  # canonical LF (git stores LF)
    p, res = run_check(frozen, func, entry, frozen_dir / "result.json")
    if res and res["verdict"] == "EXACT" and b"volatile" in frozen.read_bytes():
        # -d2 makes volatile stand-ins unnecessary: keep the cleaner source when it is still EXACT
        plain = frozen_dir / ("plain_" + frozen.name)
        plain.write_bytes(re.sub(rb"\bvolatile\s+", b"", frozen.read_bytes()))
        p2, res2 = run_check(plain, func, entry, frozen_dir / "result_plain.json")
        if res2 and res2["verdict"] == "EXACT":
            plain.replace(frozen)
            p, res = p2, res2
    print(p.stdout.strip())
    if res is None:
        print(p.stderr.strip())
        return 2
    problems = list(res["problems"]) + name_gate(res)
    exact = res["verdict"] == "EXACT" and not problems
    if draft is not None:
        if exact:
            print("candidate is EXACT; promote it instead of recording a draft")
            return 1
        with Lock():
            man = load_manifest()
            e = next(f for f in man["functions"] if f.get("name") == func)
            if e.get("status") == "matching":
                raise SystemExit("function already matching; draft not recorded")
            d = ROOT / "drafts"
            d.mkdir(exist_ok=True)
            shutil.copyfile(frozen, d / frozen.name)
            e["status"] = e.get("status") if e.get("status") == "blocked" else "candidate"
            e["draft"] = f"drafts/{frozen.name}"
            dd = res.get("diff") or {}
            e["mismatch"] = (f"{res['cand_size']}/{res['orig_size']} bytes, "
                             f"{dd.get('equal_insns')}/{dd.get('orig_insns')} insns equal; " + "; ".join(problems[:2]))[:300]
            e["note"] = draft
            save_manifest(man)
        print(f"draft recorded: drafts/{frozen.name}")
        return 0
    if not exact:
        print("NOT PROMOTED:", *problems[:6], sep="\n  - ")
        return 1
    if verify_only:
        print("verify-only: EXACT, would promote")
        return 0
    with Lock():
        man = load_manifest()
        backup = MAN.read_bytes()
        e = next(f for f in man["functions"] if f.get("name") == func)
        known = {k: v for k, v in man.get("symbols", {}).items()}
        for f in man["functions"]:
            known[f["name"]] = f"{f.get('object', 1)}:{int(f['start'], 16):x}"
        for k, v in res["bindings"].items():
            if k.startswith(("seg:", "grp:")):
                continue
            if k in known and known[k] != v:
                raise SystemExit(f"binding {k}={v} contradicts {known[k]}")
            if k not in known:
                man.setdefault("symbols", {})[k] = v
        dest = ROOT / (as_path or (("asm/" if frozen.suffix.lower() == ".asm" else "src/") + frozen.name))
        old_src = dest.read_bytes() if dest.exists() else None
        dest.parent.mkdir(exist_ok=True)
        shutil.copyfile(frozen, dest)
        e.update({"status": "matching", "src": dest.relative_to(ROOT).as_posix(), "profile": e.get("profile", "game-c"),
                  "src_sha256": hashlib.sha256(dest.read_bytes()).hexdigest()})
        for k in ("draft", "mismatch", "note"):
            e.pop(k, None)
        man["symbols"] = dict(sorted(man.get("symbols", {}).items()))
        save_manifest(man)
        v = None if no_validate else subprocess.run(
            [sys.executable, str(ROOT / "tools" / "validate.py"), "--quiet"], capture_output=True, text=True)
        if v is not None and v.returncode != 0:
            MAN.write_bytes(backup)
            if old_src is None:
                dest.unlink()
            else:
                dest.write_bytes(old_src)
            print("ROLLED BACK: regression\n" + v.stdout[-2000:])
            return 1
        (ROOT / "drafts" / frozen.name).unlink(missing_ok=True)
    print(f"PROMOTED {func} -> {dest.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
