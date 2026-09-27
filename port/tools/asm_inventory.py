"""Inventory of the historical TASM modules for translation planning and oracle testing.

    python port/tools/asm_inventory.py            # markdown table on stdout

Per module: size, public code/data labels, and the instructions/operands that decide how the
original bytes can be exercised natively by port/oracle:
  io     IN/OUT            -> emulated by the oracle VEH (routed to vhw, traced)
  if     CLI/STI           -> emulated (traced)
  int    INT n             -> emulated through vhw_int
  sreg   MOV/POP Sreg      -> ignored by the VEH (flat); PUSH Sreg is harmless
  vga    references to video memory (A000h window constants or the page/plane base
         variables that hold A000h addresses at run time) -> needs the VGA memory hook
         (work package oracle-vga) or a RAM-backed page for the test
  16bit  USE16 real-mode code (never runs in the port)
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VGA_NAMES = re.compile(r"\b(0A0000h|0B0000h|VGA_A000\w*|screen_page_base|render_page_base|"
                       r"active_video_page_buffer|vga_row_advance|background_plane_delta|"
                       r"current_vga_plane_mask|first_vga_plane_mask)\b", re.I)


def scan(path: Path):
    text = path.read_text(encoding="latin-1")
    code = [l.split(";")[0] for l in text.splitlines()]
    body = "\n".join(code)
    ops = [l.strip().split(None, 1) for l in code if l.strip()]
    mn = [o[0].lower() for o in ops if o]
    args = [(o[0].lower(), o[1] if len(o) > 1 else "") for o in ops if o]
    publics = re.findall(r"\bPUBLIC\s+(\w+)", body, re.I)
    m = re.match(r"m_([0-9a-f]+)_([0-9a-f]+)", path.stem)
    size = int(m.group(2), 16) - int(m.group(1), 16) if m else 0x149
    sreg = sum(1 for op, a in args if (op == "mov" and re.match(r"\s*(ds|es|fs|gs|ss)\s*,", a, re.I))
               or (op == "pop" and re.match(r"\s*(ds|es|fs|gs)\b", a, re.I)))
    return {
        "module": path.stem, "size": size, "publics": len(set(publics)),
        "io": sum(1 for x in mn if x in ("in", "out", "insb", "outsb", "rep")) -
              sum(1 for op, a in args if op == "rep" and not re.search(r"\b(ins|outs)", a, re.I)),
        "if": sum(1 for x in mn if x in ("cli", "sti")),
        "int": sum(1 for x in mn if x == "int"),
        "sreg": sreg,
        "vga": len(VGA_NAMES.findall(body)),
        "use16": bool(re.search(r"\bUSE16\b", body)),
    }


def classify(r):
    if r["use16"]:
        return "real-mode template: not executed by the port (data only)"
    need = []
    if r["vga"]:
        need.append("VGA memory hook")
    if r["io"] or r["if"] or r["int"] or r["sreg"]:
        need.append("VEH port/IF/INT emulation (available)")
    return "pure memory: direct" if not need else " + ".join(need)


def main():
    rows = [scan(p) for p in sorted((ROOT / "asm").glob("*.asm"))]
    print("| module | bytes | publics | IN/OUT | CLI/STI | INT | Sreg | video refs | oracle strategy |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|---|")
    for r in rows:
        print(f"| {r['module']} | {r['size']:#x} | {r['publics']} | {r['io']} | {r['if']} | {r['int']} | "
              f"{r['sreg']} | {r['vga']} | {classify(r)} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
