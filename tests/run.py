"""Verifier self-tests: negative controls must be rejected, positive controls accepted.

    python tests/run.py              # everything (about a minute)
    python tests/run.py --no-image   # skip the whole-image controls (scratch-tree build)

1. negative candidates (tests/neg): tools/check.py must exit 1 with a non-EXACT verdict;
2. positive controls: canonical sources recorded in manifest.json (smallest game-c unit, smallest host-pinned
   -ot DOSBox unit, smallest TASM unit, first obj2 IRQ function) must exit 0 with verdict EXACT;
3. whole-image controls, in an isolated scratch copy under build/tests/ (the canonical tree is never written):
   the unmodified copy links IDENTICAL; a tampered oracle (one flipped byte of assets/KE.EXE) is refused; a
   one-byte-effect source change (case of one letter in a unique string literal of a game-c unit, manifest
   hashes updated as a careless promotion would) makes `validate.py --image` exit 1 because the linked image
   differs.
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NEG = [  # (file, func, start, end)
    ("wrong_const.c", "f_7032", "0x7032", "0x704d"),
    ("wrong_width.c", "f_7032", "0x7032", "0x704d"),
    ("wrong_symbol.c", "f_7032", "0x7032", "0x704d"),
    ("wrong_call.c", "f_2250", "0x2250", "0x228e"),
    ("wrong_string.c", "f_ca6a", "0xca6a", "0xcac2"),
    ("wrong_libname.c", "f_ddb9", "0xddb9", "0xde21"),  # close() where the original calls malloc
    ("wrong_irq_vec.asm", "a_0", "0x0", "0x69", "--profile", "game-asm-tasm31", "--object", "2"),
    ("wrong_irq_call.asm", "a_0", "0x0", "0x69", "--profile", "game-asm-tasm31", "--object", "2"),
]
fails = 0


def report(ok, text):
    global fails
    print(("ok  " if ok else "FAIL") + "  " + text)
    fails += not ok


def first_line(p):
    return (p.stdout.strip().splitlines() or p.stderr.strip().splitlines() or ["(no output)"])[0]


def check(root, args, out):
    out.parent.mkdir(parents=True, exist_ok=True)
    if out.exists():
        out.unlink()
    p = subprocess.run([sys.executable, str(root / "tools" / "check.py"), *map(str, args), "--json", str(out)],
                       capture_output=True, text=True)
    res = json.loads(out.read_text()) if out.exists() else None
    return p, res


def unit_args(root, u):
    return [root / u["src"], "--all", "--at", u["start"], "--end", u["end"], "--profile", u.get("profile", "game-c"),
            *[a for pl in u.get("place", []) for a in ("--place", pl)]]


def negatives():
    for f, fn, s, e, *extra in NEG:
        out = ROOT / "build" / "tests" / (f + ".json")
        p, res = check(ROOT, [ROOT / "tests" / "neg" / f, fn, "--at", s, "--end", e, *extra], out)
        ok = p.returncode == 1 and res is not None and res["verdict"] != "EXACT"
        report(ok, f"neg {f}: rc={p.returncode} {first_line(p)}")


def positives(man):
    units = man.get("units", [])
    size = lambda u: int(u["end"], 16) - int(u["start"], 16)
    picks = []
    for prof in ("game-c", "game-c-ot-dos", "game-asm-tasm31"):
        cands = sorted((u for u in units if u.get("profile") == prof and size(u) > 0), key=lambda u: (size(u), u["id"]))
        if cands:
            picks.append((f"unit {cands[0]['id']} ({prof})", unit_args(ROOT, cands[0]), cands[0]["src"],
                          cands[0].get("src_sha256")))
        else:
            report(False, f"pos: no manifest unit with profile {prof}")
    fn = next((f for f in man["functions"] if f.get("status") == "matching" and not f.get("unit")
               and f.get("object", 1) == 2), None)
    if fn:
        picks.append((f"function {fn['name']} (obj2 {fn['profile']})",
                      [ROOT / fn["src"], fn["name"], "--at", fn["start"], "--end", fn["end"], "--profile", fn["profile"]],
                      fn["src"], fn.get("src_sha256")))
    else:
        report(False, "pos: no standalone obj2 function in the manifest")
    for label, args, src, want in picks:
        if hashlib.sha256((ROOT / src).read_bytes()).hexdigest() != want:
            report(False, f"pos {label}: {src} does not match its manifest hash")
            continue
        out = ROOT / "build" / "tests" / ("pos_" + re.sub(r"\W+", "_", label) + ".json")
        p, res = check(ROOT, args, out)
        ok = p.returncode == 0 and res is not None and res["verdict"] == "EXACT" and p.stdout.startswith("EXACT")
        report(ok, f"pos {label}: rc={p.returncode} {first_line(p)}")


def scratch_tree():
    base = ROOT / "build" / "tests"
    base.mkdir(parents=True, exist_ok=True)
    box = Path(tempfile.mkdtemp(prefix="imgneg-", dir=base))
    for d in ("src", "asm", "tools", "toolchain", "include"):
        if (ROOT / d).exists():
            shutil.copytree(ROOT / d, box / d, ignore=shutil.ignore_patterns("__pycache__"))
    shutil.copyfile(ROOT / "manifest.json", box / "manifest.json")
    (box / "assets").mkdir()
    shutil.copyfile(ROOT / "assets" / "KE.EXE", box / "assets" / "KE.EXE")
    shutil.copytree(ROOT / "build" / "pylib", box / "build" / "pylib")
    return box


STRING = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def pick_mutation(box, man):
    """(unit, new source bytes): flip the case of the first letter of a string literal that occurs once in a
    game-c unit (plain code, not a comment or preprocessor line) -> exactly one CONST byte changes."""
    for u in sorted(man.get("units", []), key=lambda u: int(u["start"], 16)):
        if u.get("profile") != "game-c" or u["start"] == u["end"]:
            continue
        text = (box / u["src"]).read_bytes().decode("latin-1")
        code = re.sub(r"/\*.*?\*/", lambda m: " " * len(m.group(0)), text, flags=re.S)
        code = re.sub(r"//[^\n]*", lambda m: " " * len(m.group(0)), code)
        for m in STRING.finditer(code):
            line = code[code.rfind("\n", 0, m.start()) + 1:m.start()]
            lit = m.group(1)
            k = next((i for i, c in enumerate(lit) if c.isalpha() and c.isascii()), None)
            if line.lstrip().startswith("#") or k is None or "\\" in lit[:k] or code.count(m.group(0)) != 1:
                continue
            pos = m.start(1) + k
            new = text[:pos] + text[pos].swapcase() + text[pos + 1:]
            return u, new.encode("latin-1"), f"{u['src']}: {m.group(0)[:40]} -> letter {k} case-flipped"
    return None, None, None


def image_controls(man):
    box = scratch_tree()
    keep = False
    try:
        py = sys.executable
        # control: the unmodified copy links IDENTICAL (so the negative below fails for the mutation alone)
        p = subprocess.run([py, str(box / "tools" / "image.py"), "--mode", "canonical"], capture_output=True,
                           text=True, cwd=box)
        report(p.returncode == 0 and "IDENTICAL" in first_line(p), f"image control (unmodified copy): {first_line(p)}")
        # tampered oracle: one flipped byte of assets/KE.EXE must be refused before any comparison
        ke = box / "assets" / "KE.EXE"
        orig = ke.read_bytes()
        ke.write_bytes(orig[:5000] + bytes([orig[5000] ^ 1]) + orig[5001:])
        p = subprocess.run([py, str(box / "tools" / "image.py"), "--mode", "canonical"], capture_output=True,
                           text=True, cwd=box)
        u0 = sorted((u for u in man["units"] if u.get("profile") == "game-c"), key=lambda u: u["id"])[0]
        q, _ = check(box, unit_args(box, u0), box / "build" / "tests" / "oracle.json")
        ok = p.returncode != 0 and "ORACLE MISMATCH" in p.stderr and q.returncode != 0 and "ORACLE MISMATCH" in q.stderr
        report(ok, f"oracle tamper refused: image.py rc={p.returncode}, check.py rc={q.returncode}")
        ke.write_bytes(orig)
        # one-byte-effect source change, recorded in the scratch manifest as if it had been promoted
        u, new, what = pick_mutation(box, man)
        if u is None:
            report(False, "image negative: no mutable string literal found in a game-c unit")
            keep = True
            return
        (box / u["src"]).write_bytes(new)
        h = hashlib.sha256(new).hexdigest()
        sm = json.loads((box / "manifest.json").read_text())
        for e in [*sm["units"], *sm["functions"]]:
            if e.get("src") == u["src"]:
                e["src_sha256"] = h
        (box / "manifest.json").write_text(json.dumps(sm, indent=1) + "\n")
        p = subprocess.run([py, str(box / "tools" / "validate.py"), "--quiet", "--image"], capture_output=True,
                           text=True, cwd=box)
        rep = json.loads((box / "build" / "image" / "canonical" / "report.json").read_text())
        linked = (box / "build" / "image" / "canonical" / "ke.exe").read_bytes()
        ndiff = sum(a != b for a, b in zip(linked, orig)) + abs(len(linked) - len(orig))
        ok = (p.returncode == 1 and "whole image not identical" in p.stdout and rep.get("identical") is False
              and rep.get("accounting", {}).get("total", {}).get("raw", 0) == 0 and ndiff > 0)
        report(ok, f"image negative ({what}): validate --image rc={p.returncode}, linked image differs in {ndiff} "
                   f"byte(s), raw {rep.get('accounting', {}).get('total', {}).get('raw', 0)}")
        keep = not ok
    finally:
        if keep or fails:
            print(f"  scratch tree kept for inspection: {box}")
        else:
            shutil.rmtree(box, ignore_errors=True)


def main():
    man = json.loads((ROOT / "manifest.json").read_text())
    negatives()
    positives(man)
    if "--no-image" not in sys.argv:
        image_controls(man)
    print(f"tests: {'FAILED (' + str(fails) + ')' if fails else 'all passed'}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
