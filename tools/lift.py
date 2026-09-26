"""Deterministic, conservative first-draft lifter for Watcom 10.0 -od functions.

    python tools/lift.py f_7032 f_2e4a --out build/workers/lift/out
    python tools/lift.py --all --out build/workers/lift/out
    python tools/lift.py --range 0x7000 0x8000 --out build/workers/lift/out

The output is ordinary C.  Unsupported instructions are marked in comments; no
machine bytes are copied into the source.  This is a draft generator, not a
decompiler proof: tools/check.py remains the authority for an EXACT match.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build" / "pylib"))
sys.path.insert(0, str(ROOT / "tools"))
import capstone  # noqa: E402
import le  # noqa: E402

JCC = {"je": "==", "jz": "==", "jne": "!=", "jnz": "!=", "jl": "<",
       "jnge": "<", "jle": "<=", "jng": "<=", "jg": ">", "jnle": ">",
       "jge": ">=", "jnl": ">=", "jb": "<", "jnae": "<", "jbe": "<=",
       "jna": "<=", "ja": ">", "jnbe": ">", "jae": ">=", "jnb": ">="}
INV = {"==": "!=", "!=": "==", "<": ">=", "<=": ">", ">": "<=", ">=": "<"}
LIB_HEADERS = {"getenv": "stdlib.h", "malloc": "stdlib.h", "free": "stdlib.h",
               "atoi": "stdlib.h", "rand": "stdlib.h", "srand": "stdlib.h",
               "memcpy": "string.h", "memset": "string.h", "strlen": "string.h",
               "strcmp": "string.h", "strcpy": "string.h", "strcat": "string.h",
               "printf": "stdio.h", "sprintf": "stdio.h", "fopen": "stdio.h",
               "fclose": "stdio.h", "fread": "stdio.h", "fwrite": "stdio.h"}


def num(s):
    try:
        return int(s, 0)
    except ValueError:
        return None


def split_ops(s):
    return [x.strip() for x in s.split(",", 1)] if "," in s else [s.strip()]


def width(s):
    return 1 if re.search(r"(?<!d)byte ptr", s) else 2 if re.search(r"(?<!d)word ptr", s) else 4


class Lifter:
    def __init__(self, fn, code, fix, names, decls, man):
        self.fn, self.code, self.fix, self.names, self.decls, self.man = fn, code, fix, names, decls, man
        self.start, self.end = int(fn["start"], 16), int(fn["end"], 16)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.ins = list(md.disasm(code[self.start:self.end], self.start))
        self.switches = {}
        table_spans = []
        for pos, ins in enumerate(self.ins):
            if ins.mnemonic != "jmp" or "cs:[" not in ins.op_str:
                continue
            site = next((a for a in range(ins.address, ins.address + ins.size) if a in fix), None)
            target = fix.get(site) if site is not None else None
            if not target or target.get("obj") != 1:
                continue
            table = target["off"]
            compare = next((x for x in reversed(self.ins[max(0, pos - 6):pos])
                            if x.mnemonic == "cmp" and num(split_ops(x.op_str)[-1]) is not None), None)
            if not compare:
                continue
            count = min(256, num(split_ops(compare.op_str)[-1]) + 1)
            entries = [fix.get(table + 4 * n) for n in range(count)]
            if not all(x and x.get("obj") == 1 for x in entries):
                continue
            default = next((num(x.op_str) for x in reversed(self.ins[max(0, pos - 6):pos])
                            if x.mnemonic in ("ja", "jnbe")), None)
            self.switches[ins.address] = ([x["off"] for x in entries], default)
            table_spans.append((table, table + 4 * count))
        if table_spans:
            self.ins = [i for i in self.ins if not any(a <= i.address < b for a, b in table_spans)]
        self.byaddr = {i.address: n for n, i in enumerate(self.ins)}
        self.used_globals, self.used_calls, self.slots = {}, {}, {}
        self.args = {}
        self.unknown = []
        self.raw_registers = set()
        self.reg = {r: r for r in ("eax", "ebx", "ecx", "edx", "esi", "edi", "esp")}
        self.pending = None
        self.pushes = []
        self.lines = []
        self.cmp = None
        self.return_slot = None
        self.labels = set()
        for i in self.ins:
            if i.mnemonic.startswith("j") and i.mnemonic != "jmp":
                t = num(i.op_str)
                if t in self.byaddr:
                    self.labels.add(t)
            elif i.mnemonic == "jmp":
                t = num(i.op_str)
                if t in self.byaddr:
                    self.labels.add(t)

    def fixed(self, ins):
        for a in range(ins.address, ins.address + ins.size):
            if a in self.fix:
                return self.fix[a]
        return None

    def global_name(self, target, w=4):
        off = target["off"]
        name = f"g_{off:x}"
        self.used_globals[name] = w
        return name

    def mem(self, s, ins=None):
        w = width(s)
        m = re.search(r"\[ebp\s*([+-])\s*(0x[0-9a-f]+|\d+)\]", s)
        if m:
            d = num(m.group(2)) * (1 if m.group(1) == "+" else -1)
            if d >= 0x14:
                k = (d - 0x14) // 4
                self.args[k] = max(self.args.get(k, 0), w)
                return f"a{k}"
            name = f"v_{-d:x}"
            self.slots[name] = max(self.slots.get(name, 0), w)
            return name
        if ins and (target := self.fixed(ins)) and target.get("obj") == 3:
            name = self.global_name(target, w)
            if "[" in s and re.search(r"\[[^]]+\+", s):
                return name
            return name
        m = re.search(r"\[(0x[0-9a-f]+)\]", s)
        if m:
            return f"(*(int *){m.group(1)})"
        m = re.search(r"\[([^]]+)\]", s)
        if m:
            address = m.group(1)
            for r in re.findall(r"\b(?:eax|ebx|ecx|edx|esi|edi|esp)\b", address):
                address = re.sub(rf"\b{r}\b", f"({self.reg[r]})", address)
                if self.reg[r] == r:
                    self.raw_registers.add(r)
            typ = "char" if w == 1 else "short" if w == 2 else "int"
            return f"(*({typ} *)({address}))"
        return s

    def expr(self, s, ins=None):
        s = s.strip()
        if "ptr [" in s:
            return self.mem(s, ins)
        if s in self.reg:
            return self.reg[s]
        if s in ("al", "ax"):
            return self.reg["eax"]
        if s in ("bl", "bx"):
            return self.reg["ebx"]
        if s in ("cl", "cx"):
            return self.reg["ecx"]
        if s in ("dl", "dx"):
            return self.reg["edx"]
        for small, full in (("ah", "eax"), ("bh", "ebx"), ("ch", "ecx"), ("dh", "edx")):
            if s == small:
                self.raw_registers.add(small)
                return small
        n = num(s)
        return str(n) if n is not None else s

    def emit(self, s):
        self.lines.append(s)

    def flush(self):
        if self.pending:
            self.emit(self.pending + ";")
            self.pending = None

    def call_name(self, i):
        target = num(i.op_str)
        if target is None:
            value = self.expr(i.op_str, i)
            if value.startswith("g_"):
                self.used_globals[value] = -1  # function pointer
            return value
        name = self.names.get(f"1:{target:x}", f"f_{target:x}")
        self.used_calls[name] = max(self.used_calls.get(name, 0), len(self.pushes))
        return name

    def condition(self, op, reverse=False):
        rel = JCC.get(op, "!=")
        if reverse:
            rel = INV[rel]
        lhs, rhs = self.cmp or ("0", "0")
        if rel in ("==", "!=") and rhs == "0":
            return lhs if rel == "!=" else f"!({lhs})"
        return f"{lhs} {rel} {rhs}"

    def translate(self, i):
        op, ss = i.mnemonic, split_ops(i.op_str)
        dst, src = ss[0], ss[1] if len(ss) > 1 else None
        if op in ("nop", "int3"):
            return
        if op in ("mov", "movsx", "movzx") and src is not None:
            v = self.expr(src, i)
            if self.pending and v != self.pending:
                self.flush()
            if op == "movzx" and width(src) == 1:
                v = f"(unsigned char){v}"
            elif op == "movzx" and width(src) == 2:
                v = f"(unsigned short){v}"
            elif op == "movsx" and width(src) == 1:
                v = f"(signed char){v}"
            elif op == "movsx" and width(src) == 2:
                v = f"(short){v}"
            if dst in self.reg:
                self.reg[dst] = v
            elif dst in ("al", "ax"):
                self.reg["eax"] = v
            elif dst in ("ah", "bh", "ch", "dh"):
                self.raw_registers.add(dst)
                self.emit(f"{dst} = {v};")
            else:
                place = self.mem(dst, i)
                if self.pending and v == self.pending:
                    self.pending = None
                self.emit(f"{place} = {v};")
            return
        if op == "lea" and src:
            target = self.fixed(i)
            if target and target.get("obj") == 3:
                self.reg[dst] = "&" + self.global_name(target)
            else:
                m = re.search(r"\[ebp - (0x[0-9a-f]+)\]", src)
                if m:
                    self.reg[dst] = "&" + self.mem("dword ptr " + m.group(0), i)
                else:
                    inside = re.search(r"\[([^]]+)\]", src)
                    address = inside.group(1) if inside else src
                    for r in re.findall(r"\b(?:eax|ebx|ecx|edx|esi|edi|esp|ebp)\b", address):
                        repl = self.reg.get(r, r)
                        if repl == r:
                            self.raw_registers.add(r)
                        address = re.sub(rf"\b{r}\b", f"({repl})", address)
                    self.reg[dst] = f"({address})"
            return
        if op == "push":
            self.pushes.append(self.expr(dst, i))
            return
        if op == "call":
            self.flush()
            name = self.call_name(i)
            args_list = list(reversed(self.pushes))
            if name == "getenv" and args_list:
                args_list[0] = f"(char *)({args_list[0]})"
            args = ", ".join(args_list)
            self.pending = f"{name}({args})"
            self.reg["eax"] = self.pending
            self.pushes = []
            return
        if op == "add" and dst == "esp":
            return
        if op in ("cmp", "test") and src:
            self.flush()
            a, b = self.expr(dst, i), self.expr(src, i)
            self.cmp = (a, "0" if op == "test" and a == b else b)
            return
        if op in ("add", "sub", "imul", "and", "or", "xor", "shl", "shr", "sar") and src:
            a, b = self.expr(dst, i), self.expr(src, i)
            if op in ("imul", "and", "or", "xor", "shl", "shr", "sar"):
                for symbol in re.findall(r"\bg_[0-9a-f]+\b", a + " " + b):
                    if any(re.search(rf"\*\s*{symbol}\b", d) for d in self.decls.get(symbol, [])):
                        a, b = f"(int){a}", f"(int){b}"
                        break
            if op == "xor" and dst == src:
                v = "0"
            else:
                sym = {"add": "+", "sub": "-", "imul": "*", "and": "&", "or": "|",
                       "xor": "^", "shl": "<<", "shr": ">>", "sar": ">>"}[op]
                v = f"({a} {sym} {b})"
            if dst in self.reg:
                self.reg[dst] = v
            else:
                self.emit(f"{self.mem(dst, i)} = {v};")
            return
        if op in ("inc", "dec"):
            place = self.expr(dst, i)
            if self.pending and place == self.pending:
                self.flush()
                return
            parent = {"al": "eax", "ax": "eax", "bl": "ebx", "bx": "ebx",
                      "cl": "ecx", "cx": "ecx", "dl": "edx", "dx": "edx"}.get(dst, dst)
            if parent in self.reg:
                self.reg[parent] = f"({place} {'+' if op == 'inc' else '-'} 1)"
            else:
                self.emit(f"{place}{'++' if op == 'inc' else '--'};")
            return
        if op == "neg":
            if dst in self.reg:
                self.reg[dst] = f"(-{self.expr(dst, i)})"
            else:
                x = self.expr(dst, i)
                self.emit(f"{x} = -{x};")
            return
        if op in ("cdq", "cwde", "movsb", "movsd"):
            return
        self.unknown.append(f"{i.address:x}: {op} {i.op_str}")

    def body(self):
        # Strip the fixed Watcom frame and final return sequence.  Alignment
        # padding is within the manifest extent for some -ot functions.
        arr = self.ins
        lo = 6 if len(arr) >= 6 and [x.mnemonic for x in arr[:6]] == ["push", "push", "push", "push", "mov", "sub"] else 0
        hi = len(arr)
        while hi > lo and arr[hi - 1].mnemonic in ("nop",):
            hi -= 1
        if hi > lo and arr[hi - 1].mnemonic == "ret":
            hi -= 1
        for op in ("pop", "pop", "pop", "pop"):
            if hi > lo and arr[hi - 1].mnemonic == op:
                hi -= 1
        if hi > lo and arr[hi - 1].mnemonic == "leave":
            hi -= 1
        # A frame-zero function uses pop ebp instead of leave; four pops
        # above already consumed it.
        body = arr[lo:hi]
        if body and body[-1].mnemonic == "mov" and body[-1].op_str.startswith("eax, "):
            self.return_slot = self.expr(split_ops(body[-1].op_str)[1], body[-1])
            body = body[:-1]
        self.walk(body, 0, len(body))
        self.flush()
        # Labels at the common epilogue are legitimate break/early-exit targets.
        for target in sorted(self.labels):
            if target >= (body[-1].address + body[-1].size if body else self.start) and target < self.end:
                self.emit(f"L_{target:x}:;")
        if self.cmp and body and body[-1].mnemonic in ("cmp", "test"):
            lhs, rhs = self.cmp
            self.emit(f"if ({lhs}{'' if rhs == '0' else ' == ' + rhs}) {{ }}")
        if self.return_slot:
            # A simple return expression owns its spill slot; spelling it as
            # a C local would make Watcom allocate a second slot.
            tail = f"{self.return_slot} = "
            if self.lines and self.lines[-1].startswith(tail):
                val = self.lines.pop()[len(tail):-1]
                self.slots.pop(self.return_slot, None)
                self.emit(f"return {val};")
            else:
                self.emit(f"return {self.return_slot};")
        referenced = set(re.findall(r"\bgoto (L_[0-9a-f]+);", "\n".join(self.lines)))
        defined = set(re.findall(r"^(L_[0-9a-f]+):", "\n".join(self.lines), re.M))
        for label in sorted(referenced - defined):
            self.emit(f"{label}:;")

    def walk(self, arr, a, b):
        addridx = {x.address: k for k, x in enumerate(arr)}
        if b - a >= 3 and arr[b - 1].mnemonic in JCC and num(arr[b - 1].op_str) == arr[a].address:
            self.emit("do {")
            self.walk(arr, a, b - 1)
            self.flush()
            self.emit(f"}} while ({self.condition(arr[b - 1].mnemonic)});")
            return
        k = a
        while k < b:
            i = arr[k]
            if i.mnemonic in JCC:
                target = num(i.op_str)
                t = addridx.get(target)
                if t is not None and k < t <= b:
                    # -od for: test -> branch to body -> jump to exit ->
                    # increment -> jump to test -> body -> jump to increment.
                    if (k + 2 < t < b and arr[k + 1].mnemonic == "jmp"):
                        exit_idx = addridx.get(num(arr[k + 1].op_str))
                        inc_idx = k + 2
                        if (exit_idx is not None and t < exit_idx <= b and
                                arr[t - 1].mnemonic == "jmp" and
                                addridx.get(num(arr[t - 1].op_str), -1) <= k and
                                arr[exit_idx - 1].mnemonic == "jmp" and
                                addridx.get(num(arr[exit_idx - 1].op_str)) == inc_idx):
                            updates = [x for x in arr[inc_idx:t - 1] if x.mnemonic in ("inc", "dec", "add", "sub")]
                            if len(updates) == 1:
                                u = updates[0]
                                uv = self.expr(split_ops(u.op_str)[0], u)
                                if u.mnemonic in ("inc", "dec"):
                                    step = uv + ("++" if u.mnemonic == "inc" else "--")
                                else:
                                    ops = split_ops(u.op_str)
                                    step = f"{uv} {'+=' if u.mnemonic == 'add' else '-='} {self.expr(ops[1], u)}"
                                self.flush()
                                self.emit(f"for (; {self.condition(i.mnemonic)}; {step}) {{")
                                self.walk(arr, t, exit_idx - 1)
                                self.flush()
                                self.emit("}")
                                k = exit_idx
                                continue
                    # Plain while has a back edge from the end of the body.
                    if (t > k + 1 and arr[t - 1].mnemonic == "jmp" and
                            addridx.get(num(arr[t - 1].op_str), b) <= k):
                        self.flush()
                        self.emit(f"while ({self.condition(i.mnemonic, True)}) {{")
                        self.walk(arr, k + 1, t - 1)
                        self.flush()
                        self.emit("}")
                        k = t
                        continue
                    # Forward branch around a block; optionally over an else.
                    self.flush()
                    cond = self.condition(i.mnemonic, True)
                    end_then = t
                    else_end = None
                    if t - 1 > k and arr[t - 1].mnemonic == "jmp":
                        j = addridx.get(num(arr[t - 1].op_str))
                        if j is not None and t < j <= b:
                            end_then, else_end = t - 1, j
                    self.emit(f"if ({cond}) {{")
                    self.walk(arr, k + 1, end_then)
                    self.flush()
                    if else_end is not None:
                        self.emit("} else {")
                        self.walk(arr, t, else_end)
                        self.flush()
                        k = else_end
                    else:
                        k = t
                    self.emit("}")
                    continue
                self.flush()
                self.emit(f"if ({self.condition(i.mnemonic)}) goto L_{target:x};" if target else "/* unresolved branch */")
            elif i.mnemonic == "jmp":
                target = num(i.op_str)
                self.flush()
                if i.address in self.switches:
                    cases, default = self.switches[i.address]
                    selector = self.reg["eax"]
                    selector = re.sub(r"^\((.*) << 2\)$", r"\1", selector)
                    self.emit(f"switch ({selector}) {{")
                    for value, dest in enumerate(cases):
                        self.emit(f"case {value}: goto L_{dest:x};")
                    if default is not None:
                        self.emit(f"default: goto L_{default:x};")
                    self.emit("}")
                else:
                    self.emit(f"goto L_{target:x};" if target in addridx else "/* external jump */")
            else:
                if i.address in self.labels and k != a:
                    self.flush()
                    self.emit(f"L_{i.address:x}:;")
                    self.reg = {r: r for r in self.reg}
                self.translate(i)
            k += 1

    def source(self):
        self.body()
        out = ["/* Draft lifted from original instructions; verify with tools/check.py. */"]
        for name in sorted(self.used_globals):
            w = self.used_globals[name]
            ds = sorted(self.decls.get(name, []), key=len)
            d = next((x for x in ds if re.search(rf"\b{name}\b", x) and x.startswith("extern ")), None)
            if w == -1:
                out.append(f"extern int (*{name})(void);")
            elif d and not re.search(r"\b(?:Rec\w*|Pair\w*|Row\w*|State|Sprite|Object)\b", d):
                out.append(d)
            else:
                out.append(f"extern {'char' if w == 1 else 'short' if w == 2 else 'int'} {name};")
        for name, argc in sorted(self.used_calls.items()):
            if name == self.fn["name"]:
                continue
            if name in LIB_HEADERS:
                out.insert(1, f"#include <{LIB_HEADERS[name]}>")
                continue
            out.append(f"extern int {name}();")
        argc = max(self.args, default=-1) + 1
        pars = ", ".join(f"{'char' if self.args.get(k) == 1 else 'short' if self.args.get(k) == 2 else 'int'} a{k}" for k in range(argc)) or "void"
        ret = "int" if self.return_slot else "void"
        out.append(f"{ret} {self.fn['name']}({pars})")
        out.append("{")
        for line in self.lines:
            self.raw_registers.update(re.findall(r"\b(?:eax|ebx|ecx|edx|esi|edi|esp|ebp|al|bl|cl|dl|ah|bh|ch|dh|ax|bx|cx|dx|cs|ss|gs|fs|es|ds)\b", line))
        for r in sorted(self.raw_registers):
            out.append(f"    int {r} = 0; /* unresolved register value */")
        for slot, w in sorted(self.slots.items(), key=lambda p: int(p[0][2:], 16)):
            out.append(f"    {'char' if w == 1 else 'short' if w == 2 else 'int'} {slot};")
        for line in self.lines:
            if line.startswith("L_"):
                out.append(line)
            else:
                out.append("    " + line)
        for u in self.unknown:
            out.append("    /* unsupported: " + u.replace("*/", "") + " */")
        out.append("}")
        return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("functions", nargs="*", help="manifest function names")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--all", action="store_true", help="all non-matching functions")
    ap.add_argument("--range", nargs=2, metavar=("START", "END"), help="non-matching functions in [start,end)")
    a = ap.parse_args()
    man = json.loads((ROOT / "manifest.json").read_text())
    byname = {f["name"]: f for f in man["functions"]}
    selected = list(a.functions)
    if a.all:
        selected += [f["name"] for f in man["functions"] if f["status"] != "matching"]
    if a.range:
        low, high = (int(x, 0) for x in a.range)
        selected += [f["name"] for f in man["functions"] if f["status"] != "matching" and low <= int(f["start"], 16) < high]
    if not selected:
        ap.error("give functions, --all, or --range")
    L = le.LE(ROOT / "assets" / "KE.EXE")
    code = L.object_bytes(L.objects[0])[:L.objects[0]["vsize"]]
    fix = {off: t for obj, off, typ, t in L.resolved_fixups() if obj == 1}
    names = {v: k for k, v in man.get("symbols", {}).items()}
    names.update({f"1:{int(f['start'], 16):x}": f["name"] for f in man["functions"]})
    decls = defaultdict(set)
    for src in (ROOT / "src").glob("*.c"):
        for statement in re.findall(r"\bextern\s+[^;{}]+;", src.read_text(errors="replace")):
            statement = " ".join(statement.split())
            for symbol in re.findall(r"\b[fg]_[0-9a-f]+\b", statement):
                decls[symbol].add(statement)
    a.out.mkdir(parents=True, exist_ok=True)
    for name in dict.fromkeys(selected):
        if name not in byname:
            ap.error(f"unknown function {name}")
        f = byname[name]
        lifted = Lifter(f, code, fix, names, decls, man)
        (a.out / (name + ".c")).write_text(lifted.source())
        print(f"{name} {f['start']}..{f['end']} unsupported={len(lifted.unknown)}")


if __name__ == "__main__":
    main()
