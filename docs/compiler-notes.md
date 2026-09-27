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

## Object-file layout: LEDATA chunking (worker `objects`, probes in `build/workers/objects/probe/`)

wcc386 `-3s -d2 -s` writes an object's `_TEXT` in LEDATA records ("chunks"); WLINK processes each chunk's fixups in
reverse, so the original chunking is visible in KE.EXE's per-page LE fixup order (chunks ascending, sites
descending; `image.Original.chains`).  A chunk is flushed:
- **size**: at the first instruction boundary at or after chunk start + 172 bytes (a switch jump-table entry is one
  4-byte unit).  The object's `_TEXT` starts a fresh chunk at offset 0; function boundaries, LINNUM and PUBDEF
  records, `$$SYMBOLS`/`$$TYPES` flushes and the fixup count (38 seen) do not cut code chunks.  PROVEN: the rule
  predicts every `_TEXT` LEDATA of all 242 compiled game objects (`build/workers/objects/rulecheck.py`).
- **imports**: every symbol an object references is imported at its first reference -- functions (even ones
  defined in the same file, before or after), data (even the TU's own globals and statics), but not CONST
  literals (segment-relative) -- as EXTDEF, or LEXTDEF for `static`.  Right after an instruction, if the pending
  import record holds >= 192 bytes (sum of `len(name) + 2`), the code chunk is flushed with the imports
  (`probe/T.c`: threshold exactly 192; `ext_long.c`: 44-char names cut after every 5th call).  The pending record
  is also written (count restarts) whenever any segment's LEDATA is flushed (size flushes, `$$SYMBOLS` flushes at
  function ends: `dbg1.c`) and when the import kind switches EXTDEF <-> LEXTDEF (`mix.c`).
- Consequences: chunk boundaries depend on the object start and on **identifier lengths** (and indirectly on the
  debug-info volume that triggers `$$SYMBOLS` flushes).  Address-named placeholders (`f_1a2b`, 8 import bytes)
  practically never cause an import flush; the original did wherever a chunk is < 172 bytes.
- Original evidence: 11 of the original's short chunks are import flushes (each after 11-16 first-referenced
  symbols, i.e. names averaging ~10-15 chars); every other short chunk ends at an object start.  The object list
  derived from this (DP over function starts, `build/workers/objects/objscan2.py`, `optpart.py`) is in
  `build/workers/objects/objects_final.json`.  A TU reproduces the original fixup order iff its compiled chunks satisfy the
  run constraints: `python build/workers/objects/chunkfit.py SRC.c START END` (fast check before image.py).
- Without `-d2` (`-3s -od -s`) both rules are the same (`probe/cc.py -3s,-od,-s ext_short.c ext_long.c dbg1.c`: 172-byte
  chunks, import cuts); `-d2` debug flushes do not cut code chunks, they only restart the import count (`dbg1.c`
  under `-d2`: code chunks 180/175/175/172 while `$$SYMBOLS` flushes every function).  This refines the "`-d2`
  splits code LEDATA where debug records are flushed" remark in the _BSS section below.
- Reproducing an original import flush therefore means choosing identifier lengths (and, through local/parameter
  names, where `$$SYMBOLS` flushes fall): `build/workers/objects/lenfit.py` lengthens `f_`/`g_` names with a
  suffix until every compiled TU chunks like the original (58 suffixes + 7 descriptive names make U00010, U00708,
  U02f0c, U0608a, U06b02 and T19 link identically).  Names of `_BSS` objects must also satisfy the `_BSS` hash
  order in the TU that defines them (tools/bssorder.py).

## _BSS order (worker `bss`, probes + random tests in `build/workers/bss/`, tool `tools/bssorder.py`)
PROVEN (≈470 random TUs up to 300 objects, mixed sizes/statics/functions/strings, `-d2`/`-od`/`-ot`/`-ox` alike,
every layout predicted exactly; supervisor probes `build/workers/sup/probe/bss*.c` reproduced):
- Every uninitialised file-scope object (public or `static`) gets `idx` = number of cfe symbol-table entries created
  before its FIRST declaration (`extern` or definition; a later definition does not move it).
  Entries: each new ordinary identifier (variable, function, typedef, implicit function declaration), each
  parameter of a function *definition*, each block-scope declaration (autos, static locals, block `extern`), one
  extra entry per non-void function definition, each distinct string literal (deduplicated TU-wide, in data
  initialisers too). Not entries: enum constants, struct/union/enum tags, members, labels, macros, prototype
  parameter names, numeric/float constants. `#include`d prototypes count (stdio.h/stdlib.h/string.h add 8/24/20 mod 25).
- Layout (no padding, `_BSS` dword aligned) = objects sorted by
  1. size rank, descending: `size%8==0` > `size%4==0` > `size%2==0 && size>2` > `size==2` > odd `>1` > `size==1`
     (only the byte size matters, not the type: `short`==`char[2]`, `short[3]`==`char[6]`, 16-byte struct==`double`);
  2. index group, descending: group 0 = idx 0..4, then groups of 25 (idx 5..29, 30..54, ...), later groups first;
  3. `key = hashpjw(name) % 241`, ascending (cfe identifier hash, case-sensitive, every character:
     `h=c0; h=(h<<4)+c1; loop { h&=0xfff; h=(h<<4)+c; h=(h^(h>>12))&0xfff; h=(h<<4)+c; h^=h>>12 }`);
  4. same bucket in the same group: later declaration first.
  So `int v1..v8;` -> v6 v7 v8 (group 1) then v1..v5; `unsigned g_e4dc, g_e4e0;` -> g_e4e0 (key 38) before
  g_e4dc (101). Initialised data (`_DATA`) stays in definition order and CONST literals stay "data-initialiser
  literals, then code literals by first use" (re-checked with 80 random names/literals: no hash effect).
