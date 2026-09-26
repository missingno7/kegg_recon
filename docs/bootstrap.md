# Bootstrap note: what we take from earlier projects

Written 2026-09-26 from audits of `empires_reconstruction`, `stunts_recon`, `icytower_recon` and
`simantw_recon` (full audit reports: `build/workers/audit/*/REPORT.md`, not tracked). Krypton Egg is the
first 32-bit Watcom / LE / DOS/4GW target; nothing below is copied code, only method.

## KEEP

- **Immutable original + hashes.** The original files are the oracle; nothing ever regenerates expected
  bytes from a candidate. Oracle identities live in `manifest.json`.
- **Pinned tools by hash, outside git.** Every executed tool image and library is hash-checked at run
  time (`toolchain/toolchain.json`, `tools/dosrun.py`). Tested-tool identity is kept distinct from the
  historical tool identity (a match proves a usable profile, not the version the developers used).
- **Evidence levels.** proven / strongly suggested / hypothesis, with falsifiers (`docs/evidence.md`).
  A register of competing hypotheses; "non-discriminating" results are recorded as such.
- **Controls.** Already-matching, *unrelated* functions are regression controls for every profile change.
  Probe features that discriminate (signed division, shifts, switch tables, x87, struct copies), not
  trivial getters.
- **Strict exactness.** Exact = every byte of the complete extent equal after *independently* resolved
  fixups; fixup sites/types equal; symbol bindings consistent. Masked / normalized / aligned diffs are
  diagnostics only (all four projects converged on this).
- **Four-command loop** (stunts/simantw after simplification): context -> search/check -> promote -> validate.
  Workers own their scratch dir and hypotheses, no attempt quotas.
- **Single writer for canonical state**, fresh re-verification at promotion (no reuse of a previously
  tested file hash, no caches in acceptance).
- **Raw-byte debt made explicit** and never counted as reconstruction; C, ASM, pinned runtime, raw
  reported separately.
- **Library members identified by extraction from pinned archives**, not by pasting bytes
  (`tools/libscan.py`). Empires showed a real single-link exact build is reachable; that is the end state
  here as well (WLINK is available).

## ADAPT

- **Tool runner.** MS-DOS Player cannot host DOS/4GW tools (tested). Watcom 10 ships the *same* bound
  compiler image with a Win32 loader stub (`BINNT/X.EXE` loads `..\BINB\X.EXE`), so the primary host
  is native; DOSBox-X running the DOS/4GW-hosted image is the independent cross-check (the stunts
  "second host" idea). MS-DOS Player remains the host for real-mode 16-bit tools.
- **Binary layer.** New small readers: `tools/le.py` (LE objects/pages/fixups) and `tools/omf.py`
  (OMF16/32 incl. Easy-OMF). Coordinates are always (LE object, offset); file offsets only in le.py.
- **Function verification.** Flat model makes binding simpler than 16-bit: compare bytes; absolute
  fixups must coincide with LE fixups at the same sites; `rel32` targets are decoded from original bytes;
  every candidate symbol/segment binds to one original address.
- **Ownership manifest.** One `manifest.json` with functions/regions (addr, size, kind, status, source,
  profile, mismatch). Per-byte partition only once a hybrid/whole-image build exists.
- **TU discovery.** Empires-style adjacent-run probes (whole TU compiled, all publics/extents/data checked),
  using Watcom-specific signals (CONST/_DATA order, string pools, static data locality).
- **Linker.** Treat WLINK behaviour as an experiment (stub, object order, alignment, fixup encoding),
  then aim for one natural link.

## DROP

- Queues, cards, attempt budgets, parking/reopen states, epochs, readiness gates, model-specific
  handoff docs (all removed by simantw/stunts migrations; they measured process, not evidence).
- Per-attempt tracked trees and giant tracked reports (simantw: 19,866 tracked job files, 44 MB
  verified-objects dump). Detail goes to ignored `build/`; git keeps small canonical facts.
- Multiple competing status ledgers or duplicated human+JSON dashboards.
- Per-function facade files as the final source form (empires later merged into real TUs).
- MZ/NE/PE/COFF/DWARF parsers, 16-bit frame/selector binders, TLINK/LINK heuristics, Turbo C/MSC flag
  lore — none applies to Watcom 32-bit flat code.
- Staged/adapter link harnesses as production machinery; fixed-placement builders only as diagnostics.
