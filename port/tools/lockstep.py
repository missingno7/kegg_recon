"""Lockstep state diff: the port vs the ORIGINAL KE.EXE machine code on one virtual PC.

    python port/tools/lockstep.py [--frames N] [--replay PF.json] [--event F:M:x:y:b ...]
                                  [--event F:K:scan ...] [--idle-keys 39,b9]
                                  [--full-at a,b] [--out DIR] [--max-report 40] [--skip-run]

Runs build/port/oracle/ke_lockstep.exe twice (--mode port, --mode orig) with the same input
schedule (KEPORTREPLAY events keyed by the Nth wait_for_tick call, plus scripted keys for
blocking BIOS keyboard waits), then compares the per-frame dumps: machine clock, PIC, PIT,
VGA registers/DAC/plane hashes, heap, BIOS data area and DGROUP (the port's globals mapped to
their original object-3 offsets through manifest symbols; pointers normalized to
object/symbol-relative form). Prints the first differing frame, the differing symbols with
both values, then the first frame at which each further symbol diverges. docs/port/lockstep.md.
"""
from __future__ import annotations

import argparse
import bisect
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1].parent
sys.path.insert(0, str(Path(__file__).resolve().parent))

DG_SIZE = 0xE610
VGA_REGS = 108
PLANE_BLOCKS = 256
REC_HEAD = struct.Struct("<4sIQI10s2x16I")
REGS_NAMES = (["SEQ%02X" % i for i in range(8)] + ["GC%02X" % i for i in range(16)] +
              ["CRTC%02X" % i for i in range(32)] + ["ATTR%02X" % i for i in range(32)] +
              ["LATCH%d" % i for i in range(4)] +
              ["seq_index", "gc_index", "crtc_index", "attr_index", "attr_flipflop", "misc",
               "dac_write_index", "dac_read_index", "dac_component", "dac_read_component",
               "pel_mask", "bios_mode", "start_latched_lo", "start_latched_hi", "pad", "pad"])


def nm_tool() -> str:
    for cand in ("nm", r"C:\msys64\mingw32\bin\nm.exe"):
        if shutil.which(cand) or Path(cand).exists():
            return cand
    raise SystemExit("lockstep: nm not found (put C:/msys64/mingw32/bin on PATH)")


def objdump_tool() -> str:
    for cand in ("objdump", r"C:\msys64\mingw32\bin\objdump.exe"):
        if shutil.which(cand) or Path(cand).exists():
            return cand
    raise SystemExit("lockstep: objdump not found")


