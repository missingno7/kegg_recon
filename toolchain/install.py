"""Provision / verify historical toolchain installs described in toolchain/toolchain.json.

    python toolchain/install.py --verify            # check every present install against its lock
    python toolchain/install.py --install wc100a    # (re)create an install from its pinned source archive
    python toolchain/install.py --lock wc100a       # (re)compute tree hash + key file hashes into the JSON

Installs live outside the repository (default C:/tools, override with KEGG_TOOLS).
Proprietary binaries are never copied into the repository.
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


def tree_hash(root: Path):
    """sha256 over sorted 'relpath<TAB>sha256' lines (case-preserving, '/' separators)."""
    lines = []
    for p in sorted(root.rglob("*")):
        if p.is_file():
            lines.append(f"{p.relative_to(root).as_posix()}\t{sha256(p)}")
    return hashlib.sha256("\n".join(lines).encode()).hexdigest(), len(lines)


def verify(name, cfg, quick=False):
    inst = cfg["installs"][name]
    root = install_dir(name, cfg)
    if not root.exists():
        return f"{name}: MISSING ({root})"
    bad = [f for f, h in inst.get("key_files", {}).items() if not (root / f).exists() or sha256(root / f) != h]
    if bad:
        return f"{name}: KEY FILE MISMATCH {bad}"
    if quick or "tree_sha256" not in inst:
        return f"{name}: ok (key files)"
    th, n = tree_hash(root)
    if th != inst["tree_sha256"] or n != inst.get("tree_files"):
        return f"{name}: TREE MISMATCH ({n} files, {th})"
    return f"{name}: ok ({n} files)"


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
    print(verify(name, cfg))


def do_lock(name, cfg):
    root = install_dir(name, cfg)
    inst = cfg["installs"][name]
    inst["key_files"] = {f: sha256(root / f) for f in inst.get("key_files", {})}
    inst["tree_sha256"], inst["tree_files"] = tree_hash(root)
    CONFIG.write_text(json.dumps(cfg, indent=1) + "\n")
    print(f"locked {name}: {inst['tree_files']} files {inst['tree_sha256']}")


def main(argv):
    cfg = load()
    if "--install" in argv:
        do_install(argv[argv.index("--install") + 1], cfg)
    elif "--lock" in argv:
        do_lock(argv[argv.index("--lock") + 1], cfg)
    else:
        for name in cfg["installs"]:
            print(verify(name, cfg, quick="--quick" in argv))


if __name__ == "__main__":
    main(sys.argv)
