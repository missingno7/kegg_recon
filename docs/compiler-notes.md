# Watcom 10.0 `-3s -d2` code-generation notes

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
  position depends on the local types and the returned expression â€” e.g. with two `unsigned` locals it
  comes first, with two `unsigned short` locals it comes last (f_9ca4). Wrong slot order is usually a
  type problem, not a flag problem.
- Nested-block autos keep their own slot (no reuse after the block); `register` has no effect; `static`
  locals move to data.

Calls
- Constant argument to an `int`/`long`/pointer parameter (or an unprototyped call): `push imm`.
  To a `char`/`short` (signed or unsigned) parameter: `mov eax,imm; push eax` (`xor eax,eax` for 0; the value is
  zero-extended to the parameter width, so `(char)-1` gives `mov eax,0xff`). So call sites reveal the callee's
  parameter types â€” and require the right prototype in the candidate.

Control flow
- `for (i = a; i < n; i++) body` compiles to: init; `L1: cmp; jl L3; jmp L4; L2: inc; jmp L1; L3: body; jmp L2; L4:`.
  A `while` loop has the plain `L1: cmp; jge L4; body; jmp L1` shape â€” pick the form by the layout.
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
- all 242 C functions EXACT today are still EXACT with `-3s -d2 -s`; the 80 of them that use `volatile` hacks stay
  EXACT under `-d2` with every `volatile` deleted (37 of those fail under `-od`). Copies: `build/workers/idioms/nov/`.
- `-d2` puts `$$SYMBOLS`/`$$TYPES` segments in the object (WLINK drops them unless `debug` is given), which
  `check.py` currently reports as unplaceable data. `build/workers/idioms/hv.py "-3s -d2 -s" FILES` compiles
  with `-d2` and runs `check.py --obj`, treating only those debug-segment problems as ignorable (`EXACT*`).
  Needed centrally: profile `game-c-d2` and a check.py rule that ignores debug-class segments. Until then
  `volatile` is NOT the source construct â€” do not add it.
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
- `if (p[k]++ == c)` on a byte -> `mov dl,[eax+k]; mov eax,[p]; inc byte ptr [eax+k]; cmp dl,c` (old value in DL;
  f_bc91). Pre-increment in a condition `if (++p[k] > c)` -> `inc byte ptr [eax]; cmp byte ptr [eax],c` (f_6e2e).
- `if ((*p = e) < 0)` (assignment as condition) -> store, reload `mov eax,[p]; mov eax,[eax]; test eax,eax`.
  A plain `if (*p < 0)` gives `cmp dword ptr [eax],0` (`asgcond.c`, `ltzero.c`).
- Shift count that is an `int` in memory: `x << s->i` -> `mov ecx,[s]; mov cl,[ecx+off]` (no movzx) and the
  result is copied `mov ecx,edx` before use. `unsigned char` count -> `movzx ecx,byte ptr` (`shiftcnt.c`).
- Pointer arithmetic vs int arithmetic (register order tells the type):
  `int + const` -> `mov edx,[g]; add edx,c; mov eax,[p]; mov [eax+f],edx` (add before loading the destination
  base); `ptr + const` (or any non-plain-dword left operand: movsx/movzx, shift, mul, div result) ->
  `mov edx,[g]; mov eax,[p]; add edx,c` (add after). `local = charptr + tbl[i].f` ->
  `<i in eax>; mov edx,[ptr]; mov eax,[eax+tbl]; add eax,edx` (f_592c); `charptr + int_global` ->
  `mov eax,[ptr]; mov edx,[int]; add edx,eax` (f_c685 `pic->pixels = pic->buf + pic->size`, 0x5531)
  (`sumorder.c`, `sumorder2.c`, `ptradd.c`, `sum2g5.c`). So a global added this way is a (byte) pointer.
- 1-D array with manual row index: `t[i*4 + k]` (k != 0) -> `shl eax,2; shl eax,2; mov eax,[eax+t+4k]`;
  `t[i*4]`, `t2d[i][k]`, `s[i].f` -> single `shl eax,4` (`arr1d.c`, `arr2d.c`).