class Symbols:
    """Original object-3 symbols (manifest/ke_symbols) and the port's addresses (nm)."""

    def __init__(self, image_dir: Path, exe: Path):
        self.orig: list[tuple[int, str]] = []
        self.orig_by_obj: dict[int, list[tuple[int, str]]] = {1: [], 2: [], 3: []}
        for line in (image_dir / "ke_symbols.txt").read_text().splitlines():
            parts = line.split()
            if len(parts) == 3:
                obj, off = int(parts[1]), int(parts[2], 16)
                self.orig_by_obj.setdefault(obj, []).append((off, parts[0]))
        manifest = json.loads((ROOT / "manifest.json").read_text())
        for f in manifest["functions"]:
            self.orig_by_obj[1].append((int(f["start"], 16), f["name"]))
        for k in self.orig_by_obj:
            self.orig_by_obj[k] = sorted(set(self.orig_by_obj[k]))
        self.orig = [s for s in self.orig_by_obj[3] if s[0] < DG_SIZE]
        self.orig_offs = [o for o, _ in self.orig]
        self.exe = exe
        self.orig_name_obj = {}
        for obj, lst in self.orig_by_obj.items():
            for off, name in lst:
                self.orig_name_obj.setdefault(name, (obj, off))
        out = subprocess.run([nm_tool(), str(exe)], capture_output=True, text=True, check=True).stdout
        port: dict[str, tuple[int, str]] = {}
        dup: set[str] = set()
        self.port_all: list[tuple[int, str, str]] = []
        for line in out.splitlines():
            parts = line.split()
            if len(parts) != 3 or parts[1].upper() not in "DBRT":
                continue
            addr, kind, name = int(parts[0], 16), parts[1], parts[2]
            if name.startswith("_"):
                name = name[1:]
            self.port_all.append((addr, kind, name))
            if name in port and port[name][0] != addr:
                dup.add(name)
            port[name] = (addr, kind)
        for name in dup:
            port.pop(name, None)
        self.port = port
        self.port_all.sort()
        self.port_addrs = [a for a, _k, _n in self.port_all]

    def portmap(self) -> list[tuple[int, int, int, str]]:
        """(orig_off, len, port_addr, name): every global of the port's historical units and
        asm translations placed at its original DGROUP offset. Initialized data is packed
        per unit like Watcom's _DATA (check_layouts.py --data), so a unit's unnamed-in-the-
        manifest globals follow the nearest preceding manifest symbol of the same object;
        BSS order differs (work package D1), so only manifest-named BSS symbols are placed."""
        orig_off = {name: off for off, name in self.orig}
        build = self.exe.parent.parent
        objs = sorted(build.glob("CMakeFiles/ke_game.dir/**/*.obj"))
        objs += sorted(build.glob("CMakeFiles/ke_lockstep.dir/asm/*.obj"))
        objs += sorted(build.glob("CMakeFiles/ke_lockstep.dir/stubs/*.obj"))
        spans: dict[str, tuple[int, int, int, str]] = {}
        for obj in objs:
            sizes = {}
            head = subprocess.run([objdump_tool(), "-h", str(obj)], capture_output=True, text=True).stdout
            for line in head.splitlines():
                parts = line.split()
                if len(parts) >= 3 and parts[1] in (".data", ".bss"):
                    sizes[parts[1]] = int(parts[2], 16)
            text = subprocess.run([nm_tool(), "-n", str(obj)], capture_output=True, text=True).stdout
            syms = {".data": [], ".bss": []}
            for line in text.splitlines():
                parts = line.split()
                if len(parts) == 3 and parts[1] in "DdBb" and parts[2].startswith("_"):
                    sec = ".data" if parts[1] in "Dd" else ".bss"
                    syms[sec].append((int(parts[0], 16), parts[2][1:], parts[1]))
            for sec, lst in syms.items():
                lst.sort()
                base = None      # (orig - objoff) of the nearest preceding anchor
                anchor_exe = None  # (exe - objoff)
                for k, (objoff, name, kind) in enumerate(lst):
                    nxt = lst[k + 1][0] if k + 1 < len(lst) else sizes.get(sec, objoff)
                    length = nxt - objoff
                    exe = self.port.get(name)
                    if name in orig_off and exe and kind in "DB":
                        base = orig_off[name] - objoff
                        anchor_exe = exe[0] - objoff
                    if sec == ".bss" and name not in orig_off:
                        continue
                    if base is None or anchor_exe is None or length <= 0:
                        continue
                    o = base + objoff
                    if name in orig_off:
                        o = orig_off[name]
                        i = bisect.bisect_right(self.orig_offs, o)
                        nxt_orig = self.orig_offs[i] if i < len(self.orig_offs) else DG_SIZE
                        if sec == ".bss":
                            length = min(length, nxt_orig - o)
                    if 0 <= o and o + length <= DG_SIZE and name not in spans:
                        spans[name] = (o, length, anchor_exe + objoff if sec == ".data" or name not in self.port
                                       else self.port[name][0], name)
        out = sorted(spans.values())
        # drop overlaps (keep the first claim of each original byte)
        result, end = [], 0
        for o, l, a, n in out:
            if o < end:
                cut = end - o
                if cut >= l:
                    continue
                o, l, a = o + cut, l - cut, a + cut
            result.append((o, l, a, n))
            end = o + l
        return result

    def orig_symbol(self, off: int) -> tuple[str, int]:
        lst = self.orig
        i = bisect.bisect_right([o for o, _ in lst], off) - 1
        return (lst[i][1], off - lst[i][0]) if i >= 0 else ("?", off)


CONST_END = 0x25EC     # CONST/CONST2 (string literals) precede the first unit's _DATA


class PEImage:
    """Static bytes of the port executable's sections (string literals in .rdata)."""

    def __init__(self, path: Path):
        data = path.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        nsec = struct.unpack_from("<H", data, pe + 6)[0]
        opt = struct.unpack_from("<H", data, pe + 20)[0]
        base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
        self.sections = []
        for i in range(nsec):
            o = pe + 24 + opt + 40 * i
            name = data[o:o + 8].rstrip(b"\0").decode("latin1")
            vsize, va, rsize, raw = struct.unpack_from("<IIII", data, o + 8)
            self.sections.append((name, base + va, vsize, data[raw:raw + min(rsize, vsize)]))

    def cstring(self, addr: int):
        for name, va, vsize, blob in self.sections:
            if va <= addr < va + len(blob) and name in (".rdata", ".data"):
                off = addr - va
                end = blob.find(b"\0", off, off + 256)
                return bytes(blob[off:end if end >= 0 else off + 256])
        return None


