"""Build one object of the ILP32 game world for a 64-bit Android ABI.

    python ilp32_world.py --abi arm64-v8a|x86_64 --clang CLANG --api 29 \
        --kind game|asm|clib|data --src FILE --out OBJ [--work DIR] [-- extra clang args]

The historical game (src/*.c), the C translations of its assembly (port/asm/*.c) and their
generated data are written for Watcom's flat 32-bit model: int, long and pointers are all
4 bytes, pointers live in ints, records overlay pointer-bearing structs, and several units
declare one global as `int` and another as a pointer (docs/android/architecture.md, "64-bit:
the ILP32 game world"). This tool keeps that data model on 64-bit CPUs:

  1. compile the unit with an ILP32 code generator for the same CPU family
       arm64-v8a: arm64_32-apple-watchos (AArch64 instructions, 32-bit pointers, Mach-O text)
       x86_64:    x86_64-linux-gnux32   (x86-64 instructions, 32-bit pointers, ELF text)
  2. normalize the assembly to ELF syntax for the 64-bit target (Mach-O relocation operators,
     sections, symbol prefixes; GOT loads of 32-bit pointers become direct PC-relative
     address materialization: every symbol lives in the same shared object below 2 GB)
  3. put the unit's data in source order (gcc -fno-toplevel-reorder, which clang lacks) and
     tentative definitions in .bss, then apply port/tools/gcc_pack_data.pack() - the exact
     layout step of the Windows build (packed _DATA/_BSS, Watcom BSS order, original CONST)
  4. turn 32-bit absolute data relocations into a self-relocation table (section
     ke32_relocs, applied once by port/android/ilp32/ke32_runtime.c before the game runs),
     rename C-library calls to their ILP32 entry points and check that every external call
     crosses the world boundary only through the allow-list below
  5. assemble for the 64-bit Android target.

The result links into the same shared object as the 64-bit virtual PC; the loader maps that
object below 2 GB, so every address the game can form is a lossless 32-bit value.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "tools"))

ILP32_TARGET = {"arm64-v8a": "arm64_32-apple-watchos", "x86_64": "x86_64-linux-gnux32"}
LP64_TARGET = {"arm64-v8a": "aarch64-linux-android", "x86_64": "x86_64-linux-android"}

# C library entry points of the game world (port/android/ilp32/include) -> 64-bit shims with
# 32-bit parameters (port/android/ilp32/ke32_shims.c). Implicit memcpy/memset calls that the
# compiler emits for struct copies are renamed too.
RENAME = {
    "malloc": "ke32_malloc", "free": "ke32_free", "abs": "ke32_abs",
    "itoa": "ke32_itoa", "ltoa": "ke32_ltoa", "strtoul": "ke32_strtoul", "_rotl": "ke32_rotl",
    "_splitpath": "ke32_splitpath", "memcpy": "ke32_memcpy", "memmove": "ke32_memmove",
    "memset": "ke32_memset", "memcmp": "ke32_memcmp", "strcpy": "ke32_strcpy",
    "strcat": "ke32_strcat", "strlen": "ke32_strlen", "strcmp": "ke32_strcmp",
    "strchr": "ke32_strchr", "_stricmp": "ke32_stricmp", "fopen": "ke32_fopen",
    "fclose": "ke32_fclose", "fread": "ke32_fread", "fwrite": "ke32_fwrite",
    "fseek": "ke32_fseek", "ftell": "ke32_ftell", "fflush": "ke32_fflush",
    "int386": "ke32_int386", "int386x": "ke32_int386x", "segread": "ke32_segread",
    "ke_atexit": "ke32_atexit", "ke_getenv": "ke32_getenv", "ke_stub_hit": "ke32_stub_hit",
    "__chkstk_darwin": "ke32_chkstk_darwin", "printf": "ke32_printf", "spawnlp": "ke32_spawnlp",
}

# 64-bit functions the game world may call directly: scalar (<= 32-bit) parameters and
# results only, so the two calling conventions agree register for register.
DIRECT_OK = {
    "inp", "inpw", "outp", "outpw", "_disable", "_enable", "delay", "sound", "nosound",
    "kbhit", "getch", "getche", "ke_exit", "vhw_enter", "vhw_leave", "vhw_cpu_poll_yield",
    "vga_mem_read8", "vga_mem_write8", "ke32_host_write", "ke32_spawn_refused",
    "vcpu_cli", "vcpu_sti", "vcpu_interrupts_enabled",
    # 32-bit data shared with the virtual PC (same size and layout in both worlds)
    "ke_lowmem_shadow", "vhw_last_inp_value",
}

LOCAL_PREFIXES = (".L", "L", "l", "ltmp")


# ---- 1. compile -------------------------------------------------------------------------

def compile_unit(args, extra, out_s: Path, ast: bool = False):
    resource = subprocess.run([args.clang, "-print-resource-dir"], capture_output=True,
                              text=True, check=True).stdout.strip()
    cmd = [args.clang, f"--target={ILP32_TARGET[args.abi]}", "-nostdinc",
           "-I", str(ROOT / "port" / "android" / "ilp32" / "include"),
           "-isystem", str(Path(resource) / "include"),
           "-fno-common", "-fwrapv", "-fno-strict-aliasing", "-fno-delete-null-pointer-checks",
           "-funsigned-char", "-mno-red-zone", "-fno-stack-protector",
           "-fkeep-persistent-storage-variables", "-fno-zero-initialized-in-bss"]
    if args.abi == "x86_64":
        cmd += ["-fPIC"]
    if args.kind == "game":
        cmd += ["-std=gnu89", "-O0", "-fpack-struct=1", "-mms-bitfields", "-ffp-eval-method=double",
                "-I", str(ROOT / "port" / "include" / "watcom"),
                "-include", str(ROOT / "port" / "include" / "watcom" / "watcom_compat.h"),
                "-Wno-int-conversion", "-Wno-incompatible-pointer-types",
                "-Wno-incompatible-function-pointer-types", "-Wno-implicit-function-declaration",
                "-Wno-implicit-int", "-Wno-return-type", "-Wno-main", "-Wno-multichar",
                "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
                "-Wno-incompatible-library-redeclaration", "-Wno-builtin-requires-header",
                "-Wno-deprecated-non-prototype", "-Wno-comment", "-Wno-format-security",
                "-Wno-parentheses", "-Wno-pointer-sign", "-Wno-dangling-else",
                "-Wno-constant-conversion", "-Wno-shift-count-overflow",
                "-Wno-tautological-constant-out-of-range-compare", "-Wno-empty-body",
                "-Wno-format", "-Wno-invalid-noreturn", "-Wno-unused-value",
                "-Wno-string-plus-int", "-Wno-extra-tokens", "-Wno-non-literal-null-conversion",
                "-Wno-int-to-void-pointer-cast", "-Wno-pointer-integer-compare",
                "-Wno-excess-initializers", "-Wno-knr-promoted-parameter",
                "-Wno-strict-prototypes", "-Wno-typedef-redefinition",
                "-Wno-single-bit-bitfield-constant-conversion", "-Wno-array-bounds",
                "-Wno-unsequenced", "-Wno-ignored-attributes", "-Wno-return-mismatch",
                "-Wno-bitfield-constant-conversion"]
        name = Path(args.src).stem
        if name == "u_13a95":
            cmd += ["-Dmain=ke_game_main"]
        protos = ROOT / "port" / "android" / "ilp32" / "include" / f"ke_clang_{name}.h"
        if protos.exists():
            cmd += ["-include", str(protos)]
    elif args.kind == "asm":
        cmd += ["-std=gnu11", "-O1", "-fno-omit-frame-pointer", "-Wall", "-Wno-unused-function",
                "-I", str(ROOT / "port" / "include")]
    else:  # clib: the game world's own C library helpers
        cmd += ["-std=gnu11", "-O1", "-Wall", "-I", str(ROOT / "port" / "include")]
    cmd += extra
    if ast:
        cmd += ["-fsyntax-only", "-Xclang", "-ast-dump", str(args.src)]
        result = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
        return result.stdout
    cmd += ["-S", str(args.src), "-o", str(out_s)]
    result = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    if result.returncode:
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return None


# ---- 2. normalize -----------------------------------------------------------------------

def strip_comment(line: str, comment_chars: str) -> str:
    out, quote = [], False
    i = 0
    while i < len(line):
        c = line[i]
        if quote:
            out.append(c)
            if c == "\\" and i + 1 < len(line):
                out.append(line[i + 1])
                i += 2
                continue
            if c == '"':
                quote = False
        else:
            if c == '"':
                quote = True
            elif c in comment_chars or line.startswith("//", i):
                break
            out.append(c)
        i += 1
    return "".join(out).rstrip()


def split_strings(line: str):
    """Yield (is_string, text) pieces so that symbol rewrites never touch string literals."""
    pieces, cur, quote, i = [], [], False, 0
    while i < len(line):
        c = line[i]
        if quote:
            cur.append(c)
            if c == "\\" and i + 1 < len(line):
                cur.append(line[i + 1])
                i += 2
                continue
            if c == '"':
                pieces.append((True, "".join(cur)))
                cur, quote = [], False
        elif c == '"':
            if cur:
                pieces.append((False, "".join(cur)))
            cur, quote = [c], True
        else:
            cur.append(c)
        i += 1
    if cur:
        pieces.append((quote, "".join(cur)))
    return pieces


def map_symbols(line: str, fn) -> str:
    out = []
    for is_string, text in split_strings(line):
        out.append(text if is_string else re.sub(r"(?<![\w.$])([A-Za-z_.$][\w.$]*)", fn, text))
    return "".join(out)


MACHO_SECTIONS = {
    "__TEXT,__text": "\t.text",
    "__TEXT,__cstring": "\t.section .rdata",
    "__TEXT,__const": "\t.section .rdata",
    "__DATA,__const": "\t.section .rdata",
    "__DATA,__data": "\t.data",
    "__TEXT,__literal4": "\t.section .rodata.kelit,\"a\",%progbits",
    "__TEXT,__literal8": "\t.section .rodata.kelit,\"a\",%progbits",
    "__TEXT,__literal16": "\t.section .rodata.kelit,\"a\",%progbits",
}


def normalize_macho(text: str) -> str:
    """arm64_32 Mach-O assembly -> AArch64 ELF assembly (C names still `_`-prefixed)."""
    lines = []
    for raw in text.splitlines():
        line = strip_comment(raw, ";")
        s = line.strip()
        if not s:
            continue
        if re.match(r"\.(watchos_version_min|build_version|subsections_via_symbols|no_dead_strip|"
                    r"loh|data_region|end_data_region|alt_entry|indirect_symbol|ident)\b", s):
            continue
        m = re.match(r"\.section\s+(__\w+,__\w+)", s)
        if m:
            sec = MACHO_SECTIONS.get(m.group(1))
            if sec is None:
                raise ValueError(f"unhandled Mach-O section: {s}")
            lines.append(sec)
            continue
        m = re.match(r"\.zerofill\s+__DATA,__(?:bss|common),(\w+),(\d+),(\d+)", s)
        if m:
            lines += ["\t.data", f"\t.p2align {m.group(3)}", f"{m.group(1)}:",
                      f"\t.zero {m.group(2)}"]
            continue
        if s.startswith(".private_extern"):
            continue            # the object is linked into one shared object; all hidden
        # relocation operators
        line = re.sub(r"([\w.$]+(?:[+-]\d+)?)@GOTPAGEOFF", r"@@GOTOFF@@\1", line)
        line = re.sub(r"([\w.$]+)@PAGEOFF((?:[+-]\d+)?)", r":lo12:\1\2", line)
        line = re.sub(r"([\w.$]+)@GOTPAGE", r"\1", line)
        line = re.sub(r"([\w.$]+)@PAGE((?:[+-]\d+)?)", r"\1\2", line)
        # a 32-bit GOT load becomes the address itself (symbol is in this shared object)
        m = re.match(r"(\s*)ldr\s+[wx](\d+),\s*\[(x\d+|sp),\s*@@GOTOFF@@([\w.$]+)\]$", line)
        if m:
            base = m.group(3)
            line = f"{m.group(1)}add\tx{m.group(2)}, {base}, :lo12:{m.group(4)}"
        if "@@GOTOFF@@" in line:
            raise ValueError(f"unhandled GOT access: {raw}")
        lines.append(line)
    # symbol names: Mach-O local labels -> ELF local labels; `_c_name` -> `c_name`
    defined_local = set()
    for line in lines:
        m = re.match(r"^([\w.$]+):", line)
        if m and m.group(1).startswith(("L", "l")):
            defined_local.add(m.group(1))

    def rename(match):
        name = match.group(1)
        if name in defined_local:
            return ".L" + name
        if name.startswith("_") and not name.startswith("__ke_original_const"):
            return name[1:]
        return name

    out = []
    for line in lines:
        s = line.strip()
        if s.startswith(".asciz"):
            line = line.replace(".asciz", ".string", 1)
            out.append(line)
            continue
        if s.startswith((".ascii", ".string")):
            out.append(line)
            continue
        if re.match(r"^[\w.$]+:", s):
            label, rest = s.split(":", 1)
            label = rename(re.match(r"(.+)", label))
            out.append(f"{label}:{rest}")
            continue
        if s.startswith("."):
            dm = re.match(r"(\.\S+)\s*(.*)$", s)
            directive, operands = dm.group(1), dm.group(2)
            if directive in (".p2align", ".space", ".zero", ".byte", ".short", ".cfi_startproc",
                             ".cfi_endproc", ".cfi_def_cfa_offset", ".cfi_offset", ".cfi_def_cfa",
                             ".text", ".data", ".section", ".cfi_remember_state",
                             ".cfi_restore_state", ".cfi_restore"):
                out.append(line)
                continue
            out.append("	" + directive + ("	" + map_symbols(operands, rename) if operands else ""))
            continue
        # instruction: mnemonic stays, operands renamed (register names never start with _/L/l
        # followed by a defined local label name)
        m = re.match(r"(\s*)(\S+)(.*)$", line)
        out.append(m.group(1) + m.group(2) + map_symbols(m.group(3), rename))
    return "\n".join(out) + "\n"


def normalize_elf_x32(text: str) -> str:
    """x32 ELF assembly -> canonical sections for the layout step."""
    out = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = strip_comment(lines[i], "#")
        i += 1
        s = line.strip()
        if not s or s.startswith((".file", ".ident", ".addrsig")):
            continue
        if s.startswith('.section\t".note.GNU-stack"') or s.startswith('.section ".note.GNU-stack"'):
            continue
        m = re.match(r"\.section\s+\.rodata\.str", s)
        if m:
            out.append("\t.section .rdata")
            continue
        if re.match(r"\.section\s+\.rodata,", s) or s == ".section\t.rodata" or s == ".section .rodata":
            nxt = next((strip_comment(l, "#").strip() for l in lines[i:i + 4]
                        if re.match(r"^[\w.$]+:", strip_comment(l, "#").strip())), "")
            if nxt.startswith(".LJTI"):
                out.append("\t.section .rodata.kejt,\"a\",@progbits")
            else:
                out.append("\t.section .rdata")
            continue
        if re.match(r"\.section\s+\.(data\.rel\.ro|data\.rel)", s):
            out.append("\t.section .rdata")
            continue
        if s.startswith(".asciz"):
            line = line.replace(".asciz", ".string", 1)
        out.append(line)
    return "\n".join(out) + "\n"


# ---- 3. source order ----------------------------------------------------------------------

def ast_var_order(ast_text: str):
    order, info = [], {}
    for line in ast_text.splitlines():
        if not re.match(r"^[|`]-VarDecl ", line):
            continue
        m = re.search(r"> (?:col:\d+|line:\d+:\d+)\s+(?:used\s+|referenced\s+)*(?:invalid\s+)?"
                      r"([A-Za-z_]\w*) '", line)
        if not m:
            continue
        name = m.group(1)
        tail = line[m.end():]
        is_extern = bool(re.search(r"'\s+extern\b|\sextern(\s|$)", tail))
        initialized = bool(re.search(r"\b(cinit|callinit|listinit)\b", tail))
        entry = info.get(name)
        if entry is None:
            entry = {"extern": True, "init": False, "index": None}
            info[name] = entry
        if not is_extern and entry["index"] is None:
            entry["index"] = len(order)
            order.append(name)
        if not is_extern:
            entry["extern"] = False
        if initialized:
            entry["init"] = True
    return order, info


SECTION_RE = re.compile(r"^\s*\.(text|data|bss)\b|^\s*\.section\s+([^\s,]+)")


def section_name(line: str):
    m = SECTION_RE.match(line)
    if not m:
        return None
    return (m.group(1) or m.group(2)).lstrip(".")


def data_size(lines) -> int:
    size = 0
    widths = {"byte": 1, "short": 2, "hword": 2, "2byte": 2, "long": 4, "word": 4, "4byte": 4,
              "int": 4, "quad": 8, "xword": 8, "8byte": 8}
    for line in lines:
        s = line.strip()
        m = re.match(r"\.(zero|space)\s+(\d+)", s)
        if m:
            size += int(m.group(2))
            continue
        m = re.match(r"\.(\w+)\s+(.+)$", s)
        if m and m.group(1) in widths:
            size += widths[m.group(1)] * len(m.group(2).split(","))
        elif m and m.group(1) in ("ascii", "string"):
            raise ValueError("string data in a tentative definition")
    return size


def source_order(text: str, ast_text: str) -> str:
    """Emit the unit's .data chunks in declaration order, tentative definitions as .bss."""
    order, info = ast_var_order(ast_text)
    lines = text.splitlines()
    sections = []
    cur = None
    for line in lines:
        name = section_name(line)
        if name is not None:
            cur = name
        sections.append(cur)
    chunks, others = [], []
    pending, chunk = [], None
    for line, sec in zip(lines, sections):
        if sec != "data":
            if chunk is not None:
                chunks.append(chunk)
                chunk = None
            if pending:
                others.extend(pending)
                pending = []
            others.append(line)
            continue
        s = line.strip()
        if section_name(line) == "data":
            continue
        gm = re.match(r"\.globl\s+([\w.$]+)", s)
        tm = re.match(r"\.(type|size)\s+([\w.$]+)", s)
        lm = re.match(r"^([\w.$]+):", s)
        if gm or s.startswith(".p2align") or (tm and tm.group(1) == "type"):
            if chunk is not None:
                chunks.append(chunk)
                chunk = None
            pending.append(line)
            continue
        if lm and not lm.group(1).startswith(".L"):
            if chunk is not None:
                chunks.append(chunk)
            chunk = {"name": lm.group(1), "pre": pending, "body": [line]}
            pending = []
            continue
        if chunk is None:
            raise ValueError(f"data line outside a symbol: {line}")
        chunk["body"].append(line)
    if chunk is not None:
        chunks.append(chunk)
    if pending:
        others.extend(pending)

    def key(c):
        entry = info.get(c["name"])
        return entry["index"] if entry and entry["index"] is not None else 1 << 30

    data_chunks = [c for c in chunks if not (c["name"] in info and not info[c["name"]]["init"])]
    bss_chunks = [c for c in chunks if c["name"] in info and not info[c["name"]]["init"]]
    data_chunks.sort(key=key)
    bss_chunks.sort(key=key)
    out = list(others)
    if data_chunks:
        out.append("\t.data")
        for c in data_chunks:
            out += [l for l in c["pre"] if not l.strip().startswith(".type")]
            out += [l for l in c["body"] if not l.strip().startswith(".size")]
    if bss_chunks:
        out.append("\t.bss")
        for c in bss_chunks:
            out += [l for l in c["pre"] if not l.strip().startswith(".type")]
            body = [l for l in c["body"] if not l.strip().startswith(".size")]
            out.append(body[0])
            out.append(f"\t.zero {data_size(body[1:])}")
    return "\n".join(out) + "\n"


