"""Provision / verify historical toolchain installs described in toolchain/toolchain.json.

    python toolchain/install.py --verify            # check every install, external runner and python dependency
    python toolchain/install.py --verify --quick    # installs: key files only (no full-tree hashes)
    python toolchain/install.py --install wc100a    # (re)create an install from its pinned source archive
    python toolchain/install.py --lock wc100a       # (re)compute tree hash + key file hashes into the JSON

Installs live outside the repository (default C:/tools, override with KEGG_TOOLS).
Proprietary binaries are never copied into the repository.

Verification fails closed: exit status 1 if any install is missing, any key file or tree differs from its lock,
any key file / launched host executable has no configured digest, an external runner (toolchain.json "runners":
DOSBox-X, MS-DOS Player) differs from its pinned SHA-256, or the vendored capstone tree (build/pylib,
"python_deps") differs.  `--install` is fatal if the freshly extracted tree does not verify.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
CONFIG = HERE / "toolchain.json"


def tools_root():
    return Path(os.environ.get("KEGG_TOOLS", "C:/tools"))


def load():
    return json.loads(CONFIG.read_text())


def install_dir(name, cfg=None):
    cfg = cfg or load()
    return tools_root() / cfg["installs"][name]["dir"]


def sha256(p: Path):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def tree_hash(root: Path, skip=lambda p: False):
    """sha256 over sorted 'relpath<TAB>sha256' lines (case-preserving, '/' separators)."""
    lines = []
    for p in sorted(root.rglob("*")):
        if p.is_file() and not skip(p):
            lines.append(f"{p.relative_to(root).as_posix()}\t{sha256(p)}")
    return hashlib.sha256("\n".join(lines).encode()).hexdigest(), len(lines)


def launched_files(inst):
    """Every executable a host mapping can launch (tools/dosrun.py hash-checks each of them)."""
    return sorted({p for hs in inst.get("hosts", {}).values() for p in hs.values()})


def verify(name, cfg, quick=False):
    """(ok, message) for one install."""
    inst = cfg["installs"][name]
    root = install_dir(name, cfg)
    if not root.exists():
        return False, f"{name}: MISSING ({root})"
    key_files = inst.get("key_files", {})
    if not key_files:
        return False, f"{name}: NO KEY FILES LOCKED"
    locked = {k.lower() for k, h in key_files.items() if h}
    undigested = [f for f, h in key_files.items() if not h]
    undigested += [p for p in launched_files(inst) if p.lower() not in locked]
    if undigested:
        return False, f"{name}: KEY FILE WITHOUT DIGEST {sorted(set(undigested))}"
    bad = [f for f, h in key_files.items() if not (root / f).is_file() or sha256(root / f) != h.lower()]
    if bad:
        return False, f"{name}: KEY FILE MISMATCH {bad}"
    if quick or "tree_sha256" not in inst:
        return True, f"{name}: ok ({len(key_files)} key files{'' if 'tree_sha256' in inst else '; no tree lock'})"
    th, n = tree_hash(root)
    if th != inst["tree_sha256"] or n != inst.get("tree_files"):
        return False, f"{name}: TREE MISMATCH ({n} files, {th})"
    return True, f"{name}: ok ({n} files)"


def verify_runner(name, r):
    if not r.get("sha256"):
        return False, f"runner {name}: NO PINNED SHA-256"
    path = Path(os.environ[r["env"]]) if r.get("env") and os.environ.get(r["env"]) else tools_root() / r["path"]
    if not path.is_file():
        return False, f"runner {name}: MISSING ({path})"
    got = sha256(path)
    if got != r["sha256"].lower():
        return False, f"runner {name}: MISMATCH {path} sha256 {got} != pinned {r['sha256']}"
    return True, f"runner {name}: ok ({path} {got[:16]})"


def _pycache(p: Path):
    return "__pycache__" in p.parts or p.suffix == ".pyc"


def verify_pydep(name, d):
    root = ROOT / d["dir"]
    if not d.get("tree_sha256"):
        return False, f"python {name}: NO PINNED TREE HASH"
    if not root.is_dir():
        return False, f"python {name}: MISSING ({root})"
    th, n = tree_hash(root, _pycache)
    if th != d["tree_sha256"] or n != d.get("tree_files"):
        return False, f"python {name}: TREE MISMATCH {root} ({n} files, {th})"
    return True, f"python {name} {d.get('version', '')}: ok ({n} files in {d['dir']})"


def used_installs(cfg):
    """installs the canonical build needs: profiles of every manifest unit/function, plus the link install"""
    man = json.loads((CONFIG.parent.parent / "manifest.json").read_text())
    profs = {u.get("profile", "game-c") for u in man.get("units", [])}
    profs |= {f["profile"] for f in man.get("functions", []) if f.get("src") and f.get("profile")}
    used = {cfg["profiles"][p]["install"] for p in profs if p in cfg["profiles"]}
    return used | {"wc100"}


def verify_all(cfg, quick=False):
    used = used_installs(cfg)
    results = []
    for name in cfg["installs"]:
        ok, msg = verify(name, cfg, quick)
        if not ok and name not in used and not install_dir(name, cfg).exists():
            ok, msg = True, f"{name}: optional (control/comparison install, not used by the canonical build) - absent"
        results.append((ok, msg))
    runners = cfg.get("runners", {})
    for need in ("dosbox-x", "msdos"):
        if need not in runners:
            results.append((False, f"runner {need}: NOT PINNED in toolchain.json"))
    results += [verify_runner(n, r) for n, r in runners.items()]
    results += [verify_pydep(n, d) for n, d in cfg.get("python_deps", {}).items()]
    if "capstone" not in cfg.get("python_deps", {}):
        results.append((False, "python capstone: NOT PINNED in toolchain.json"))
    return results


def do_install(name, cfg):
    inst = cfg["installs"][name]
    src = inst["source"]
    archive = tools_root() / src["local"]
    if sha256(archive) != src["sha256"]:
        raise SystemExit(f"source archive hash mismatch: {archive}")
    dest = install_dir(name, cfg)
    if dest.exists():
        raise SystemExit(f"{dest} exists; remove it first")
    with tempfile.TemporaryDirectory(dir=tools_root()) as tmp:
        subprocess.run(["7z", "x", "-y", f"-o{tmp}", str(archive)], check=True, stdout=subprocess.DEVNULL)
        sub = Path(tmp) / src.get("subdir", "")
        shutil.move(str(sub), str(dest))
    for rel, new in src.get("renames", {}).items():
        (dest / rel).rename(dest / new)
    ok, msg = verify(name, cfg)
    print(msg)
    if not ok:
        raise SystemExit(f"INSTALL FAILED VERIFICATION: {name} ({dest}) does not match its lock; do not use it")


def do_lock(name, cfg):
    root = install_dir(name, cfg)
    inst = cfg["installs"][name]
    inst["key_files"] = {f: sha256(root / f) for f in inst.get("key_files", {})}
    inst["tree_sha256"], inst["tree_files"] = tree_hash(root)
    tmp = CONFIG.with_suffix(".json.tmp")
    tmp.write_bytes((json.dumps(cfg, indent=1) + "\n").encode())
    os.replace(tmp, CONFIG)
    print(f"locked {name}: {inst['tree_files']} files {inst['tree_sha256']}")


def main(argv):
    cfg = load()
    if "--install" in argv:
        do_install(argv[argv.index("--install") + 1], cfg)
        return 0
    if "--lock" in argv:
        do_lock(argv[argv.index("--lock") + 1], cfg)
        return 0
    results = verify_all(cfg, quick="--quick" in argv)
    for ok, msg in results:
        print(msg)
    bad = [m for ok, m in results if not ok]
    print(f"toolchain verification: {'FAILED, ' + str(len(bad)) + ' problem(s)' if bad else 'all OK'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
