# Watcom 10.0 `-3s -od` code-generation notes

Proven by controlled probes (identical under 10.0 GA and 10.0a, `-od` and `-ot -od`) or by EXACT matches.
Details: `build/workers/slots/REPORT.md` (not tracked; regenerate with the probes it lists).

Frame
- Prologue `push ebx; push esi; push edi; push ebp; mov ebp,esp; sub esp,imm32` always (imm32 even for 0).
  Epilogue `leave` if frame > 0 else `pop ebp`; then `pop edi/esi/ebx; ret`. Caller pops args (`add esp,N`).
- Arguments: first at `[ebp+0x14]`.

Stack slots
- Every scalar auto (`char`, `short`, `int`, pointer) takes a 4-byte slot; accesses use the C width.
  Slots are assigned in declaration order from `[ebp-4]` downward.
- Small structs take a dword slot; larger ones keep their size, 4-aligned. Arrays are packed at element
  width with a dword-aligned base.
- The spill of a returned value (`mov [ebp-N],eax; mov eax,[ebp-N]; leave`) takes its own slot whose
  position depends on the local types and the returned expression — e.g. with two `unsigned` locals it
  comes first, with two `unsigned short` locals it comes last (f_9ca4). Wrong slot order is usually a
  type problem, not a flag problem.
- Nested-block autos keep their own slot (no reuse after the block); `register` has no effect; `static`
  locals move to data.

Calls
- Constant argument to an `int`/`long`/pointer parameter (or an unprototyped call): `push imm`.
  To a `char`/`short` (signed or unsigned) parameter: `mov eax,imm; push eax` (`xor eax,eax` for 0; the value is
  zero-extended to the parameter width, so `(char)-1` gives `mov eax,0xff`). So call sites reveal the callee's
  parameter types — and require the right prototype in the candidate.

Control flow
- `for (i = a; i < n; i++) body` compiles to: init; `L1: cmp; jl L3; jmp L4; L2: inc; jmp L1; L3: body; jmp L2; L4:`.
  A `while` loop has the plain `L1: cmp; jge L4; body; jmp L1` shape — pick the form by the layout.
- `switch` with a dense range: jump table inside the function, preceded by `jmp` over it and a `nop` to
  4-align the table.

Profiles
- `-ot -od` (profile `game-c-ot`): same code as `-od` for a single function, but `_TEXT` is dword aligned
  and every function is padded to 4 with `90` / `8bc0` / `8d4000`. It is a TU property.

## Debug-info codegen (`-d2`) and source idioms  (worker `idioms`, probes in `build/workers/idioms/probes/`)

TU flags: the game C was compiled with Watcom debug info (`-d2`, or equivalently `-d1+ -od` / `-d3`; code-identical
in every probe, `-hw` format; `-hc` changes code and is excluded). PROVEN by:
- the original never contains `push dword ptr [ebp+x]` (0 sites) but has 94 `mov eax,[ebp+x]; push eax`;
  plain `-od` always emits the former for int/pointer params and locals, `-d2` the latter (probe `pushp.c`).
  Globals are still pushed directly (`push dword ptr [g]`) under both.
- all 226 C functions EXACT today are still EXACT with `-3s -d2 -s`; the 73 of them that use `volatile` hacks stay
  EXACT under `-d2` with every `volatile` deleted (33 of those fail under `-od`). Copies: `build/workers/idioms/nov/`.
- `-d2` puts `$$SYMBOLS`/`$$TYPES` segments in the object (WLINK drops them unless `debug` is given), which
  `check.py` currently reports as unplaceable data. `build/workers/idioms/hv.py "-3s -d2 -s" FILES` compiles
  with `-d2` and runs `check.py --obj`, treating only those debug-segment problems as ignorable (`EXACT*`).
  Needed centrally: profile `game-c-d2` and a check.py rule that ignores debug-class segments. Until then
  `volatile` is NOT the source construct — do not add it.
