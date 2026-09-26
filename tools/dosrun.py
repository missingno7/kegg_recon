"""Run a historical tool from a pinned install, deterministically.

    python tools/dosrun.py [--install wc100] [--host nt] [--cwd DIR] TOOL [ARGS...]
    python tools/dosrun.py wcc386 -3s -od -s foo.c          # runs in the current directory

Library use:
    from dosrun import run
    r = run("wcc386", ["-3s", "foo.c"], cwd=work)   # -> Result(rc, out, cmd, host, tool_sha256)

Hosts
  nt      Watcom's own Win32 loader stub (BINNT/<TOOL>.EXE) which loads the bound
          BINB/<TOOL>.EXE image of the *same install* (verified: the stub opens its
          sibling ..\\BINB image and fails without it).  Same compiler code as the
          DOS/4GW-hosted tool; host independence is checked by the dosbox host.
  msdos   MS-DOS Player (C:/tools/nmlgcdos/msdos.exe) for real-mode 16-bit tools.
          Cannot host DOS/4GW programs (verified: silent failure).
  dosbox  DOSBox-X, DOS/4GW-hosted tools (independent cross-check path).

The environment is built from scratch: no inherited WCC386/WLINK/WPP386/INCLUDE
variables can leak in.  Output is stdout+stderr combined, exit status preserved.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from dataclasses import dataclass
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

    @property
    def ok(self):
        return self.rc == 0


def _sha(p: Path):
    st = p.stat()
    key = (str(p), st.st_size, st.st_mtime_ns)
    if key not in _hash_cache:
        _hash_cache[key] = hashlib.sha256(p.read_bytes()).hexdigest()
    return _hash_cache[key]


def _check(inst_cfg, root: Path, rel: str):
    want = inst_cfg.get("key_files", {}).get(rel)
    got = _sha(root / rel)
    if want and want != got:
        raise RuntimeError(f"toolchain file {root / rel} hash {got} != locked {want}")
    return got


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
        stub = hosts["nt"]
        image = hosts.get("dos4gw")
        _check(inst, root, stub)
        tool_sha = _check(inst, root, image) if image and image.startswith("BINB/") else _sha(root / stub)
        cmd = [str(root / stub), *map(str, args)]
    elif host == "msdos":
        exe = hosts["msdos"]
        tool_sha = _check(inst, root, exe)
        cmd = [str(tools_root() / MSDOS), "-e", str(root / exe), *map(str, args)]
    elif host == "dosbox":
        raise NotImplementedError("dosbox host: see tools/dosbox.py (cross-check path)")
    else:
        raise ValueError(host)
    p = subprocess.run(cmd, cwd=cwd, env=environment(inst, root, cwd), stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=timeout)
    return Result(p.returncode, p.stdout.decode("latin-1"), cmd, host, tool_sha)


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
