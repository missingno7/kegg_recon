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
