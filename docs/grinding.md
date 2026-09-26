# Matching loop (workers)

```
python tools/context.py f_1b7e              # packet: disassembly, proven declarations, callers
python tools/harvest.py build/workers/NAME  # check every candidate in a dir at once
# write C into build/workers/NAME/f_1b7e.c
python tools/check.py build/workers/NAME/f_1b7e.c f_1b7e      # compile (pinned wcc386, profile game-c) + strict compare
python tools/promote.py build/workers/NAME/f_1b7e.c f_1b7e --verify-only
```

Conventions
- Calls into the runtime library (obj1 >= 0x13B9C) must use the real Watcom clib name at that address
  (manifest.json runtime.publics; check.py rejects any other name there, e.g. close() where the original
  calls malloc). Include the proper header; do not invent prototypes for library functions.
- Name unknown functions `f_<obj1 offset hex>` and globals `g_<obj3 offset hex>` (lower-case hex, no
  leading zeros). The verifier checks that such names bind to exactly that address, so the names are
  self-verifying. Semantic renames happen later, centrally.
- A candidate file is self-contained C: declare the externs/prototypes it needs. Types matter: they select
  instructions (`movsx` from `short`, `movzx` from `unsigned char`, pointer scaling, signed/unsigned
  compares and shifts). Prototypes matter for argument pushing.
- Default profile `game-c` = `wcc386 -3s -od -s` (Watcom 10.0 GA). Some regions use other flags (e.g. functions
  4-aligned with `90`/`8bc0`/`8d4000` fillers = `-ot -od`); a profile is a TU property — report it, don't
  hack around it.
- `check.py` output: `EXACT`, or `DIFF` with problems (size, fixup sites, binding conflicts, byte diffs) and
  aligned instruction islands (`[replace] orig: ... cand: ...`). JSON detail in `build/check/FUNC/result.json`
  or `--json PATH`.
- Only EXACT counts. Record the best non-exact candidate with
  `python tools/promote.py CAND FUNC --draft "what remains"` only when asked; otherwise report it.

Report (<=20 lines): functions attempted with verdicts; for non-exact ones the exact remaining mismatch
and what you tried; candidate paths; any global clue (compiler behaviour, type of a global, TU flags).