- Consequences: placeholder names decide the layout; renaming, adding/removing an `#include`, a prototype, a local
  or a string literal before an object can reorder `_BSS`. A size-2 object can never precede a 16-byte one, so
  T14's 100-byte `g_e324` (short at +0 followed by 16-byte arrays) must be one aggregate (a struct), not separate
  variables. Static locals are emitted after all file-scope objects, per function; their inner order is not modelled.
- Other emission orders: PUBDEF = definition order for code/_DATA, layout order for _BSS; EXTDEF = first use in
  code, except that under `-d2` the TU's own global data objects are also EXTDEF'd (from `$$SYMBOLS` fixups) in the
  same hash/group order, in the first EXTDEF record; `-d2` also splits code LEDATA where debug records are flushed
  (without `-d2` one code LEDATA + one EXTDEF record). None of this changes image bytes; only chunk/fixup order.
- Tool: `python tools/bssorder.py NAME[:SIZE][@IDX] ...` (layout), `--solve PATTERN[:SIZE] ...` (names for a
  required address order; `a|b` alternatives, `{}` free suffix), `--src TU.c [--flags ...]` (measures each
  object's idx with prefix probes, compares prediction with the compiler), `--src TU.c --want A,B,.. --rename
  A=PAT --verify` (solve + recompile). Avoid suffixes that make `g_<hex>` names (check.py reads them as addresses):
  use `g_e1d0_{}`.

## `-ot` CONST literal padding = stale source-buffer bytes  (worker `otpad`, probes in `build/workers/otpad/`)

- Only `-ot` 4-aligns each CONST string literal (`-od`, `-ox`, `-os` pack them). The pad bytes after a literal's NUL
  are never written by the compiler: pad byte at CONST offset `k` = stale byte `buf[BASE+k]` of the cfe's 4 KiB
  source-file read buffer, whose memory the CONST data buffer reuses. `buf[i]` = byte `i` of the LAST 4 KiB chunk
  of the main source file that reached index `i` (`file[4096*m+i]`, largest `m` with `4096*m+i < len(file)`; a
  short final read overwrites only the start of the buffer). Beyond EOF of a small file: zeros. Exception:
  `buf[BASE+2]` always reads `01` (overwritten by the compiler; KE's T06 has it too: `00 00 01 DB`).
  The LAST literal of the object gets no pad bytes in the OMF (segment ends at its NUL; the linker pads with 00).
  PROVEN: every pad byte of every probe/TU located in the source text; T06/T08 reproduced byte-exactly.
- `BASE` does not depend on code, comments, literal count/length, identifiers, `#define`s, line count, CRLF/LF or
  environment variables (INCLUDE/WATCOM length included). It moves -32 per `#include` (string.h) and -4 per 4 bytes
  of the compiler's full source path (`roundup4(len)`), and it differs by host: DOS/4GW(DOSBox-X) BASE = NT BASE
  + 104 + (roundup4(NT path) - roundup4(DOS path)); e.g. no-include probe NT 2029/2033 (depends on the checkout
  path), DOS `C:\CAND.C` 2173. Measure it for a TU with `python build/workers/otpad/findbase.py TU.c dosbox`
  (appends position-coded comment text; comments do not move BASE).
- So the pads are deterministic from (source text, source path, #includes, host). The original machine's path and
  file layout are unknown; reconstructions put plausible CP437 comment banners (`█` = 0xDB, CRLF lines) where
  `buf[BASE+k]` must hold the original pad bytes. T08 (`.VGA DB DB DB ... .LBM DB 0D 0A ...`) = two full-width
  banner rows with the row end (CRLF) at `BASE+22`; T06 (`00 00 01 DB`) = one `█` at `buf[BASE+3]`.
- Host rule: only the DOS host (check.py `--host dosbox`: fixed `C:\CAND.C`, `INCLUDE=D:\H`) gives a
  location-independent BASE. The NT host compiles at the real path (check.py: the file's own directory; image.py:
  `src/`), so its BASE changes with the checkout directory and file name; NT cannot even reach the DOS BASE (fixed
  +104 offset). -ot units whose CONST has pad bytes are EXACT only with `--host dosbox`; the canonical build
  (image.py `cache_compile`, validate.py default `nt`) must compile such units on the DOS host with the
  `C:\CAND.C` layout (supervisor: add a per-profile/unit `host` for C in `check.compile_candidate`).

## Data records (data agent, verified through the whole-image link)
- Initialised data LEDATA records are cut on a 172-byte grid at element boundaries; implicit trailing zeros of an
  initialiser are skipped (not emitted) and restart the grid.
- Import flushes (EXTDEF name bytes reaching 192) also happen inside data initialisers (pointer initialisers import
  their targets); after such a flush the next name is written as its own EXTDEF.
- Defining a TU's own data changes its code chunking (own globals are EXTDEF'd up front under -d2 and $$SYMBOLS
  shifts), so name lengths must be re-fitted after moving data into a TU (build/workers/data/refit.py).