class Normalizer:
    """Map a 32-bit value to a canonical pointer token, or None if not a known pointer.
    Pointers to string literals compare by content (their CONST layout is G2's work)."""

    def __init__(self, syms: Symbols, bases: dict[int, int], sizes: dict[int, int],
                 spans: list[tuple[int, int, int, str]]):
        self.syms, self.bases, self.sizes = syms, bases, sizes
        self.port_spans = sorted((addr, off, ln) for off, ln, addr, _ in spans)
        self.port_span_addrs = [a for a, _, _ in self.port_spans]
        # port text symbol -> original object 1 offset by name
        self.orig_funcs = {name: off for off, name in syms.orig_by_obj[1]}
        self.orig_o2 = {name: off for off, name in syms.orig_by_obj.get(2, [])}
        self.pe = PEImage(syms.exe)
        self.orig_fn_names: dict[int, frozenset] = {}
        tmp: dict[int, set] = {}
        for off, name in syms.orig_by_obj[1]:
            tmp.setdefault(off, set()).add(name)
        self.orig_fn_names = {k: frozenset(v) for k, v in tmp.items()}
        tmp = {}
        for addr, kind, name in syms.port_all:
            if kind in "Tt" and not name.startswith("."):
                tmp.setdefault(addr, set()).add(name)
        self.port_fn_names = {k: frozenset(v) for k, v in tmp.items()}
        self.o2_base = syms.port.get("a_0", (None,))[0]
        self.vga_alias = bases.get("vga", 0)

    def port_span_contains(self, v: int) -> bool:
        i = bisect.bisect_right(self.port_span_addrs, v) - 1
        return i >= 0 and v < self.port_spans[i][0] + self.port_spans[i][2]

    def orig(self, v: int, dg: bytes | None = None):
        a = self.vga_alias
        if a and a <= v < a + 0x20000:
            return ("vga", v - a + 0xA0000)
        if a and 4 * a <= v < 4 * a + 0x80000:
            return ("vga", (v - 4 * a) + 0x280000)
        for obj in (1, 2, 3):
            b = self.bases.get(obj)
            if b and b <= v <= b + self.sizes[obj] + (0x1000 if obj != 2 else 0):
                off = v - b
                if obj == 1 and off in self.orig_fn_names:
                    return ("fn", self.orig_fn_names[off])
                if obj == 3 and off < CONST_END and dg is not None:
                    end = dg.find(bytes(1), off, off + 256)
                    return ("str", bytes(dg[off:end if end >= 0 else off + 256]))
                return (obj, off)
        if 0x20000000 <= v < 0x26000000:
            return ("heap", v)
        return None

    def port(self, v: int):
        if 0xA0000 <= v < 0xC0000 or 0x280000 <= v < 0x300000:
            return ("vga", v)
        if self.o2_base is not None and self.o2_base <= v <= self.o2_base + 0x149:
            return (2, v - self.o2_base)
        i = bisect.bisect_right(self.port_span_addrs, v) - 1
        if i >= 0:
            addr, off, ln = self.port_spans[i]
            if v <= addr + ln:
                return (3, off + (v - addr))
        if v in self.port_fn_names:
            return ("fn", self.port_fn_names[v])
        if self.pe is not None:
            text = self.pe.cstring(v)
            if text is not None:
                return ("str", text)
        if 0x20000000 <= v < 0x26000000:
            return ("heap", v)
        return None


def tokens_equal(a, b) -> bool:
    if a is None or b is None:
        return False
    if a[0] == "fn" and b[0] == "fn":
        return bool(a[1] & b[1])
    return a == b


