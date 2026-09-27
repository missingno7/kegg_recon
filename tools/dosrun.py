"""Run a historical tool from a pinned install, deterministically.

    python tools/dosrun.py [--install wc100] [--host nt] [--cwd DIR] TOOL [ARGS...]
    python tools/dosrun.py wcc386 -3s -od -s foo.c          # runs in the current directory

Library use:
    from dosrun import run
    r = run("wcc386", ["-3s", "foo.c"], cwd=work)   # -> Result(rc, out, cmd, host, tool_sha256, images)

Hosts
  nt      BINNT/<TOOL>.EXE.  For wcc386/wasm/wdisasm this is Watcom's generic Win32 loader stub which loads
          the bound BINB/<TOOL>.EXE image of the *same install* (hosts.<tool>.nt_payload; verified: the stub
          crashes without its BINB sibling).  BINNT/wlink.exe and BINNT/wlib.exe are native Win32 programs
          (verified: they run from an isolated directory).  Same compiler code as the DOS/4GW-hosted tool;
          host independence is checked by the dosbox host.
  msdos   MS-DOS Player (C:/tools/nmlgcdos/msdos.exe, pinned in toolchain.json "runners") for real-mode
          16-bit tools.  Cannot host DOS/4GW programs (verified: silent failure).
  dosbox  DOSBox-X (pinned in toolchain.json "runners"), DOS/4GW-hosted tools (independent cross-check path).

Every launched image is hash-checked against toolchain.json before it runs (fail closed: a launched file
without a locked SHA-256 is an error): the host executable, the loader-stub payload, the configured DOS/4GW
image (e.g. BIN/wlink.exe) and the external runner.  Result.tool_sha256 is the hash of the image holding the
tool's code (the payload for loader stubs); Result.images lists every checked file with its hash.

The environment is built from scratch: no inherited WCC386/WLINK/WPP386/INCLUDE
variables can leak in.  Output is stdout+stderr combined, exit status preserved.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "toolchain" / "toolchain.json"
MSDOS = "nmlgcdos/msdos.exe"
_hash_cache: dict = {}


def tools_root() -> Path:
    return Path(os.environ.get("KEGG_TOOLS", "C:/tools"))


def config():
    return json.loads(CONFIG.read_text())


@dataclass
class Result:
    rc: int
    out: str
    cmd: list
    host: str
    tool_sha256: str
    images: dict = field(default_factory=dict)   # every hash-checked launched file -> sha256

    @property
    def ok(self):
        return self.rc == 0


def _sha(p: Path):
    st = p.stat()
    key = (str(p), st.st_size, st.st_mtime_ns)
    if key not in _hash_cache:
        _hash_cache[key] = hashlib.sha256(p.read_bytes()).hexdigest()
    return _hash_cache[key]


def locked_digest(inst_cfg, rel: str):
    """The locked SHA-256 of an install file (exact key first, then case-insensitive: Windows file names)."""
    keys = inst_cfg.get("key_files", {})
    if rel in keys:
        return keys[rel]
    low = {k.lower(): v for k, v in keys.items()}
    return low.get(rel.lower())


def _check(inst_cfg, root: Path, rel: str):
    """Hash-check one install file against its lock; fail closed if it is missing or has no locked digest."""
    path = root / rel
    if not path.is_file():
        raise RuntimeError(f"pinned toolchain file is missing: {path}")
    want = locked_digest(inst_cfg, rel)
    if not want:
        raise RuntimeError(f"no locked SHA-256 for toolchain file {rel} ({root}); add it to toolchain.json key_files")
    got = _sha(path)
    if want.lower() != got:
        raise RuntimeError(f"toolchain file {path} hash {got} != locked {want}")
    return got


def runner(name: str, path: Path | None = None, cfg=None):
    """Path of a pinned external runner ('dosbox-x', 'msdos') after checking its SHA-256 (fail closed)."""
    cfg = cfg or config()
    r = cfg.get("runners", {}).get(name)
    if not r or not r.get("sha256"):
        raise RuntimeError(f"no pinned SHA-256 for external runner {name!r} in toolchain.json 'runners'")
    if path is None:
        path = Path(os.environ[r["env"]]) if r.get("env") and os.environ.get(r["env"]) else tools_root() / r["path"]
    path = Path(path)
    if not path.is_file():
        raise RuntimeError(f"external runner {name} is missing: {path}")
    got = _sha(path)
    if got != r["sha256"].lower():
        raise RuntimeError(f"external runner {name} {path} hash {got} != pinned {r['sha256']}")
    return path, got


def nt_images(inst_cfg, root: Path, tool: str):
    """Hash-check the NT host executable, its loader payload and the configured DOS/4GW image.
    Returns (tool_sha256, {rel: sha256})."""
    hosts = inst_cfg["hosts"][tool]
    stub = hosts["nt"]
    images = {stub: _check(inst_cfg, root, stub)}
    payload = hosts.get("nt_payload")
    if not payload:
        # fail closed: a generic loader stub (the same digest as another tool's stub with a payload) must name
        # the image it loads, else the code that actually runs would go unchecked
        loaders = {locked_digest(inst_cfg, h["nt"]) for h in inst_cfg["hosts"].values()
                   if h.get("nt") and h.get("nt_payload")}
        if images[stub] in {d.lower() for d in loaders if d}:
            raise RuntimeError(f"{stub} is a Watcom loader stub but hosts.{tool}.nt_payload is not configured")
    else:
        images[payload] = _check(inst_cfg, root, payload)
    image = hosts.get("dos4gw")
    if image and image not in images:
        images[image] = _check(inst_cfg, root, image)   # configured DOS/4GW image (e.g. BIN/wlink.exe)
    return images[payload] if payload else images[stub], images


def launch_identity(tool: str, install: str = "wc100", host: str = "nt", cfg=None):
    """{file: sha256} of everything `run(tool, host=host)` would launch, all checked against their locks."""
    cfg = cfg or config()
    inst = cfg["installs"][install]
    root = tools_root() / inst["dir"]
    hosts = inst["hosts"][tool]
    if host == "nt":
        return nt_images(inst, root, tool)[1]
    if host == "msdos":
        return {hosts["msdos"]: _check(inst, root, hosts["msdos"]), "runner:msdos": runner("msdos", cfg=cfg)[1]}
    if host == "dosbox":
        import dosbox
        image = hosts.get("dos4gw") or hosts.get("dos")
        if not image:
            raise ValueError(f"{tool} has no DOS/4GW image in toolchain/toolchain.json")
        return {image: _check(inst, root, image), "BIN/DOS4GW.EXE": _check(inst, root, "BIN/DOS4GW.EXE"),
                "runner:dosbox-x": runner("dosbox-x", dosbox.DOSBOX_X, cfg=cfg)[1]}
    raise ValueError(host)


def environment(inst_cfg, root: Path, cwd: Path):
    env = {k: os.environ[k] for k in ("SYSTEMROOT", "WINDIR", "COMSPEC") if k in os.environ}
    env["PATH"] = os.pathsep.join([str(root / "BINNT"), str(root / "BIN"),
                                   os.environ.get("SYSTEMROOT", "C:\\Windows") + "\\system32"])
    env["TEMP"] = env["TMP"] = str(cwd)
    for k, v in inst_cfg.get("env", {}).items():
        env[k] = str(root / v) if v else str(root)
    return env


def run(tool: str, args, install: str = "wc100", host: str = "nt", cwd=None, timeout=300) -> Result:
    cfg = config()
    inst = cfg["installs"][install]
    root = tools_root() / inst["dir"]
    cwd = Path(cwd or os.getcwd()).resolve()
    hosts = inst["hosts"][tool]
    if host == "nt":
        tool_sha, images = nt_images(inst, root, tool)
        cmd = [str(root / hosts["nt"]), *map(str, args)]
    elif host == "msdos":
        exe = hosts["msdos"]
        tool_sha = _check(inst, root, exe)
        msdos, msdos_sha = runner("msdos", cfg=cfg)
        images = {exe: tool_sha, "runner:msdos": msdos_sha}
        cmd = [str(msdos), "-e", str(root / exe), *map(str, args)]
    elif host == "dosbox":
        import dosbox
        images = launch_identity(tool, install, "dosbox", cfg)
        rc, out = dosbox.run_dosbox(tool, args, install=install, cwd=cwd, timeout=timeout)
        image = hosts.get("dos4gw") or hosts.get("dos")
        return Result(rc, out, ["dosbox", tool, *map(str, args)], host, images[image], images)
    else:
        raise ValueError(host)
    p = subprocess.run(cmd, cwd=cwd, env=environment(inst, root, cwd), stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=timeout)
    return Result(p.returncode, p.stdout.decode("latin-1"), cmd, host, tool_sha, images)


def main(argv):
    install, host, cwd = "wc100", "nt", None
    args = list(argv[1:])
    while args and args[0].startswith("--"):
        opt = args.pop(0)
        val = args.pop(0)
        if opt == "--install":
            install = val
        elif opt == "--host":
            host = val
        elif opt == "--cwd":
            cwd = val
        else:
            raise SystemExit(f"unknown option {opt}")
    r = run(args[0], args[1:], install=install, host=host, cwd=cwd)
    sys.stdout.write(r.out)
    return r.rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
