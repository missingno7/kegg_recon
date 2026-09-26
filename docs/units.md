# Translation units

A translation unit (TU) is one original .c file = one OMF object. WLINK concatenates each segment class in object
order, so a TU owns one contiguous code range plus one contiguous block in each of CONST, _DATA and _BSS
(obj3 classes: CONST [0x4,0x24C4), _DATA [0x24C4,0x886F), _BSS [0x886F,0xE610), STACK 0x1000). The final
single WLINK link needs every TU as a real source; per-function matches are only a stepping stone.

Watcom 10.0 data layout (proven by probes, `-d2` and `-od` alike):
- CONST: identical literals stored once; literals used in data initialisers first, then code literals in order of
  first use; no padding between; block 4-aligned. `-ot` objects pad each literal to 4 bytes (open problem).
- _DATA: definition order, no padding. _BSS: not definition order (only block boundaries are usable).

Tools
```
python tools/tu.py build --range 0xSTART 0xEND [--literals] [--defs DEFS.c] [--out FILE]
      # merges src/ members into one TU, resolves conflicting declarations (keeps what stays EXACT),
      # --literals turns extern char g_X[] into CONST strings, --defs adds the TU's own data definitions;
      # verified by check.py --all (code + every referenced data segment + pointer fixups)
python tools/tu.py scan --literals      # boundary probes; python tools/tu.py evidence  # CONST/_DATA ownership
python tools/check.py TU.c --all --at 0xSTART --end 0xEND   # the authority
      # [--place CONST=0xOFF --place _DATA=0xOFF] asserts where a TU segment lies when none of the TU's own code
      # references it (e.g. a text pool used only by other TUs); verified by contents + pointer fixups
```
Supervisor promotes an EXACT unit with `python tools/promote.py --unit ID TU.c src/NAME.c --range 0xSTART 0xEND [--place SEG=OFF]`;
member functions then point at the unit (manifest `units`), per-function files are retired, validate checks the
unit as a whole. Current proposal with evidence: build/tus.json (regenerate: `python tools/tu.py verify
build/workers/tu/proposal.json`).

Data definitions are real C: string literals, numeric tables, struct arrays, pointer initialisers (to functions and
data). Never a byte dump of code; a byte table is fine only where the original is a byte table.
