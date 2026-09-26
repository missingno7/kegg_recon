"""Run pinned DOS/4GW Watcom tools in an isolated DOSBox-X instance.

    python tools/dosbox.py [--install wc100a] [--cwd DIR] TOOL [ARGS...]

Library use:
    rc, out = run_dosbox("wcc386", ["-3s", "-od", "-s", "foo.c"], cwd=work)

The work directory is mounted as C:, the pinned Watcom install as D:. Each call
gets a private DOS batch/output name and a unique DOSBox-X configuration.
"""
from __future__ import annotations

import hashlib
import json
import os
import secrets
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "toolchain" / "toolchain.json"
DOSBOX_X = Path(os.environ.get("KEGG_DOSBOX_X", "C:/tools/dosbox-x/dosbox-x.exe"))
_hash_cache: dict[tuple[str, int, int], str] = {}


def tools_root() -> Path:
    return Path(os.environ.get("KEGG_TOOLS", "C:/tools"))


def config():
    return json.loads(CONFIG.read_text(encoding="utf-8"))


def _sha(path: Path) -> str:
    stat = path.stat()
    key = (str(path), stat.st_size, stat.st_mtime_ns)
    if key not in _hash_cache:
        _hash_cache[key] = hashlib.sha256(path.read_bytes()).hexdigest()
    return _hash_cache[key]


def _check(inst_cfg: dict, root: Path, rel: str) -> str:
    path = root / Path(rel)
    if not path.is_file():
        raise FileNotFoundError(f"pinned toolchain image is missing: {path}")
    keys = {k.lower(): v for k, v in inst_cfg.get("key_files", {}).items()}
    expected = keys.get(rel.lower())
    if not expected:
        raise RuntimeError(f"no locked SHA-256 for toolchain image {rel}")
    actual = _sha(path)
    if actual.lower() != expected.lower():
        raise RuntimeError(f"toolchain file {path} hash {actual} != locked {expected}")
    return actual


def _dos_quote(value: str) -> str:
    """Quote one DOS shell token, rejecting batch syntax and control chars."""
    if any(ch in value for ch in '\r\n"') or any(ord(ch) < 32 for ch in value):
        raise ValueError(f"argument cannot be represented safely in a DOS batch file: {value!r}")
    # Percent expansion and command separators remain active in DOS batch files,
    # including in quoted arguments. They are not part of the tool argv contract.
    if any(ch in value for ch in "%&|<>^"):
        raise ValueError(f"DOS batch metacharacter in argument: {value!r}")
    value = value.replace("/", "\\")
    if not value or any(ch.isspace() for ch in value):
        return f'"{value}"'
    return value


def _dos_arg(value, cwd: Path, root: Path) -> str:
    arg = os.fspath(value)
    # Map absolute host paths to the two mounted DOS volumes. Relative paths
    # are relative to C:\ (the requested working directory).
    p = Path(arg)
    if p.is_absolute():
        resolved = p.resolve()
        try:
            rel = resolved.relative_to(cwd)
            arg = "C:\\" + str(rel)
        except ValueError:
            try:
                rel = resolved.relative_to(root)
                arg = "D:\\" + str(rel)
            except ValueError as exc:
                raise ValueError(f"absolute argument is outside the mounted directories: {arg}") from exc
    return _dos_quote(arg)


def _host_environment() -> dict[str, str]:
    """Minimal Windows process environment; DOS tool variables are set in batch."""
    env = {k: os.environ[k] for k in ("SYSTEMROOT", "WINDIR", "TEMP", "TMP") if k in os.environ}
    system_root = env.get("SYSTEMROOT", env.get("WINDIR", r"C:\Windows"))
    runner_dir = str(DOSBOX_X.resolve().parent)
    env["PATH"] = os.pathsep.join([runner_dir, str(Path(system_root) / "System32")])
    env["SDL_VIDEODRIVER"] = "dummy"
    env["SDL_AUDIODRIVER"] = "dummy"
    return env


def _private_names(cwd: Path):
    """Reserve unique, DOS 8.3-compatible control filenames in C:\."""
    for _ in range(100):
        token = secrets.token_hex(3).upper()
        batch = cwd / f"R{token}.BAT"
        output = cwd / f"O{token}.TXT"
        status = cwd / f"E{token}.TXT"
        if any(p.exists() for p in (batch, output, status)):
            continue
        try:
            with batch.open("xb"):
                pass
        except FileExistsError:
            continue
        return batch, output, status
    raise RuntimeError(f"could not reserve unique DOSBox scratch names in {cwd}")


