# Krypton Egg (DOS, 1994) — matching reconstruction

Reconstruct source for `KE.EXE` (Watcom C/C++32 10.0-era, DOS/4GW LE executable) that the historical
toolchain rebuilds into the original bytes — ultimately a byte-identical executable from one WLINK run.

## Layout

| path | content |
|---|---|
| `assets/` | original game files (ignored; user supplied) |
| `manifest.json` | the one canonical status file: original identities, functions/regions, status |
| `src/`, `asm/` | canonical reconstructed C / hand-written assembly (only verified material) |
| `tools/` | small deterministic tools (see below) |
| `toolchain/` | `toolchain.json` (pinned installs, hashes, provenance, profiles), `install.py` |
| `docs/` | `evidence.md` (hypothesis register), `bootstrap.md` (lessons from earlier projects) |
| `build/` | everything generated, worker scratch dirs (ignored) |

## Toolchain

Historical installs live outside git under `C:/tools` (override `KEGG_TOOLS`), see
`toolchain/toolchain.json`. `python toolchain/install.py` verifies them (tree hashes);
`--install NAME` recreates one from its pinned source archive.

```
python tools/dosrun.py wcc386 -3s -d2 -s foo.c      # pinned tool, clean environment
```

Setup: `python -m pip install --no-user --target build/pylib capstone==5.0.7` (tools import capstone from there).

## Tools

| command | purpose |
|---|---|
| `python tools/context.py NAME` | worker packet: annotated disassembly, proven declarations, caller usage, best draft |
| `python tools/check.py CAND NAME` | strict verifier for one function (`--all --at A --end B` for a whole TU incl. its data) |
| `python tools/harvest.py DIR...` | check every candidate in worker dirs at once (`--promote`: supervisor only) |
| `python tools/promote.py ...` | single-writer promotion of EXACT functions / asm / units (`--unit`) |
| `python tools/validate.py [--host dosbox] [--image]` | regression gate over everything claimed; `--image` also links the whole EXE |
| `python tools/image.py --mode canonical` | one WLINK run: canonical sources + explicit raw debt + GA libs -> must equal KE.EXE |
| `python tools/lift.py NAME --refine` | automatic `-d2` decompiler (first drafts; many EXACT) |
| `python tools/tu.py build --range A B` | synthesise a whole translation unit from matched members |
| `python tools/status.py` | progress from manifest.json |
| `tools/le.py`, `omf.py`, `omfwrite.py`, `libscan.py`, `inventory.py`, `tumap.py`, `show.py`, `dosrun.py`, `dosbox.py` | readers/writers and runners |

Docs: `docs/evidence.md` (what is proven), `docs/compiler-notes.md` (Watcom idioms), `docs/grinding.md` and
`docs/units.md` (worker workflows), `docs/bootstrap.md` (lessons from earlier projects).
