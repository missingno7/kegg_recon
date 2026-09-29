"""64-bit lockstep: the Android ILP32-world build against the Windows i686 port, frame by frame.

    python port/android/tools/lockstep64.py --frames 1100 --click-every 100:60:150:82:400 [--sound]
        [--serial emulator-5554] [--build-dir android/app/build/intermediates/cxx/Debug/<id>/obj/x86_64]

1. Runs the Windows port on the deterministic lockstep virtual PC
   (port/tools/lockstep.py --only port: build/port/oracle/ke_lockstep.exe --mode port).
2. Runs the same machine, input schedule and frame hook on the Android device/emulator with
   the 64-bit build (ke_lockstep_loader + libkelockstep.so, port/android/lockstep; built with
   gradlew assembleDebug -Pke.lockstep=1 -Pke.abis=x86_64).
3. Compares every frame's machine clock, PIC, PIT, VGA registers/DAC/planes, heap arena top,
   BIOS data area, sound/DMA state and the mapped DGROUP (the game's globals in the original
   layout; pointer values compare as tokens: object offset, function name, heap, VGA).
Output: build/port/lockstep64/report.txt ("NO DIVERGENCE in N frames" or the first frame).
"""
from __future__ import annotations

import argparse
import bisect
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "tools"))
import lockstep  # noqa: E402

NDK_BIN = Path(os.environ.get("LOCALAPPDATA", "")) / "Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin"
ADB = Path(os.environ.get("LOCALAPPDATA", "")) / "Android/Sdk/platform-tools/adb.exe"
REMOTE = "/data/local/tmp/kels"


