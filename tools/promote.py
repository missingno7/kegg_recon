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
    rolled back if anything regresses.
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


def main(argv):
    args = argv[1:]
    if args and args[0] == "--batch":
        return batch(Path(args[1]))
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
        backup = MAN.read_text()
        e = next(f for f in man["functions"] if f.get("name") == func)
        known = {k: v for k, v in man.get("symbols", {}).items()}
        for f in man["functions"]:
            known[f["name"]] = f"1:{int(f['start'], 16):x}"
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
            MAN.write_text(backup)
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
