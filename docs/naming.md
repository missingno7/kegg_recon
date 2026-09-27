# Readability and naming pass

The build is complete (`python tools/validate.py --image` = byte-identical KE.EXE, zero raw debt). This pass makes
the canonical sources read like a 1994 C program without ever breaking that gate.

What to change (inside the units you own)
- Names: every function, global/static, struct/union type and field, parameter and local you can understand gets a
  descriptive name (functions `verb_noun` snake_case, e.g. `draw_racket`, `load_bob_file`; globals nouns, e.g.
  `ball_count`, `sb_base_port`; types `CamelCase`). Keep `main`, all Watcom clib names, and names you cannot
  justify (leave `f_HEX`/`g_HEX` rather than inventing meaning).
- Structure: replace lifter artefacts (gotos, redundant casts, `(int)` noise, split declarations) with the natural C
  that compiles to the same bytes; reuse struct types instead of pointer arithmetic; string/data tables as real
  initialisers. Short comments only where they explain game behaviour.
- Evidence for meaning: game strings (obj3), data files (assets/KE_*.BOB/DIG/GIF/PAL), callers/callees, clib calls,
  hardware ports (0x60 keyboard, 0x3C8 VGA DAC, SB DSP base+0xC/0xE, PIT 0x40/0x43), DPMI/int386 calls.

Hard constraints (the gate enforces them; know them to avoid churn)
- Code must stay EXACT: `python tools/check.py UNIT.c --all --at S --end E [--profile P] [--place ...]` (the unit's
  record in manifest.json "units" has range/profile/place).
- Identifier LENGTHS shape object chunking: wcc386 cuts a code LEDATA when pending EXTDEF names reach 192 bytes
  (docs/compiler-notes.md "Object-file layout"). Renaming a symbol other objects import changes THEIR chunking too.
  If the sandbox fails with "LE fixup order", adjust name lengths (a helper: build/workers/objects/lenfit.py).
- Pre-check renames in seconds: `python tools/namefit.py build/workers/NAME/plan.json --compile` recompiles every C
  unit that imports a renamed symbol and names the first rename that changes its object layout (fix that name's
  length, rerun; usually only a handful of importers are near a flush). `--suggest NAME LEN` lists equal-length
  spellings; an equal-length rename is always layout-neutral. Symbols named in T06/T08 are FROZEN (plain audit).
- No `#define readable_name f_HEX` aliases: the real symbol gets the readable name (fit its length instead). The
  fitted placeholder names in the tree (`x_dd40_xbukycw`, `f_608a_ujonj`, `g_dDc8`, ...) are length/hash padding
  from the object-layout phase, not historical names: replace them with readable names of a fitting length/hash.
- _BSS order follows the name hash (`python tools/bssorder.py --src UNIT.c`; `--solve` to pick names).
- Units T06 and T08 (`game-c-ot-dos`) are byte-sensitive source files (leaked source-buffer padding): do not edit.

Workflow
1. Write rewritten unit files under build/workers/NAME/src/ (same file name as in src/).
2. Write build/workers/NAME/plan.json: {"units": [{"id", "file", "dest", "range", "profile", "place"}],
   "renames": {old: new, ...}} — rename ONLY symbols defined in your own units (others' references are rewritten
   automatically by replan); units copied from manifest "units" with "file" pointing at your rewritten source.
3. `python tools/replan.py build/workers/NAME/plan.json --sandbox` — applies everything to a throwaway copy and runs
   validate + the whole-image link; PASS is required. The supervisor applies passing plans to the canonical tree.
