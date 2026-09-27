# Historical cleanup (pre-freeze) — worker instructions

Goal: the cleanest, most understandable representation of the original program that still rebuilds the original
`KE.EXE` exactly. This is NOT the port: no platform abstraction, no SDL concepts in these sources. The frozen result
(tag `historical-exact-clean-v1`) becomes the behavioural oracle for a separate `portable-sdl3` branch.

Read first: README.md, AGENTS.md, docs/evidence.md, docs/compiler-notes.md (object layout, _BSS order, control-flow
layouts), docs/naming.md (rename mechanics). Current debt: `python tools/debt.py` (`--unit ID`, `--list addr|raw|hw`).

## Invariants (the gate enforces them; never weaken a check, never touch expected bytes)
- Every unit EXACT: `python tools/check.py FILE --all --at S --end E [--profile P] [--place ...]` (record in
  manifest.json "units"). Whole image: `python tools/replan.py build/workers/NAME/plan.json --sandbox` must PASS
  (validate 70/70 + 4/4, canonical image IDENTICAL, 0 raw debt). Nothing is promotable without a sandbox PASS.
- Byte-sensitive details (all proven): identifier lengths of imported symbols (LEDATA chunking —
  `python tools/namefit.py PLAN --compile`), _BSS order = hash of names + declaration index (`tools/bssorder.py`;
  adding/removing file-scope declarations before a BSS object shifts its index group!), declaration order, frame slot
  order (block-scope autos are allocated after function-level autos), evaluation order, push idioms.
  Comments, local variable names, parameter names, struct/enum TAG and member names, typedef names and macros
  (#define constants) do not reach code bytes in game-c/game-c-ot units — but a new typedef, enum constant or
  prototype is a symbol-table entry and can shift _BSS grouping: verify.
- src/t06.c and src/t08.c (profile game-c-ot-dos, DOSBox-X host) leak source bytes into literal padding: they are
  editable ONLY under the proven offset rule in docs/compiler-notes.md ("-ot padding: exact source-offset rule"):
  keep the anchor bytes at their offsets, the file length in its band, T08 CRLF. A plan that renames a symbol
  they mention must include the T06/T08 unit itself (replan refuses textual edits to them otherwise).
- TASM modules: label names, EQU constants, STRUC field names and comments do not change bytes as long as the
  instruction encodings stay identical — still verify every module.

## What to do (priority order)
A. Names: functions, globals, locals, params, struct/union types and fields, enums, constants — from behaviour, call
   sites, strings, data files (assets/KE_*), hardware ports. Neutral descriptive names when the evidence is weak
   (`sprite_flags`, not `invincibility_after_spell_7`). Keep `f_HEX`/`g_HEX` only when nothing can be justified —
   and then add a one-line comment saying what is known.
B. Control flow: `python tools/structure.py FILE --out DIR` first; then hand-structure what remains using the
   control-flow rules in docs/compiler-notes.md. Cleaner if exact, historical ugliness if required: never fake
   constructs (dummy loops, `do {} while(0)` tricks) to kill a goto; a goto that no template produces stays, with a
   short comment.
C. Types: replace repeated `*(int *)(p + 0x74)` pointer arithmetic with real struct/array access; reuse the shared type
   catalogue (build/workers/types/TYPES.md when present) so the same struct has the same name/fields in every unit.
D. Magic values: `#define`/enum names for flags, scan codes, ports, modes, states, error codes, formats.
E. Comments: short, on non-obvious behaviour and DOS/hardware specifics; no restating code.

## Do NOT
Create platform wrappers or a Platform object; split, merge, move or reorder functions or translation units; move
globals between units or change their definition order; put shared declarations into new headers unless you prove
the image stays identical; change the call graph; "modernise" (stdint, bool, const-correctness churn); remove
historical oddities that are needed for exactness (keep them, comment why if it helps).

## Deliverable and report
- Rewritten files in build/workers/NAME/src/ (same file names), plan build/workers/NAME/plan.json:
  {"units": [...copied from manifest "units", "file" -> your file...], "renames": {old: new}} (docs/naming.md).
- Report (final answer, <=20 lines; details in build/workers/NAME/REPORT.md): files/functions changed, key semantic
  findings with evidence, debt before/after (`python tools/debt.py --unit ...`), per-unit check result, namefit
  result, sandbox result, anything left unnamed/unstructured and why. Iterate autonomously; partial progress that
  PASSES beats a big change that does not. Sandbox runs take a few minutes: batch your changes.