- `-od` vs `-d2` differ only on ebp-relative operands (params/autos), never on globals (`od_vs_d2.c`):
  push (`push dword ptr [ebp+x]` vs `mov eax,[ebp+x]; push eax`), flag test (`test byte ptr [ebp+x],m` vs
  `test dword ptr [ebp+x],m`; the original has 21 dword / 0 byte tests on dword autos), and the dead load of a
  post-increment statement. Every `volatile` in src/ (params, locals, `volatile int g_dee0`, `int (* volatile
  g_8db8)(void)`) was a stand-in for one of these (or for the pointer-arithmetic order below); none is authentic.
  A real volatile *global* would be visible under `-d2` as a dead `mov eax,[g]` before `g++;` statements and as
  `test dword ptr [g],m` instead of `test byte ptr` (`volat2.c`); the original has neither (its 4
  `mov eax,[g]; inc [g]` sites all use the old value, e.g. `tbl[g++]`), so no ISR-shared global is declared
  volatile in the game C. Call form `g_8db8()` vs `(*g_8db8)()` makes no difference (`volat.c`).

Idioms (all under `-d2`; `->` = emitted code; probe file in parentheses)
- Post-increment statement of a local/param `i++;` / `p++;` -> dead `mov eax,[slot]` before `inc`/`add [slot],n`
  (loop increments `L2: mov eax,[ebp-8]; inc [ebp-8]`). `++i` has no dead load; globals never get one (`postinc.c`).
- `*p++ = e;` -> `mov edx,[p]; inc [p]; <e in al>; mov [edx],al`. Split `p++; *p = e;` gives a dead load and DL
  (`postincstore.c`).
- `if ((*p = e) < 0)` (assignment as condition) -> store, reload `mov eax,[p]; mov eax,[eax]; test eax,eax`.
  A plain `if (*p < 0)` gives `cmp dword ptr [eax],0` (`asgcond.c`, `ltzero.c`).
- Shift count that is an `int` in memory: `x << s->i` -> `mov ecx,[s]; mov cl,[ecx+off]` (no movzx) and the
  result is copied `mov ecx,edx` before use. `unsigned char` count -> `movzx ecx,byte ptr` (`shiftcnt.c`).
- Pointer arithmetic vs int arithmetic (register order tells the type):
  `int + const` -> `mov edx,[g]; add edx,c; mov eax,[p]; mov [eax+f],edx` (add before loading the destination
  base); `ptr + const` (or any non-plain-dword left operand: movsx/movzx, shift, mul, div result) ->
  `mov edx,[g]; mov eax,[p]; add edx,c` (add after). `charptr + int_expr` ->
  `mov edx,[ptr]; mov eax,<int>; add eax,edx`; `charptr + int_global` -> `mov eax,[ptr]; mov edx,[int]; add edx,eax`
  (`sumorder.c`, `sumorder2.c`, `ptradd.c`, `sum2g5.c`). So a global added this way is a (byte) pointer.
- 1-D array with manual row index: `t[i*4 + k]` (k != 0) -> `shl eax,2; shl eax,2; mov eax,[eax+t+4k]`;
  `t[i*4]`, `t2d[i][k]`, `s[i].f` -> single `shl eax,4` (`arr1d.c`, `arr2d.c`).
- `&&`/`||` never thread jumps: in `if (A && B)` A's false edge goes to a `jmp` trampoline; `if (A) if (B)` and
  consecutive `if (A) break; if (B) break;` give direct `jcc exit` (`andand.c`, `oror.c`).
- Several `return expr;` share one spill slot: `mov [ebp-N],eax; jmp L; ...; mov [ebp-N],eax; L: mov eax,[ebp-N]`;
  an unused first local shifts that slot (f_d36c: `int unused;` -> spill at `[ebp-8]`).
- Flag tests `if (g & m)` on a global/struct field -> `test byte ptr [g+k], m>>8k` when m fits one byte;
  on a param/local -> `test dword ptr [ebp+x], m`; `(g & m) == m` -> `mov; and; cmp` (`testb.c`).
- Structs are packed (`-zp1` default): `struct {int a; unsigned char b; int c;}` puts `c` at +5 (`testb.c`).
- `enum` objects take the smallest integer type (byte if values fit): `movzx eax,byte ptr` when passed (`p2.c`).
- Address constants as arguments (`&g`, arrays, string literals, function names) -> `mov eax,offset; push eax`;
  an integer literal gives `push imm` (`p3.c`).
- Unexplained: `ptr->f = g1 + g2` (two int globals) gives `mov eax,g1; add eax,g2; mov edx,eax` in every probe,
  while the original has `mov edx,[g1]; add edx,[g2]; mov eax,[ptr]` at 0x584, 0x55a0, 0x6ad0 (EDX direct
  only happens here when `ptr` is a local/param). `+=`/`-=` forms match (`sum2g*.c`, `brute.py`).