def nm_symbols(lib: Path):
    """(relative address, kind, name) of every symbol of the unstripped Android library."""
    out = subprocess.run([str(NDK_BIN / "llvm-nm.exe"), str(lib)], capture_output=True, text=True,
                         check=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            syms.append((int(parts[0], 16), parts[1], parts[2]))
    return syms


def android_portmap(win_syms: lockstep.Symbols, spans, lib_syms):
    """Windows spans (orig_off, len, win_addr, name) -> library-relative Android addresses:
    each span is placed relative to the Windows data symbol that contains it, then moved to
    the same symbol in the Android library (identical per-unit layout, check_ilp32_layout.py)."""
    win_data = sorted((a, n.lstrip("_")) for a, k, n in win_syms.port_all if k in "DdBbRr")
    win_addrs = [a for a, _n in win_data]
    android = {}
    for addr, kind, name in lib_syms:
        if kind in "DdBbRr":
            android.setdefault(name.lstrip("_"), addr)
    result, missing = [], []
    for off, ln, waddr, name in spans:
        if name == "CONST":
            const = next(((a - int(n.rsplit("_", 1)[1], 16)) for n, a in android.items()
                          if n.startswith("ke_original_const3_")), None)
            if const is not None:
                result.append((off, ln, const + off, name))
                continue
        i = bisect.bisect_right(win_addrs, waddr) - 1
        anchor = win_data[i][1] if i >= 0 else None
        if anchor is None or anchor not in android:
            missing.append(name)
            continue
        result.append((off, ln, android[anchor] + (waddr - win_data[i][0]), name))
    return result, missing


class AndroidNormalizer:
    def __init__(self, spans_rel, base, lib_syms):
        self.spans = sorted((base + rel, off, ln, name) for off, ln, rel, name in spans_rel)
        self.addrs = [a for a, _o, _l, _n in self.spans]
        tmp = {}
        for addr, kind, name in lib_syms:
            if kind in "Tt" and not name.startswith("."):
                tmp.setdefault(base + addr, set()).add(name)
        self.fn = {k: frozenset(v) for k, v in tmp.items()}
        self.o2 = next((base + a for a, _k, n in lib_syms if n == "a_0"), None)
        self.pre = {base + rel - 8: off - 8 for off, _ln, rel, name in spans_rel
                    if name == "collision_animation_frames" and off >= 8}

    def port(self, v, dg=None):
        if 0xA0000 <= v < 0xC0000 or 0x280000 <= v < 0x300000:
            return ("vga", v)
        if self.o2 is not None and self.o2 <= v <= self.o2 + 0x149:
            return (2, v - self.o2)
        if v in self.pre:
            return (3, self.pre[v])
        i = bisect.bisect_right(self.addrs, v) - 1
        if i >= 0:
            addr, off, ln, _n = self.spans[i]
            if v <= addr + ln:
                return (3, off + (v - addr))
        if v in self.fn:
            return ("fn", self.fn[v])
        if 0x20000000 <= v < 0x26000000:
            return ("heap", v)
        return None


def adb(*args, check=True, **kw):
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    return subprocess.run([str(ADB), *args], check=check, env=env, capture_output=True, text=True, **kw)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, default=1100)
    ap.add_argument("--click-every", default="")
    ap.add_argument("--event", action="append")
    ap.add_argument("--idle-keys", default="39,b9")
    ap.add_argument("--replay", help="kegg_forged input JSON (as port/tools/lockstep.py)")
    ap.add_argument("--replay-offset", type=int, default=0)
    ap.add_argument("--sound", action="store_true")
    ap.add_argument("--serial")
    ap.add_argument("--build-dir", help="directory with ke_lockstep_loader and libkelockstep.so")
    ap.add_argument("--out", default=str(ROOT / "build/port/lockstep64"))
    ap.add_argument("--skip-windows", action="store_true")
    ap.add_argument("--in-app", action="store_true",
                    help="run the runner inside the installed debug app (needs adb root; for an arm64 "
                         "build under the emulator's ARM translation) instead of ke_lockstep_loader")
    ap.add_argument("--abi", default="x86_64")
    ap.add_argument("--max-report", type=int, default=40)
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if args.serial:
        os.environ["ANDROID_SERIAL"] = args.serial
    build = Path(args.build_dir) if args.build_dir else next(
        (p.parent for p in sorted((ROOT / "android/app/build/intermediates/cxx").rglob("libkelockstep.so"),
                                  key=lambda p: -p.stat().st_mtime) if args.abi in str(p)), None)
    if build is None:
        raise SystemExit("libkelockstep.so not found: gradlew assembleDebug -Pke.lockstep=1 -Pke.abis=x86_64")

    # 1. Windows port side
    if not args.skip_windows:
        cmd = [sys.executable, str(ROOT / "port/tools/lockstep.py"), "--only", "port", "--out", str(out),
               "--frames", str(args.frames), "--idle-keys", args.idle_keys]
        if args.click_every:
            cmd += ["--click-every", args.click_every]
        if args.replay:
            cmd += ["--replay", args.replay, "--replay-offset", str(args.replay_offset)]
        for e in args.event or []:
            cmd += ["--event", e]
        if args.sound:
            cmd.append("--sound")
        subprocess.run(cmd, check=True)
    exe = ROOT / "build/port/oracle/ke_lockstep.exe"
    win_syms = lockstep.Symbols(ROOT / "build/port/oracle", exe)
    spans = win_syms.portmap()
    lib = build / "libkelockstep.so"
    lib_syms = nm_symbols(lib)
    spans_a, missing = android_portmap(win_syms, spans, lib_syms)
    if missing:
        print(f"lockstep64: {len(missing)} spans without an Android anchor: {missing[:8]}")
    (out / "portmap64.txt").write_text("".join(f"{o:x} {l:x} {a:x} {n}\n" for o, l, a, n in spans_a))

    # 2. Android side
    if args.in_app:
        app = "/data/data/org.kryptonegg.port/files"
        rdir = app + "/ls"
        adb("shell", "rm", "-rf", rdir)
        adb("shell", "mkdir", "-p", rdir)
        for f in (out / "portmap64.txt", out / "lockstep.kereplay"):
            if f.exists():
                adb("push", str(f), rdir + "/")
        uid = adb("shell", "stat", "-c", "%u", app).stdout.strip()
        adb("shell", "chown", "-R", f"{uid}:{uid}", rdir)
        ke_args = ["--lockstep", "--data", app + "/game", "--out", rdir + "/port64.lsd",
                   "--portmap", rdir + "/portmap64.txt", "--frames", str(args.frames),
                   "--idle-keys", args.idle_keys.replace(",", ":"), "--log", rdir + "/port64.log"]
        if (out / "lockstep.kereplay").exists():
            ke_args += ["--replay", rdir + "/lockstep.kereplay"]
        if args.sound:
            ke_args += ["--sound", "--dma-capture", rdir + "/dma-capture-orig.bin"]
        adb("shell", "am", "force-stop", "org.kryptonegg.port")
        adb("logcat", "-c")
        adb("shell", "am", "start", "-W", "-n", "org.kryptonegg.port/.GameActivity",
            "--esa", "ke.args", ",".join(ke_args))
        import time
        deadline = time.time() + 3600
        while time.time() < deadline:
            time.sleep(5)
            if not adb("shell", "pidof", "org.kryptonegg.port", check=False).stdout.strip():
                break
        log = adb("logcat", "-d", "-s", "KryptonEgg:*", check=False).stdout
        (out / "port64.stdout").write_text(log)
        base = next((int(l.split("game library at ")[1].split(",")[0], 16) for l in log.splitlines()
                     if "game library at" in l), None)
        remote = rdir
    else:
        adb("shell", "rm", "-rf", REMOTE)
        adb("shell", "mkdir", "-p", REMOTE + "/data")
        sdl = build / "libSDL3.so"
        for f in (build / "ke_lockstep_loader", lib, sdl, out / "portmap64.txt", out / "lockstep.kereplay"):
            if f.exists():
                adb("push", str(f), REMOTE + "/")
        for f in sorted((ROOT / "assets").glob("KE_*")):
            adb("push", str(f), REMOTE + "/data/")
        adb("shell", "chmod", "755", REMOTE + "/ke_lockstep_loader")
        remote_cmd = (f"cd {REMOTE} && LD_LIBRARY_PATH={REMOTE} ./ke_lockstep_loader --data {REMOTE}/data "
                      f"--out {REMOTE}/port64.lsd --portmap {REMOTE}/portmap64.txt --frames {args.frames} "
                      f"--idle-keys {args.idle_keys} --log {REMOTE}/port64.log")
        if (out / "lockstep.kereplay").exists():
            remote_cmd += f" --replay {REMOTE}/lockstep.kereplay"
        if args.sound:
            remote_cmd += f" --sound --dma-capture {REMOTE}/dma-capture-orig.bin"
        r = adb("shell", remote_cmd, check=False, timeout=3600)
        (out / "port64.stdout").write_text(r.stdout + r.stderr)
        print("android:", (r.stdout.strip().splitlines() or ["(no output)"])[-1])
        base = next((int(l.split()[-1], 16) for l in r.stdout.splitlines() if l.startswith("lockstep: image base")), None)
        remote = REMOTE
    if base is None:
        raise SystemExit("lockstep64: the Android runner did not start; see port64.stdout / logcat")
    adb("pull", f"{remote}/port64.lsd", str(out / "port64.lsd"))
    adb("pull", f"{remote}/port64.log", str(out / "port64.log"), check=False)
    if args.sound:
        adb("pull", f"{remote}/dma-capture-orig.bin", str(out / "dma-capture-orig.bin"), check=False)

    # 3. compare: lockstep.compare() with the Android dump as the second side.
    # A Windows span runs to the next symbol, so the last variable of a unit's _DATA also
    # covers the COFF section padding that follows it; those bytes are not game state (the
    # 64-bit library places the next unit's data there). Compare each variable over its size.
    data_syms = sorted((a, n.lstrip("_")) for a, k, n in lib_syms if k in "DdBb")
    sym_size = {}
    for (a, n), (b, _m) in zip(data_syms, data_syms[1:]):
        if b > a:
            sym_size.setdefault(n, b - a)
    trimmed = []
    for o, l, a, n in spans:
        size = sym_size.get(n)
        if size and 0 < size < l and l - size < 4 and n != "CONST":
            print(f"lockstep64: comparing {n} over its {size} bytes (span {l} includes padding)")
            l = size
        trimmed.append((o, l, a, n))
    spans = trimmed
    class Args:
        pass
    cargs = Args()
    cargs.out, cargs.sound, cargs.max_report = str(out), args.sound, args.max_report
    sizes = {1: 0x1bcd1, 2: 0x149, 3: 0xf610}
    win_norm = lockstep.Normalizer(win_syms, {}, sizes, spans)
    and_norm = AndroidNormalizer(spans_a, base, lib_syms)
    orig_normalizer = lockstep.Normalizer
    # compare() tokenizes its first side with .port() and its second with .orig()
    class Bridge:
        def __init__(self, *_a, **_k):
            self.first = not hasattr(Bridge, "made")
            Bridge.made = True

        def port(self, v, dg=None):
            return win_norm.port(v, dg)

        def orig(self, v, dg=None):
            return and_norm.port(v, dg)
    lockstep.Normalizer = Bridge
    # copy_ds_to_es (port/asm/m_13a48_13a95.c) stores the host's DS selector on i386 (Windows:
    # 002Bh) and the virtual PC's flat selector 0170h on 64-bit hosts; the game never reads it.
    lockstep.KNOWN_TABLES = set(lockstep.KNOWN_TABLES) | {"saved_ds"}
    try:
        rc = lockstep.compare(cargs, win_syms, spans, out / "port.lsd", out / "port64.lsd", {}, {})
    finally:
        lockstep.Normalizer = orig_normalizer
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
