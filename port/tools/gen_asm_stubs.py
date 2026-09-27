"""Generate link stubs for every public symbol of the historical TASM modules.

    python port/tools/gen_asm_stubs.py            # (re)write port/stubs/asm_data.S + asm_stubs.c
    python port/tools/gen_asm_stubs.py --check    # exit 1 if the committed files are stale

Per module (asm/m_XXXXX_YYYYY.asm):
  * _DATA is re-emitted byte for byte (DB/DW/DD, DUP, EQU constants, label addresses) as one
    GNU-as blob with every PUBLIC data label aliased at its original offset, so adjacency and
    initial values are the historical ones.
  * every PUBLIC code label (PROC / LABEL NEAR in _TEXT) becomes a C stub that logs its name
    once (ke_stub_hit) and returns 0.
  * data declared inside _TEXT (tables) is emitted the same way as _DATA.
irq.asm (LE object 2, USE16 real-mode IRQ templates) is emitted as one 0x149-byte zero blob with
the object-2 offsets of its publics: the templates are copied into DOS memory and patched by C
code, but real mode never executes in the virtual PC (docs/port/architecture.md).

A module is OWNED by a translation once port/asm/<module stem>.c exists: the generator then emits
nothing for it (that file must define the module's code and data). This is how work packages
replace stubs without editing generated files.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASM = ROOT / "asm"
OUT_S = ROOT / "port" / "stubs" / "asm_data.S"
OUT_C = ROOT / "port" / "stubs" / "asm_stubs.c"
OWNED = ROOT / "port" / "asm"

IRQ_OBJECT2 = {"a_0": 0x0, "a_70": 0x70, "a_e0": 0xE0, "o2_e0": 0xE0, "o2_103": 0x103,
               "keyboard_irq_entry": 0x110, "irq_110": 0x110}
IRQ_OBJECT2_SIZE = 0x149

NUM = re.compile(r"^[0-9][0-9A-Fa-f]*[hH]$|^[0-9]+$")


def strip_comment(line: str) -> str:
    out, q = [], None
    for ch in line:
        if q:
            out.append(ch)
            if ch == q:
                q = None
        elif ch in "'\"":
            q = ch
            out.append(ch)
        elif ch == ";":
            break
        else:
            out.append(ch)
    return "".join(out).rstrip()


def parse_num(tok: str, eq: dict) -> int | None:
    tok = tok.strip()
    if NUM.match(tok):
        return int(tok[:-1], 16) if tok[-1] in "hH" else int(tok)
    if tok in eq:
        return eq[tok]
    try:  # simple expressions over EQU names (e.g. A*B, A+1)
        expr = re.sub(r"\b([0-9][0-9A-Fa-f]*)[hH]\b", lambda m: str(int(m.group(1), 16)), tok)
        expr = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(eq[m.group(0)]), expr)
        if re.fullmatch(r"[0-9+\-*/() ]+", expr):
            return int(eval(expr))  # noqa: S307 - digits and operators only
    except (KeyError, SyntaxError, ValueError):
        pass
    return None


def split_operands(s: str) -> list[str]:
    parts, cur, depth, q = [], [], 0, None
    for ch in s:
        if q:
            cur.append(ch)
            if ch == q:
                q = None
            continue
        if ch in "'\"":
            q = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
            continue
        cur.append(ch)
    if cur:
        parts.append("".join(cur).strip())
    return parts


SIZE = {"DB": 1, "DW": 2, "DD": 4, "DF": 6, "DQ": 8}
DIRECTIVE = {1: ".byte", 2: ".short", 4: ".long"}


def emit_items(kind: str, operands: str, eq: dict, notes: list) -> tuple[list[str], int]:
    """Return (GNU as lines, byte size) for one DB/DW/DD statement."""
    width = SIZE[kind]
    lines, size = [], 0
    for op in split_operands(operands):
        m = re.fullmatch(r"(.+?)\s+DUP\s*\((.*)\)", op, re.I)
        if m:
            count = parse_num(m.group(1), eq)
            inner = m.group(2).strip()
            val = 0 if inner == "?" else parse_num(inner, eq)
            if count is None or val is None:
                raise ValueError(f"cannot evaluate DUP operand {op!r}")
            lines.append(f"    .fill {count}, {width}, {val & ((1 << (8 * width)) - 1)}")
            size += count * width
            continue
        if op == "?":
            lines.append(f"    .fill 1, {width}, 0")
            size += width
            continue
        if op[:1] in "'\"" and kind == "DB":
            text = op[1:-1]
            lines.append("    .byte " + ", ".join(str(ord(c)) for c in text))
            size += len(text)
            continue
        val = parse_num(op, eq)
        if val is not None:
            if width in DIRECTIVE:
                lines.append(f"    {DIRECTIVE[width]} {val & ((1 << (8 * width)) - 1)}")
            else:
                lines.append(f"    .fill 1, {width}, 0 /* {op} */")
            size += width
            continue
        m = re.fullmatch(r"(?:OFFSET\s+)?([A-Za-z_]\w*)", op, re.I)
        if m and width == 4:
            notes.append(m.group(1))
            lines.append(f"    .long KE_LABEL_{m.group(1)}")
            size += 4
            continue
        raise ValueError(f"cannot evaluate data operand {op!r}")
    return lines, size


def parse_module(path: Path):
    eq: dict[str, int] = {}
    publics: list[str] = []
    seg = None
    in_struc = False
    blobs = {"_DATA": [], "_TEXT": []}     # (label|None, lines, size)
    code_labels: list[str] = []
    all_code: set[str] = set()
    label_refs: list[str] = []
    for raw in path.read_text(encoding="latin-1").splitlines():
        line = strip_comment(raw)
        if not line.strip():
            continue
        s = line.strip()
        up = s.upper()
        m = re.match(r"^(\w+)\s+STRUC\b", s, re.I)
        if m:
            in_struc = True
            continue
        if in_struc:
            if re.match(r"^\w+\s+ENDS\b", s, re.I):
                in_struc = False
            continue
        m = re.match(r"^(\w+)\s+EQU\s+(.+)$", s, re.I)
        if m:
            v = parse_num(m.group(2), eq)
            if v is not None:
                eq[m.group(1)] = v
            continue
        m = re.match(r"^(\w+)\s+SEGMENT\b", s, re.I)
        if m:
            seg = m.group(1).upper()
            continue
        if re.match(r"^\w+\s+ENDS\b", s, re.I):
            seg = None
            continue
        for pm in re.finditer(r"\bPUBLIC\s+(.+)$", s, re.I):
            publics += [p.strip() for p in pm.group(1).split(",")]
        if up.startswith("PUBLIC"):
            continue
        m = re.match(r"^(\w+)\s+(DB|DW|DD|DF|DQ)\b\s*(.*)$", s, re.I)
        if m and seg in blobs:
            lines, size = emit_items(m.group(2).upper(), m.group(3), eq, label_refs)
            blobs[seg].append((m.group(1), lines, size))
            continue
        m = re.match(r"^(DB|DW|DD|DF|DQ)\b\s*(.*)$", s, re.I)
        if m and seg in blobs:
            lines, size = emit_items(m.group(1).upper(), m.group(2), eq, label_refs)
            blobs[seg].append((None, lines, size))
            continue
        m = re.match(r"^(\w+)\s+LABEL\s+(\w+)", s, re.I)
        if m and seg in blobs:
            if m.group(2).upper() in ("NEAR", "FAR", "PROC"):
                all_code.add(m.group(1))
                if seg == "_TEXT":
                    code_labels.append(m.group(1))
            else:
                blobs[seg].append((m.group(1), [], 0))
            continue
        m = re.match(r"^(\w+)\s+PROC\b", s, re.I)
        if m:
            all_code.add(m.group(1))
            code_labels.append(m.group(1))
            continue
        m = re.match(r"^(\w+):", s)
        if m:
            all_code.add(m.group(1))
    return publics, blobs, code_labels, all_code, label_refs


def generate():
    s_out = ["/* GENERATED by port/tools/gen_asm_stubs.py - do not edit. */",
             "/* Byte-exact _DATA of the historical TASM modules that no translation owns yet. */",
             "/* Symbols follow the i686-w64-mingw32 C convention (leading underscore). */", ""]
    c_out = ["/* GENERATED by port/tools/gen_asm_stubs.py - do not edit. */",
             "/* Link stubs for the public code labels of the historical TASM modules that no",
             " * translation (port/asm/<module>.c) owns yet. Each logs once and returns 0. */",
             '#include "ke_stub.h"', ""]
    inventory = []
    owned = {p.stem for p in OWNED.glob("*.c")} if OWNED.exists() else set()
    for path in sorted(ASM.glob("*.asm")):
        stem = path.stem
        if stem in owned:
            s_out.append(f"/* {stem}: owned by port/asm/{stem}.c */\n")
            c_out.append(f"/* {stem}: owned by port/asm/{stem}.c */\n")
            continue
        if stem == "irq":
            s_out += [f"/* {stem}.asm: LE object 2 real-mode IRQ templates (USE16). Zero bytes: the",
                      " * virtual PC never executes real mode; C code only copies and patches them. */",
                      "    .data", "    .balign 16", f"_ke_asm_irq_object2:"]
            by_off = sorted(IRQ_OBJECT2.items(), key=lambda kv: kv[1])
            for name, off in by_off:
                s_out += [f"    .globl _{name}", f"    .set _{name}, _ke_asm_irq_object2 + {off:#x}"]
                inventory.append((stem, name, "data(object2)", off))
            s_out += [f"    .fill {IRQ_OBJECT2_SIZE:#x}, 1, 0", ""]
            continue
        publics, blobs, code_labels, all_code, label_refs = parse_module(path)
        pubset = set(publics)
        defined_data = set()
        for seg in ("_DATA", "_TEXT"):
            items = blobs[seg]
            if not items:
                continue
            blob = f"_ke_asm_{stem}{seg.lower()}"
            s_out += [f"/* {stem}.asm {seg} */", "    .data", "    .balign 4", f"{blob}:"]
            off = 0
            for label, lines, size in items:
                if label:
                    if label in pubset:
                        s_out += [f"    .globl _{label}"]
                        inventory.append((stem, label, f"data{seg}", off))
                    s_out.append(f"_{label}:")
                    defined_data.add(label)
                s_out += lines
                off += size
            s_out += [f"/* {stem} {seg}: {off:#x} bytes */", ""]
        code_pub = [p for p in publics if p not in defined_data]
        seen = set()
        for name in code_pub:
            if name in seen:
                continue
            seen.add(name)
            c_out.append(f"KE_ASM_STUB({name}, \"{stem}\")")
            inventory.append((stem, name, "code", None))
        # data words that hold code addresses: resolve to the stub when public, else 0
        for ref in sorted(set(label_refs)):
            if ref in seen:
                s_out.insert(4, f"    .set KE_LABEL_{ref}, _{ref}")
            elif ref in defined_data:
                s_out.insert(4, f"    .set KE_LABEL_{ref}, _{ref}")
            else:
                s_out.insert(4, f"    .set KE_LABEL_{ref}, 0 /* {stem}: private label */")
        c_out.append("")
    return "\n".join(s_out) + "\n", "\n".join(c_out) + "\n", inventory


def main(argv):
    s_text, c_text, inventory = generate()
    if "--check" in argv:
        ok = OUT_S.exists() and OUT_S.read_text() == s_text and OUT_C.exists() and OUT_C.read_text() == c_text
        print("asm stubs up to date" if ok else "asm stubs STALE: run python port/tools/gen_asm_stubs.py")
        return 0 if ok else 1
    OUT_S.parent.mkdir(parents=True, exist_ok=True)
    OUT_S.write_text(s_text, newline="\n")
    OUT_C.write_text(c_text, newline="\n")
    if "--list" in argv:
        for stem, name, kind, off in inventory:
            print(f"{stem:18} {kind:14} {name}" + (f" +{off:#x}" if off is not None else ""))
    n_code = sum(1 for i in inventory if i[2] == "code")
    print(f"wrote {OUT_S.relative_to(ROOT)} and {OUT_C.relative_to(ROOT)}: "
          f"{n_code} code stubs, {len(inventory) - n_code} data symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