- `&&`/`||` never thread jumps: in `if (A && B)` A's false edge goes to a `jmp` trampoline; `if (A) if (B)` and
  consecutive `if (A) break; if (B) break;` give direct `jcc exit` (`andand.c`, `oror.c`).
- Several `return expr;` share one spill slot: `mov [ebp-N],eax; jmp L; ...; mov [ebp-N],eax; L: mov eax,[ebp-N]`;
  an unused first local shifts that slot (f_d36c: `int unused;` -> spill at `[ebp-8]`).
- Return spill at `[ebp-4]` *before* the autos (and two int autos in reverse declaration order) means a narrow
  return type: f_b680 is `short f_b680(...)` with `int xend; int yend;` (spill -4, yend -8, xend -0xc). With an
  `int` return the spill comes after the autos. `return local;` never reuses the local's slot (extra spill).
- Flag tests `if (g & m)` on a global/struct field -> `test byte ptr [g+k], m>>8k` when m fits one byte;
  on a param/local -> `test dword ptr [ebp+x], m`; `(g & m) == m` -> `mov; and; cmp` (`testb.c`).
- Bitfields. Store `s->f = v` (unsigned int container): `<v in edx>; and edx,(1<<w)-1; and byte ptr [eax+byte],~mask;
  shl edx,pos; or dword ptr [eax+base],edx` (f_7e62 `unsigned b0:1, anim:4, f5:1, ...; p->anim = flags >> 4;`,
  f_6a1d `unsigned type:2`). Read of an `unsigned char` bitfield: `mov al,[eax]; shr al,pos` / `and al,m; movzx eax,al`;
  `if (s->hi)` -> `test byte ptr [eax],mask` (f_3918 `struct { unsigned char lo:2, hi:6; unsigned char flag; }`;
  `bitf.c`). Byte-width `shr al`/`and al` on a loaded byte means a bitfield, not `(x >> n) & m`.
- Sums of calls: write the natural left-associative `f(a)+f(b)+f(c)+...` in call order; partial sums rotate through
  ESI/EBX across the calls (`movzx esi,al; ...; add ebx,esi`) — f_a966, no reordering tricks needed.
- Structs are packed (`-zp1` default): `struct {int a; unsigned char b; int c;}` puts `c` at +5 (`testb.c`).
- `enum` objects take the smallest integer type (byte if values fit): `movzx eax,byte ptr` when passed (`p2.c`).
- Address constants as arguments (`&g`, arrays, string literals, function names) -> `mov eax,offset; push eax`;
  an integer literal gives `push imm` (`p3.c`).
- Unexplained: `ptr->f = g1 + g2` (two int globals) gives `mov eax,g1; add eax,g2; mov edx,eax` in every probe,
  while the original has `mov edx,[g1]; add edx,[g2]; mov eax,[ptr]` at 0x584, 0x55a0, 0x6ad0 (EDX direct
  only happens here when `ptr` is a local/param, or for `charptr + mem` with an indexed/indirect int operand:
  `(int)(p + a[i])` -> `mov edx,[p]; add edx,[a+eax]`). `+=`/`-=` forms match (`sum2g*.c`, `ctx2.c`, `brute.py`).