def read_dump(path: Path):
    data = path.read_bytes()
    if data[:4] != b"LSD1":
        raise SystemExit(f"{path}: not a lockstep dump")
    size = struct.unpack_from("<I", data, 4)[0]
    mask = data[8:8 + size]
    pos = 8 + size
    frames = []
    while pos + REC_HEAD.size <= len(data):
        head = REC_HEAD.unpack_from(data, pos)
        if head[0] != b"LSF1":
            break
        p = pos + REC_HEAD.size
        rec = {"frame": head[1], "clock": head[2], "wft": head[3], "pic": head[4],
               "pit": head[5:21]}
        rec["regs"] = data[p:p + VGA_REGS]; p += VGA_REGS
        rec["dac"] = data[p:p + 768]; p += 768
        rec["hash"] = struct.unpack_from("<256I", data, p); p += 4 * PLANE_BLOCKS
        rec["heap_top"], rec["heap_hash"] = struct.unpack_from("<II", data, p); p += 8
        rec["bda"] = data[p:p + 0x100]; p += 0x100
        full = data[p]; p += 4
        if full:
            rec["planes"] = data[p:p + 0x40000]; p += 0x40000
        rec["dg"] = data[p:p + size]; p += size
        if p > len(data):
            break
        frames.append(rec)
        pos = p
    return mask, frames


def run_side(mode: str, exe: Path, out: Path, args, replay: Path | None, portmap: Path | None):
    dump = out / f"{mode}.lsd"
    log = out / f"{mode}.log"
    cmd = [str(exe), "--mode", mode, "--image", str(ROOT / "build/port/oracle"),
           "--data", str(ROOT / "assets"), "--out", str(dump), "--log", str(log),
           "--frames", str(args.frames), "--idle-keys", args.idle_keys]
    if replay:
        cmd += ["--replay", str(replay)]
    if portmap:
        cmd += ["--portmap", str(portmap)]
    if args.full_at:
        cmd += ["--full-at", args.full_at]
    env = dict(os.environ)
    home = out / f"home-{mode}"
    shutil.rmtree(home, ignore_errors=True)
    home.mkdir(parents=True)
    env["LOCALAPPDATA"] = str(home)      # fresh high-score directory per run
    for k in ("KE_SB", "KE_JOY", "KE_WINDOWS", "KE_IRQ", "KE_DOSENV"):
        env.pop(k, None)
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=args.timeout)
    (out / f"{mode}.stdout").write_text(proc.stdout + proc.stderr, encoding="utf-8")
    bases = {}
    for line in proc.stdout.splitlines():
        if line.startswith("oracle: KE.EXE objects at"):
            parts = line.split()
            bases.update({1: int(parts[4], 16), 2: int(parts[5], 16), 3: int(parts[6].rstrip(","), 16)})
        if line.startswith("lockstep: vga alias"):
            bases["vga"] = int(line.split()[-1], 16)
    status = [l for l in proc.stdout.splitlines() if l.startswith("lockstep:")]
    print(f"{mode}: exit {proc.returncode}; {status[-1] if status else 'no status line'}")
    return dump, bases


def build_replay(args, out: Path) -> Path | None:
    events: list[tuple[int, int, str, tuple[int, ...]]] = []
    frames = 0
    if args.replay:
        from smoke import convert_replay
        tmp = out / "pf.kereplay"
        convert_replay(Path(args.replay), tmp)
        lines = tmp.read_text().splitlines()
        for seq, line in enumerate(lines[1:]):
            parts = line.split()
            events.append((int(parts[1]) + args.replay_offset, seq, parts[0],
                           tuple(int(x) for x in parts[2:])))
    if args.click_every:
        first, step, x, y = (int(v) for v in args.click_every.split(":"))
        for f in range(first, args.frames, step):
            args.event = (args.event or []) + [f"{f}:M:{2 * x}:{2 * y}:1", f"{f + 5}:M:{2 * x}:{2 * y}:0"]
    for seq, spec in enumerate(args.event or []):
        parts = spec.split(":")
        frame, kind = int(parts[0]), parts[1].upper()
        vals = tuple(int(x, 16) if kind == "K" else int(x) for x in parts[2:])
        events.append((frame, 1000000 + seq, kind, vals))
    if not events:
        return None
    events.sort(key=lambda e: (e[0], e[1]))
    frames = max(e[0] for e in events) + 1
    path = out / "lockstep.kereplay"
    with path.open("w", newline="\n") as f:
        f.write(f"KEPORTREPLAY 1 {len(events)} {frames}\n")
        for occ, _s, kind, vals in events:
            f.write(f"{kind} {occ} " + " ".join(map(str, vals)) + "\n")
    return path


def fmt_bytes(b: bytes, n: int = 24) -> str:
    s = b[:n].hex(" ")
    return s + (" ..." if len(b) > n else "")