# ---- 4. finalize --------------------------------------------------------------------------

def finalize(text: str, abi: str, unit: str):
    progbits = "%progbits" if abi == "arm64-v8a" else "@progbits"
    nobits = "%nobits" if abi == "arm64-v8a" else "@nobits"
    out, relocs, globals_ = [], [], set()
    sec = "text"
    reloc_no = 0
    long_dir = ".long"
    defined = set()
    for line in text.splitlines():
        m = re.match(r"^\s*([\w.$]+):", line)
        if m:
            defined.add(m.group(1))
        m = re.match(r"^\s*\.set\s+([\w.$]+)\s*,", line)
        if m:
            defined.add(m.group(1))
    for line in text.splitlines():
        s = line.strip()
        name = section_name(line)
        if name is not None:
            if name.startswith("data$KECONST"):
                line = f"\t.section .data.{name[5:].replace('$', '_').lower()},\"aw\",{progbits}"
                sec = "data"
            elif name == "rdata":
                line = f"\t.section .rodata.ke32,\"a\",{progbits}"
                sec = "rdata"
            elif name == "bss":
                line = f"\t.section .bss,\"aw\",{nobits}"
                sec = "bss"
            elif name == "data":
                line = "\t.data"
                sec = "data"
            elif name == "text":
                sec = "text"
            else:
                sec = name
            out.append(line)
            continue
        if sec == "rdata" and not s.startswith((".globl", ".type", ".size", ".set", "#")):
            # a const object with symbolic relocations cannot stay read-only after the
            # self-relocation pass: keep all game constants writable, like Watcom's _DATA
            out.append(f"\t.section .data.ke32const,\"aw\",{progbits}")
            sec = "data"
        gm = re.match(r"\.globl\s+([\w.$]+)", s)
        if gm:
            globals_.add(gm.group(1))
            out.append(line)
            out.append(f"\t.hidden {gm.group(1)}")
            continue
        dm = re.match(r"\.(long|word|4byte|int)\s+(.+)$", s)
        if dm and sec != "text" and not sec.startswith("rodata.kejt"):
            items = [x.strip() for x in dm.group(2).split(",")]
            if any(re.search(r"[A-Za-z_.$]", x) and not re.fullmatch(r"0x[0-9a-fA-F]+", x)
                   for x in items):
                for item in items:
                    if re.search(r"[A-Za-z_.$]", item) and not re.fullmatch(r"0x[0-9a-fA-F]+", item):
                        if re.search(r"[\w.$]\s*-\s*[A-Za-z_.$]", item):
                            raise ValueError(f"{unit}: label difference in data: {s}")
                        label = f".Lke32_reloc_{reloc_no}"
                        reloc_no += 1
                        out.append(f"{label}:")
                        out.append(f"\t{long_dir}\t0")
                        relocs.append((label, rename_target(item)))
                    else:
                        out.append(f"\t{long_dir}\t{item}")
                continue
        out.append(rename_calls(line, abi))
    out.append(f"\t.section .note.GNU-stack,\"\",{progbits}")
    if relocs:
        out.append(f"\t.section ke32_relocs,\"aw\",{progbits}")
        out.append("\t.p2align 3")
        for label, target in relocs:
            out.append(f"\t.quad\t{label}")
            out.append(f"\t.quad\t{target}")
    return "\n".join(out) + "\n"