## Frame layout rule (lifter agent, fitted to 16 probes, reproduces 297/309 matched functions)
- Watcom shell-sorts the list [parameters, autos in declaration order, return temp] by size (gap sequence n//2,
  then (g+1)//2) and assigns slots in the sorted order. So parameter types and the return type move the locals:
  wrong slot order usually means a wrong prototype, a wrong narrow/wide local or return type, or a missing
  nested-block auto. `tools/lift.py` inverts this rule; use it to get a consistent declaration set.

## Operand evaluation order and expression temps (worker `hard`, probes `build/workers/hard/p/t3..t10.c`, `dis.py`)
- Binary operators (`+`, `<`, ...) evaluate the *deeper* operand tree first; on a tie the left one goes first.
  No-op conversion nodes count as a level: int<->unsigned (explicit cast or usual arithmetic conversion),
  short->int (`movsx`), integer->pointer cast; pointer->pointer casts and constant struct/array offsets
  folded into the address do not always (2-D row offset does). Writing the operands in the other order does
  not help (commutative ops are not swapped). So register order tells you where the hidden conversions are:
  - `g_e326[a1] + g_e336[a1]` -> left index in EAX first; the original's right-index-in-EDX-first is
    `int g_e326[3][4]; g_e326[0][a1] + g_e326[1][a1] + g_e326[2][a1]` (f_a810 EXACT; `g_e336[(unsigned)a1]`
    also works) (`t3.c`).
  - `p->e + *p->f` (int, int*) -> right chain first (`mov eax,[p]; mov eax,[eax+f]; mov edx,[p]; mov edx,[edx+e]`);
    with `unsigned *f` (int+unsigned converts the left) or `(int)unsigned_e + ...` the left base is loaded first
    (`mov edx,[p]; mov eax,[p]; mov eax,[eax+f]; mov edx,[edx+e]; add edx,[eax]`). `*(unsigned *)int_field`
    does not (int->ptr cast deepens the right again); `*(unsigned *)charptr_field` does (`t6.c`-`t8.c`).
    f_7bf4 EXACT with `struct Frame { unsigned img; int time; } *f;` and `g_8db4 = p->e + p->f->img;`.
  - `if ((xs = bx + b.ox) + b.w < (ax = x + a.ox))`: the left (assignment + movsx) is deeper -> computed first
    into EDX, compare `cmp edx,[ax]; jl` (f_b6e4). With `... + mx` (int) on the right side the original's
    `cmp eax,edx; jg` comes from writing `(ax = x + a.ox) + mx > (xs = bx + b.ox) + b.w` (f_b76c) (`t10.c`).
- Assignment inside a condition keeps the `mov [slot],r; cmp r2,[slot]` reload (as `asgcond.c`).
- A call result copied *before* the caller's `add esp,N` (`call f; mov ebx,eax` or `mov [ebp-8],eax; add esp,8`)
  is a temporary inside a larger expression; a named-local assignment statement stores *after* `add esp`.
  FPU conversion temps get ordinary 4-byte stack slots and are reused: `(float)(int expr)` spilled across a
  later call is `mov [t],ebx; fild [t]; fstp [t]`, and `(int)(f() * g * fl)` gives `mov [t],eax; fild [t];
  ...; call __CHP; fistp [t]`. So a frame smaller than the declared locals + a temp means "no locals, one big
  expression": f_b03c EXACT as `g_e1f8 = f_dd53(..) + ((f_13324(..) + f_13324(..)) >> 1) +
  (int)(f_dd53(-g_740c >> 1, g_740c >> 1) * g_e1fc * (float)(abs(x1-x2) + abs(y1-y2))) / (g_740c >> 1);`
  (heaviest operand -- the division -- evaluated first, partial sums in EBX/ESI).
- Struct-by-value params: f_b6e4/f_b76c take `(Frame a, int x, int y, Frame b, int x, int y, ...)` with an
  18-byte packed `Frame` of 9 shorts (arguments are dword-padded: 20 bytes on the stack). Their return type is
  `short` (no `int` return + local order reproduces the frame); the matched caller f_7551 compares `cmp eax,-1`,
  i.e. its TU saw an `int` (probably implicit) declaration.
- `and eax,1; and eax,1; mov edx,eax; and byte ptr [p],~m; shl edx,k; or dword ptr [p],edx` = a 1-bit bitfield
  store of an already-masked value: `((Flags *)g_de5c)->b2 = (x >> 3) & 1;` (f_4cf2 EXACT; manual
  `(*p & ~4) | (v << 2)` gives load/or/store).