def value_repr(b: bytes) -> str:
    if len(b) == 1:
        return f"{b[0]:#04x} ({b[0]})"
    if len(b) == 2:
        v = struct.unpack("<H", b)[0]
        return f"{v:#06x} ({struct.unpack('<h', b)[0]})"
    if len(b) == 4:
        v = struct.unpack("<I", b)[0]
        return f"{v:#010x} ({struct.unpack('<i', b)[0]})"
    return fmt_bytes(b)


# Translation-internal tables: the assembly modules' jump/dispatch tables and the unused
# ProTracker channel table hold original code/data addresses; the C translations call the
# targets directly (no C unit references them: grep src/). Not machine state that matters.
KNOWN_TABLES = {"sprite_operation_dispatch_table", "sprite_render_mode_table",
                "module_channel_state_table"}


def dg_diffs(syms, spans, mask, a, b, norm_p: Normalizer, norm_o: Normalizer):
    """Symbols whose mapped bytes differ (port a, orig b) after pointer normalization."""
    out = []
    for off, ln, _addr, name in spans:
        if name in KNOWN_TABLES:
            continue
        pa, ob = a[off:off + ln], b[off:off + ln]
        if pa == ob:
            continue
        diff = [i for i in range(ln) if pa[i] != ob[i]]
        same = set()
        for i in diff:
            for k in range(max(0, i - 3), min(i, ln - 4) + 1):
                if k in same:
                    continue
                vp = struct.unpack_from("<I", pa, k)[0]
                vo = struct.unpack_from("<I", ob, k)[0]
                tp, to = norm_p.port(vp), norm_o.orig(vo, b)
                if tokens_equal(tp, to):
                    same.update(range(k, k + 4))
        left = [i for i in diff if i not in same]
        if left:
            out.append((off, ln, name, left, pa, ob))
    return out


