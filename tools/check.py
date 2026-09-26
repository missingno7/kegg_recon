"""Verify a candidate C/ASM function (or contiguous run of functions) against KE.EXE.

    python tools/check.py CAND.c FUNC [--at 0x1234] [--end 0x1300] [--profile game-c] [--json OUT]
    python tools/check.py CAND.c --all --at 0x1234      # whole _TEXT of CAND vs original from 0x1234

FUNC is the symbol in the candidate object (public or static); the original extent comes from
manifest.json (entry with that name) unless --at/--end are given.  Output: one summary line plus
JSON (default build/check/FUNC.json) with the first differences and an aligned instruction diff.

Verdict EXACT requires, over the complete extent:
  * identical length and identical bytes outside fixup fields;
  * every candidate absolute fixup (off32) sits where the original has an LE fixup, and vice versa;
  * every relocated value resolves consistently: each candidate symbol / segment binds to exactly one
    original address, which must agree with manifest.json symbols/functions when known;
  * self-relative (call/jmp rel32) fixups: the original rel32 decodes to the bound target.
Nothing is masked for the verdict; normalisation is used only for the diagnostic alignment.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dosrun  # noqa: E402
import le as lemod  # noqa: E402
import omf  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
_ORIG = None


def original():
    global _ORIG
    if _ORIG is None:
        L = lemod.LE(ROOT / "assets" / "KE.EXE")
        code = L.object_bytes(L.objects[0])[:L.objects[0]["vsize"]]
        fix = {}
        for obj, off, typ, tgt in L.resolved_fixups():
            if obj == 1:
                fix[off] = (typ, tgt)
        _ORIG = (L, code, fix)
    return _ORIG


def manifest():
    return json.loads((ROOT / "manifest.json").read_text())


def known_symbols(man):
    """name -> 'obj:offset' string for every symbol with a fixed original address."""
    out = {}
    for f in man.get("functions", []):
        if f.get("name"):
            out[f["name"]] = f"1:{int(f['start'], 16):x}"
    for k, v in man.get("symbols", {}).items():
        out[k] = v
    return out


def compile_candidate(src: Path, profile: str, outdir: Path):
    cfg = dosrun.config()
    prof = cfg["profiles"][profile]
    outdir.mkdir(parents=True, exist_ok=True)
    obj = outdir / (src.stem + ".obj")
    if obj.exists():
        obj.unlink()
    tool = prof.get("tool", "wcc386")
    if tool == "wasm":
        args = [*prof["flags"], f"-fo={obj}", src.name]
    else:
        args = [*prof["flags"], f"-fo={obj}", f"-i={ROOT / 'include'}", src.name]
    r = dosrun.run(tool, args, install=prof["install"], cwd=src.parent)
    (outdir / (src.stem + ".log")).write_text(r.out)
    if r.rc != 0 or not obj.exists():
        raise SystemExit(f"COMPILE FAILED rc={r.rc}\n{r.out}")
    return obj, r


def code_segment(mod):
    segs = [i for i, s in enumerate(mod.segments) if s and s.cls.upper() == "CODE" and s.size]
    if len(segs) != 1:
        raise SystemExit(f"expected one non-empty CODE segment, got {[mod.segments[i].name for i in segs]}")
    return segs[0]


def symbol_extents(mod, si):
    """Sorted [(offset, name)] of all publics (incl. static) in segment si."""
    return sorted((o, n) for n, s, o, _ in mod.publics if s == si)


def resolve_fixup(mod, f, seg_si):
    """Return (key, addend) where key names what the candidate points at."""
    seg = mod.segments[f.seg]
    field = int.from_bytes(seg.data[f.off:f.off + 4], "little", signed=f.self_rel)
    m, d = f.target
    m &= 3
    if m == 2:
        key = mod.externs[d]
        # a local/public defined here resolves to its segment
        for n, s, o, _ in mod.publics:
            if n == key:
                key = f"{n}"
                break
    elif m == 0:
        key = "seg:" + mod.segments[d].name
    elif m == 1:
        key = "grp:" + mod.groups[d][0]
    else:
        key = f"?{f.target}"
    return key, f.disp + field


def compare(mod, si, c0, c1, a0, orig_len=None):
    L, code, ofix = original()
    cand = bytes(mod.segments[si].data[c0:c1])
    n = c1 - c0
    olen = orig_len if orig_len is not None else n
    orig = code[a0:a0 + olen]
    res = {"cand_range": [hex(c0), hex(c1)], "orig_range": [hex(a0), hex(a0 + olen)],
           "cand_size": n, "orig_size": olen, "problems": [], "bindings": {}, "byte_diffs": 0}
    probs = res["problems"]
    if n != olen:
        probs.append(f"size {n} != original {olen}")
    fix_sites = {}
    for f in mod.fixups:
        if f.seg == si and c0 <= f.off < c1:
            fix_sites[f.off - c0] = f
    masked = set()
    binds = res["bindings"]

    def bind(key, value, where):
        if key in binds and binds[key] != value:
            probs.append(f"binding conflict {key}: {binds[key]} vs {value} at +{where:#x}")
        binds.setdefault(key, value)

    for rel, f in sorted(fix_sites.items()):
        size = f.size()
        masked.update(range(rel, rel + size))
        key, addend = resolve_fixup(mod, f, si)
        if f.loc not in (9, 13):
            probs.append(f"unsupported candidate fixup {f.kind()} at +{rel:#x} -> {key}")
            continue
        if rel + 4 > olen:
            continue
        ofield = int.from_bytes(orig[rel:rel + 4], "little")
        osite = a0 + rel
        if f.self_rel:
            if osite in ofix:
                probs.append(f"original has LE fixup at self-relative site +{rel:#x}")
            tgt = (osite + 4 + (ofield - (1 << 32) if ofield & 0x80000000 else ofield)) & 0xFFFFFFFF
            bind(key, f"1:{(tgt - addend) & 0xFFFFFFFF:x}", rel)
        else:
            if osite not in ofix:
                probs.append(f"no original LE fixup at absolute site +{rel:#x} ({key})")
                continue
            typ, tgt = ofix[osite]
            if typ != "off32":
                probs.append(f"original fixup type {typ} at +{rel:#x}")
            tobj, toff = tgt.get("obj"), tgt.get("off", 0)
            if toff != ofield:
                probs.append(f"original field {ofield:#x} != LE target {toff:#x} at +{rel:#x}")
            bind(key, f"{tobj}:{(toff - addend) & 0xFFFFFFFF:x}", rel)
    for osite in ofix:
        if a0 <= osite < a0 + olen and (osite - a0) not in fix_sites:
            probs.append(f"original LE fixup at +{osite - a0:#x} has no candidate fixup")
    diffs = [i for i in range(min(n, olen)) if i not in masked and cand[i] != orig[i]]
    res["byte_diffs"] = len(diffs)
    if diffs:
        res["first_byte_diff"] = hex(diffs[0])
        probs.append(f"{len(diffs)} byte(s) differ, first at +{diffs[0]:#x} (orig {a0 + diffs[0]:#x})")
    return res, cand, orig, masked


def check_bindings(res, man, own_names):
    known = known_symbols(man)
    new = {}
    for k, v in res["bindings"].items():
        if k.startswith("seg:") or k.startswith("grp:"):
            new[k] = v
            continue
        if k in known:
            if known[k] != v:
                res["problems"].append(f"binding {k}={v} contradicts manifest {known[k]}")
        else:
            new[k] = v
    res["new_bindings"] = new


def disasm(buf, base, masked_rel):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out = []
    for ins in md.disasm(buf, base):
        rel = ins.address - base
        nb = "".join("xx" if rel + k in masked_rel else f"{b:02x}" for k, b in enumerate(ins.bytes))
        out.append({"a": ins.address, "b": ins.bytes.hex(), "n": nb, "t": f"{ins.mnemonic} {ins.op_str}".strip()})
    return out


def norm(i):
    return i["n"]


def instruction_diff(cand, orig, a0, masked):
    ci = disasm(cand, a0, masked)
    oi = disasm(orig, a0, masked)
    sm = difflib.SequenceMatcher(a=[norm(x) for x in oi], b=[norm(x) for x in ci], autojunk=False)
    ops, first = [], None
    eq = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            eq += i2 - i1
            continue
        if first is None:
            first = oi[i1]["a"] if i1 < len(oi) else a0 + len(orig)
        if len(ops) < 12:
            ops.append({"op": tag, "orig": [f"{x['a']:05x} {x['t']}" for x in oi[i1:i2][:6]],
                        "cand": [f"{x['t']}" for x in ci[j1:j2][:6]]})
    return {"orig_insns": len(oi), "cand_insns": len(ci), "equal_insns": eq,
            "first_diff_insn": hex(first) if first is not None else None, "islands": ops}


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("func", nargs="?")
    ap.add_argument("--at")
    ap.add_argument("--end")
    ap.add_argument("--all", action="store_true", help="compare the whole CODE segment")
    ap.add_argument("--profile", default="game-c")
    ap.add_argument("--json")
    ap.add_argument("--obj", help="use an existing object instead of compiling")
    a = ap.parse_args(argv[1:])
    src = Path(a.source).resolve()
    man = manifest()
    tag = a.func or src.stem
    outdir = ROOT / "build" / "check" / tag
    if a.obj:
        objp = Path(a.obj)
    else:
        objp, _ = compile_candidate(src, a.profile, outdir)
    mod = omf.load(objp)[0]
    si = code_segment(mod)
    ext = symbol_extents(mod, si)
    seg_end = mod.segments[si].size
    if a.all:
        c0, c1 = 0, seg_end
    else:
        names = [n for _, n in ext]
        if a.func not in names:
            raise SystemExit(f"{a.func} not defined in candidate; symbols: {names}")
        k = names.index(a.func)
        c0 = ext[k][0]
        c1 = ext[k + 1][0] if k + 1 < len(ext) else seg_end
    entry = next((f for f in man.get("functions", []) if f.get("name") == a.func), None)
    if a.at:
        a0 = int(a.at, 16)
        olen = int(a.end, 16) - a0 if a.end else None
    elif entry:
        a0 = int(entry["start"], 16)
        olen = int(entry["end"], 16) - a0
    else:
        raise SystemExit("original address unknown: give --at (and --end) or add the function to manifest.json")
    if a.all and olen is None:
        olen = c1 - c0
    res, cand, orig, masked = compare(mod, si, c0, c1, a0, olen)
    own = {n for n, *_ in mod.publics}
    check_bindings(res, man, own)
    # internal consistency: symbols defined in this object must sit at the matching offset
    for off, name in ext:
        if name in res["bindings"] and c0 <= off < c1:
            want = f"1:{a0 + off - c0:x}"
            if res["bindings"][name] != want:
                res["problems"].append(f"local symbol {name} bound to {res['bindings'][name]} but lies at {want}")
    res["verdict"] = "EXACT" if not res["problems"] else "DIFF"
    res["diff"] = instruction_diff(cand, orig, a0, masked) if res["verdict"] != "EXACT" else None
    res["source"] = str(src)
    res["source_sha256"] = hashlib.sha256(src.read_bytes()).hexdigest()
    res["profile"] = a.profile
    res["symbols"] = [{"name": n, "off": hex(o)} for o, n in ext]
    out = Path(a.json) if a.json else outdir / "result.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(res, indent=1))
    d = res["diff"]
    line = f"{res['verdict']} {tag} orig {res['orig_range'][0]}..{res['orig_range'][1]} size {res['cand_size']}/{res['orig_size']}"
    if d:
        line += f" insns {d['equal_insns']}/{d['orig_insns']} equal, first diff {d['first_diff_insn']}"
    print(line)
    for p in res["problems"][:8]:
        print("  -", p)
    if d:
        for isl in d["islands"][:4]:
            print(f"  [{isl['op']}] orig: {' | '.join(isl['orig'][:3])}")
            print(f"  {' ' * (len(isl['op']) + 2)} cand: {' | '.join(isl['cand'][:3])}")
    if res["new_bindings"]:
        print("  bindings:", ", ".join(f"{k}={v}" for k, v in list(res["new_bindings"].items())[:10]))
    print(f"  json: {out}")
    return 0 if res["verdict"] == "EXACT" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