LDST_LO12 = re.compile(r"^(\s*)(ldr|str|ldrh|strh|ldrsh|ldrsw|ldur|stur)\s+([wxhsdq]\d+|wzr|xzr),\s*"
                       r"\[(x\d+|sp),\s*:lo12:([^\]]+)\]\s*$")


def unaligned_lo12(text: str) -> str:
    """AArch64: scaled loads/stores need a naturally aligned :lo12: offset.

    The ILP32 code generator assumed natural alignment of every global; the Watcom layout
    (gcc_pack_data.pack) packs them byte-aligned, so `ldr w8, [x8, :lo12:sym]` may address an
    odd offset the instruction cannot encode. Materialize the address with ADD into a
    temporary register the function does not use, and access through it (unaligned normal
    memory accesses are architecturally allowed). Byte accesses need no rewrite.
    """
    lines = text.split("\n")
    out, func = [], []

    def flush(block):
        used = set()
        for l in block:
            for m in re.finditer(r"\b[wx](\d+)\b", l):
                used.add(int(m.group(1)))
        scratch = next((r for r in (17, 16, 15, 14, 13, 12, 11, 10, 9) if r not in used), None)
        for l in block:
            m = LDST_LO12.match(l)
            if not m:
                out.append(l)
                continue
            indent, op, reg, base, expr = m.groups()
            if scratch is not None:
                out.append(f"{indent}add\tx{scratch}, {base}, :lo12:{expr}")
                out.append(f"{indent}{op}\t{reg}, [x{scratch}]")
            elif op.startswith("ldr") and reg[0] in "wx":
                # a load may compute the address in its own destination register
                out.append(f"{indent}add\tx{reg[1:]}, {base}, :lo12:{expr}")
                out.append(f"{indent}{op}\t{reg}, [x{reg[1:]}]")
            else:
                # every temporary is live: borrow one around the access (SP stays 16-aligned)
                busy = {reg[1:], base[1:]}
                tmp = next(r for r in ("17", "16", "15") if r not in busy)
                out.append(f"{indent}str\tx{tmp}, [sp, #-16]!")
                out.append(f"{indent}add\tx{tmp}, {base}, :lo12:{expr}")
                out.append(f"{indent}{op}\t{reg}, [x{tmp}]")
                out.append(f"{indent}ldr\tx{tmp}, [sp], #16")

    for line in lines:
        s = line.strip()
        if s.startswith(".cfi_startproc"):
            out.extend(func)
            func = [line]
            continue
        if func:
            func.append(line)
            if s.startswith(".cfi_endproc"):
                flush(func)
                func = []
            continue
        out.append(line)
    if func:
        flush(func)
    return "\n".join(out)