def compare(args, syms: Symbols, spans, port_dump: Path, orig_dump: Path, bases_p, bases_o):
    mask_p, fp = read_dump(port_dump)
    _mask_o, fo = read_dump(orig_dump)
    sizes = {1: 0x1bcd1, 2: 0x149, 3: 0xf610}
    norm_p = Normalizer(syms, bases_p, sizes, spans)
    norm_o = Normalizer(syms, bases_o, sizes, spans)
    n = min(len(fp), len(fo))
    print(f"frames dumped: port {len(fp)}, orig {len(fo)}; comparing {n}")
    first_seen: dict[str, tuple[int, str]] = {}
    first_frame = None
    report_lines: list[str] = []
    for i in range(n):
        p, o = fp[i], fo[i]
        items: list[tuple[str, str]] = []
        if p["clock"] != o["clock"]:
            items.append(("machine.clock_ns", f"port {p['clock']} orig {o['clock']} (delta {p['clock'] - o['clock']})"))
        if p["pic"] != o["pic"]:
            items.append(("pic", f"port {p['pic'].hex(' ')} orig {o['pic'].hex(' ')}"))
        if p["pit"] != o["pit"]:
            items.append(("pit", f"port {list(p['pit'][:14])} orig {list(o['pit'][:14])}"))
        for k in range(VGA_REGS):
            if p["regs"][k] != o["regs"][k]:
                items.append((f"vga.{REGS_NAMES[k]}", f"port {p['regs'][k]:#04x} orig {o['regs'][k]:#04x}"))
        if p["dac"] != o["dac"]:
            idx = [j // 3 for j in range(768) if p["dac"][j] != o["dac"][j]]
            items.append(("vga.dac", f"{len(set(idx))} entries differ, first {idx[0]}: port "
                          f"{p['dac'][idx[0]*3:idx[0]*3+3].hex(' ')} orig {o['dac'][idx[0]*3:idx[0]*3+3].hex(' ')}"))
        if p["hash"] != o["hash"]:
            blocks = [j for j in range(PLANE_BLOCKS) if p["hash"][j] != o["hash"][j]]
            desc = ", ".join(f"plane{j // 64}+{(j % 64) * 1024:#06x}" for j in blocks[:8])
            items.append(("vga.planes", f"{len(blocks)} KiB blocks differ: {desc}{' ...' if len(blocks) > 8 else ''}"))
        if (p["heap_top"], p["heap_hash"]) != (o["heap_top"], o["heap_hash"]):
            items.append(("heap", f"port top {p['heap_top']:#x} hash {p['heap_hash']:08x} orig top {o['heap_top']:#x} hash {o['heap_hash']:08x}"))
        if p["bda"] != o["bda"]:
            j = next(k for k in range(256) if p["bda"][k] != o["bda"][k])
            items.append(("bios_data_area", f"first at {0x400 + j:#x}: port {p['bda'][j]:#04x} orig {o['bda'][j]:#04x}"))
        for off, ln, name, left, pa, ob in dg_diffs(syms, spans, mask_p, p["dg"], o["dg"], norm_p, norm_o):
            first = left[0]
            if ln <= 4:
                val = f"port {value_repr(pa)} orig {value_repr(ob)}"
            else:
                s = max(0, first - 4)
                val = (f"+{first:#x} ({len(left)} bytes): port {fmt_bytes(pa[s:s+16])} | "
                       f"orig {fmt_bytes(ob[s:s+16])}")
            items.append((f"D:{off:04X} {name}[{ln}]", val))
        if items and first_frame is None:
            first_frame = i
            report_lines.append(f"FIRST DIVERGENCE at frame {p['frame']} (wait_for_tick entry #{p['frame']}, "
                                f"clock port {p['clock']} ns / orig {o['clock']} ns):")
            for key, val in items[:args.max_report]:
                report_lines.append(f"  {key}: {val}")
            if len(items) > args.max_report:
                report_lines.append(f"  ... {len(items) - args.max_report} more")
        for key, val in items:
            base = key.split("[")[0]
            if base not in first_seen:
                first_seen[base] = (p["frame"], val)
    if first_frame is None:
        report_lines.append(f"NO DIVERGENCE in {n} frames")
    else:
        report_lines.append("")
        report_lines.append("first frame of each diverging item (in order):")
        for key, (frame, val) in sorted(first_seen.items(), key=lambda kv: kv[1][0])[:args.max_report * 3]:
            report_lines.append(f"  frame {frame:5d}  {key}: {val}")
    text = "\n".join(report_lines)
    print(text)
    (Path(args.out) / "report.txt").write_text(text + "\n", encoding="utf-8")
    return 0 if first_frame is None else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, default=300)
    ap.add_argument("--replay", help="kegg_forged input JSON (portforge-dos-input-script-v1 / replay-v2)")
    ap.add_argument("--replay-offset", type=int, default=0, help="frame offset added to the JSON's occurrences")
    ap.add_argument("--event", action="append", help="F:M:x:y:buttons (DOS mouse coords) or F:K:hexscan")
    ap.add_argument("--click-every", default="", help="FIRST:STEP:X:Y - left clicks at game raster X,Y "
                    "(held 5 frames) every STEP frames from FIRST until --frames")
    ap.add_argument("--idle-keys", default="39,b9", help="hex scancodes for blocking BIOS keyboard waits")
    ap.add_argument("--full-at", default="", help="frames whose full VGA planes are dumped")
    ap.add_argument("--out", default=str(ROOT / "build/port/lockstep"))
    ap.add_argument("--exe", default=str(ROOT / "build/port/oracle/ke_lockstep.exe"))
    ap.add_argument("--max-report", type=int, default=40)
    ap.add_argument("--timeout", type=int, default=1800)
    ap.add_argument("--skip-run", action="store_true", help="only compare existing dumps in --out")
    ap.add_argument("--only", choices=("port", "orig"), help="run one side only")
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    exe = Path(args.exe)
    syms = Symbols(ROOT / "build/port/oracle", exe)
    spans = syms.portmap()
    portmap = out / "portmap.txt"
    portmap.write_text("".join(f"{o:x} {l:x} {a:x} {n}\n" for o, l, a, n in spans))
    replay = build_replay(args, out)
    bases_file = out / "bases.json"
    if not args.skip_run:
        bases = {}
        for mode in ("port", "orig"):
            if args.only and args.only != mode:
                continue
            _dump, b = run_side(mode, exe, out, args, replay, portmap if mode == "port" else None)
            bases[mode] = b
        old = json.loads(bases_file.read_text()) if bases_file.exists() else {}
        old.update({k: {str(o): v for o, v in b.items()} for k, b in bases.items()})
        bases_file.write_text(json.dumps(old))
        if args.only:
            return 0
    bases = json.loads(bases_file.read_text())
    conv = lambda d: {(int(k) if k.isdigit() else k): v for k, v in d.items()}
    return compare(args, syms, spans, out / "port.lsd", out / "orig.lsd",
                   conv(bases.get("port", {})), conv(bases.get("orig", {})))


if __name__ == "__main__":
    raise SystemExit(main())