def run_dosbox(tool: str, args, install: str = "wc100", cwd=None, timeout=300) -> tuple[int, str]:
    """Run a DOS/4GW Watcom tool; return its DOS ERRORLEVEL and output text."""
    cfg = config()
    try:
        inst = cfg["installs"][install]
        hosts = inst["hosts"][tool]
    except KeyError as exc:
        raise ValueError(f"unknown install/tool combination: {install}/{tool}") from exc

    root = (tools_root() / inst["dir"]).resolve()
    work = Path(cwd or os.getcwd()).resolve()
    if not work.is_dir():
        raise NotADirectoryError(work)
    if not DOSBOX_X.is_file():
        raise FileNotFoundError(f"DOSBox-X executable is missing: {DOSBOX_X}")

    # These are the DOS/4GW images, not the Win32 loader stubs in BINNT.
    image_rel = hosts.get("dos4gw") or hosts.get("dos")
    if not image_rel:
        raise ValueError(f"{tool} has no DOS/4GW image in toolchain/toolchain.json")
    _check(inst, root, image_rel)
    _check(inst, root, "BIN/DOS4GW.EXE")

    batch, output_file, status_file = _private_names(work)
    dos_batch = f"C:\\{batch.name}"
    dos_output = f"C:\\{output_file.name}"
    dos_status = f"C:\\{status_file.name}"
    dos_exe = "D:\\" + image_rel.replace("/", "\\")
    command = " ".join([dos_exe, *(_dos_arg(arg, work, root) for arg in args)])
    batch_lines = [
        "@echo off",
        "set WATCOM=D:\\",
        r"set INCLUDE=D:\H",
        r"set PATH=D:\BINB;D:\BIN",
        f"{command} > {dos_output}",
    ]
    # The clean DOS PATH excludes DOSBox-X's Z: utilities, so capture the
    # executable's DOS 8-bit status directly with the shell's IF ERRORLEVEL.
    batch_lines.extend(f"if errorlevel {code} goto E{code}" for code in range(255, 0, -1))
    batch_lines.extend([f"echo 0 > {dos_status}", "goto DONE"])
    for code in range(255, 0, -1):
        batch_lines.extend([f":E{code}", f"echo {code} > {dos_status}", "goto DONE"])
    batch_lines.extend([":DONE", "exit"])
    batch.write_bytes(("\r\n".join(batch_lines) + "\r\n").encode("ascii"))

    config_lines = [
        "[sdl]",
        "output=surface",
        "[cpu]",
        "cycles=max",
        "[mixer]",
        "nosound=true",
        "[autoexec]",
        f'mount c "{work}"',
        f'mount d "{root}"',
        "c:",
        "cd " + chr(92),
        dos_batch,
    ]
    try:
        with tempfile.TemporaryDirectory(prefix="dosbox-") as temp_dir:
            conf = Path(temp_dir) / "dosbox.conf"
            conf.write_text("\n".join(config_lines) + "\n", encoding="ascii")
            cmd = [str(DOSBOX_X.resolve()), "-conf", str(conf), "-fastlaunch", "-exit"]
            kwargs = {}
            if os.name == "nt":
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
                kwargs["startupinfo"] = startup
                kwargs["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            proc = subprocess.run(cmd, cwd=work, env=_host_environment(), stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=timeout, **kwargs)
            box_output = proc.stdout.decode("latin-1", errors="replace")
            tool_output = output_file.read_text(encoding="latin-1") if output_file.is_file() else ""
            status_text = status_file.read_text(encoding="ascii", errors="replace") if status_file.is_file() else ""
            try:
                rc = int(status_text.strip())
            except ValueError:
                raise RuntimeError(
                    "DOSBox-X did not record DOS ERRORLEVEL; "
                    f"host rc={proc.returncode}, status={status_text!r}, console={box_output!r}"
                ) from None
            if not 0 <= rc <= 255:
                raise RuntimeError(f"DOSBox-X returned out-of-range DOS ERRORLEVEL {rc}: {status_text!r}")
            if proc.returncode != 0:
                raise RuntimeError(f"DOSBox-X host exited {proc.returncode}: {box_output}")
            return rc, tool_output
    finally:
        for path in (batch, output_file, status_file):
            try:
                path.unlink()
            except FileNotFoundError:
                pass


def main(argv) -> int:
    install, cwd, timeout = "wc100", None, 300
    args = list(argv[1:])
    while args and args[0].startswith("--"):
        opt = args.pop(0)
        if opt in ("--install", "--cwd", "--timeout"):
            if not args:
                raise SystemExit(f"{opt} requires a value")
            value = args.pop(0)
            if opt == "--install":
                install = value
            elif opt == "--cwd":
                cwd = value
            else:
                timeout = int(value)
        else:
            raise SystemExit(f"unknown option {opt}")
    if not args:
        raise SystemExit("usage: dosbox.py [--install NAME] [--cwd DIR] [--timeout SECONDS] TOOL [ARGS...]")
    rc, out = run_dosbox(args[0], args[1:], install=install, cwd=cwd, timeout=timeout)
    sys.stdout.write(out)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