def rename_target(expr: str) -> str:
    return re.sub(r"(?<![\w.$])([A-Za-z_][\w.$]*)",
                  lambda m: RENAME.get(m.group(1), m.group(1)), expr)


def rename_calls(line: str, abi: str) -> str:
    s = line.strip()
    if not s or s.startswith((".", "#")) or s.endswith(":"):
        return line
    return map_symbols(line, lambda m: RENAME.get(m.group(1), m.group(1)))


# ---- 5. assemble + boundary check --------------------------------------------------------

def assemble(args, asm: Path, out: Path):
    cmd = [args.clang, f"--target={LP64_TARGET[args.abi]}{args.api}", "-c", str(asm), "-o", str(out)]
    result = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    if result.returncode:
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)


def undefined_symbols(args, obj: Path):
    nm = str(Path(args.clang).with_name("llvm-nm" + Path(args.clang).suffix))
    text = subprocess.run([nm, "-u", str(obj)], capture_output=True, text=True).stdout
    return {line.split()[-1] for line in text.splitlines() if line.strip()}


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--abi", required=True, choices=sorted(ILP32_TARGET))
    ap.add_argument("--clang", required=True)
    ap.add_argument("--api", default="29")
    ap.add_argument("--kind", required=True, choices=["game", "asm", "clib", "data"])
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--world-symbols", help="file listing symbols defined by the ILP32 world")
    ap.add_argument("extra", nargs="*")
    args = ap.parse_args(argv)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    raw = out.with_suffix(".raw.s")
    norm = out.with_suffix(".norm.s")
    packed = out.with_suffix(".packed.s")
    final = out.with_suffix(".final.s")
    unit = Path(args.src).name
    if args.kind == "data":
        # generated TASM _DATA (port/stubs/asm_data.S): i686-mingw names -> ELF names
        text = Path(args.src).read_text()
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        text = re.sub(r"(?<![\w.$])_([A-Za-z_]\w*)", r"\1", text)
        final.write_text(finalize(text, args.abi, unit))
        assemble(args, final, out)
        return 0
    compile_unit(args, args.extra, raw)
    text = raw.read_text(errors="replace")
    text = normalize_macho(text) if args.abi == "arm64-v8a" else normalize_elf_x32(text)
    if args.kind == "game":
        ast = compile_unit(args, args.extra, raw, ast=True)
        text = source_order(text, ast)
        norm.write_text(text)
        import gcc_pack_data
        text = gcc_pack_data.pack(text, str(Path(args.src)))
    else:
        norm.write_text(text)
    packed.write_text(text)
    text = finalize(text, args.abi, unit)
    if args.abi == "arm64-v8a":
        text = unaligned_lo12(text)
    final.write_text(text)
    assemble(args, final, out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
