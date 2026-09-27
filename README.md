# Krypton Egg (DOS, 1994) — matching reconstruction

Reconstruct source for `KE.EXE` (Watcom C/C++32 10.0-era, DOS/4GW LE executable) that the historical
toolchain rebuilds into the original bytes — ultimately a byte-identical executable from one WLINK run.

**Status: frozen historical oracle (`historical-exact-clean-v1`).** The cleaned, semantically named source rebuilds
`KE.EXE` byte for byte (SHA-256 `5a465cc7…50b7ee`) from 71 objects with zero raw debt — see docs/freeze.md. Types:
docs/types.md; remaining historical oddities: docs/unresolved.md; SDL3 migration map: docs/porting.md.

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
| `port/` | branch `portable-sdl3`: Windows/SDL3 port over a small virtual PC - docs/port/architecture.md, docs/port/workpackages.md |

## Reproduce

```
git clone <repo> && cd <repo>          # any path (compiles run in a short scratch dir, KEGG_TMP or <drive>:\kgtmp)
# put the original game files into assets/ (KE.EXE: 210031 bytes, sha256 below; every verifier refuses any other file)
python toolchain/install.py --verify   # installs, external runners and capstone against their pins; exit 1 on any problem
python tools/validate.py --image --fresh   # FREEZE GATE: every claimed unit/function EXACT, then every object freshly
                                           # compiled (no object cache) and one WLINK run == KE.EXE, raw debt 0
python tests/run.py                    # verifier self-tests (negatives, positives, isolated whole-image negative)
python tools/status.py                 # progress and raw-debt accounting
```

`python tools/validate.py --image` (without `--fresh`) is the fast iteration gate: it reuses
`build/image/objcache` objects whose sidecar metadata re-verifies (source hash, profile, launched tool hashes,
install lock, Watcom `H/` tree hash). Only `--fresh` is used to freeze or to accept a toolchain change.

### External dependencies (all pinned; nothing is fetched automatically)

| dependency | location | pin (full SHA-256) | enforced by |
|---|---|---|---|
| original `KE.EXE` (oracle) | `assets/KE.EXE` (user supplied, ignored) | `5a465cc78d7ef797934f4a95029a278ff10c1ae547ce0cd067eb8c076f50b7ee` (210031 bytes) | `tools/oracle.py` (check.py, image.py fail closed); also `manifest.json` `original` |
| Watcom C/C++32 10.0 GA (compiler `wcc386`, `wlink`, clib3s/math387s/emu387, `wstub`, `H/`) | `C:/tools/watcom-10.0` | tree `5498d143742753b52bdc57c5f76f27d1da224b1996e40ad316fa1a5f681a8157` (2965 files); archive `deaddoomer_WATCOM.zip` `7bf0b04c1d076400bf998a8ad2e2fe1fb7e4d61382b7079b07ed5b282e17a27b` | `install.py --verify`; every launched image (`BINNT/*` stub, `BINB/*` payload, `BIN/wlink.exe`, `BIN/dos4gw.exe`) and each library by `tools/dosrun.py` / `image.py` before use |
| Borland TASM 3.1 (all `game-asm-tasm31` modules) | `C:/tools/watcom-10.0/BIN/TASM.EXE` | `80534cd6a1e2c2b4376ae3f21954c6dbfa77a6998d5e0bf8b8bee6153f3174ea` | `install.py`, `dosrun.py` per run |
| DOSBox-X 2022.09.0 (host of `game-c-ot-dos`: `src/t06.c`, `src/t08.c`) | `C:/tools/dosbox-x/dosbox-x.exe` (or `KEGG_DOSBOX_X`) | `b028a4d328302ea270722dec69f3ec3a10ebf76beba72a51970db33a88df2bf6` | `install.py`, `dosbox.py` / `dosrun.py` per run |
| MS-DOS Player (ReC98 P0281 build; host of TASM/MASM) | `C:/tools/nmlgcdos/msdos.exe` | `f7f6cb0a3e816c5edb13112d327c1bddbf7463fe7bf9a005ca1eb5317751bd02` | `install.py`, `dosrun.py` per run |
| capstone 5.0.7 (py3-none-win_amd64 wheel; disassembly, rel32 decoding) | `build/pylib` (ignored) | tree `65446f83f0aef8bc765ee6c6ff6e4bbe082e34c3343d2b25fbbce3a34463c800` (65 files, `__pycache__` excluded) | `install.py --verify` |
| control installs (not used by the canonical build): Watcom 10.0a, 9.5b, MASM 5.00/5.10/5.10A/6.00, TASM 1.00/1.01/2.00/2.01 | `C:/tools/...` | trees / key files in `toolchain/toolchain.json` | `install.py --verify` (missing = failure) |

Provisioning: the Watcom trees are recreated from their pinned archives with `python toolchain/install.py --install
NAME` (needs `7z`; archives under `C:/tools/download/...`, sources and provenance in `toolchain.json`).
Capstone: `python -m pip install --no-user --target build/pylib capstone==5.0.7` on a networked machine, then
`install.py --verify` checks the tree. DOSBox-X and MS-DOS Player are installed from their upstream releases
and must hash to the pins above. Python 3.10+ (no other packages).

## Toolchain

Historical installs live outside git under `C:/tools` (override `KEGG_TOOLS`), see
`toolchain/toolchain.json`. `python toolchain/install.py --verify` verifies them (tree hashes, key files,
runner and capstone pins; `--quick` skips full trees) and exits 1 on any failure;
`--install NAME` recreates one from its pinned source archive (fatal if the result does not verify).

```
python tools/dosrun.py wcc386 -3s -d2 -s foo.c      # pinned tool, clean environment, every launched image hash-checked
```

## Tools

| command | purpose |
|---|---|
| `python tools/context.py NAME` | worker packet: annotated disassembly, proven declarations, caller usage, best draft |
| `python tools/check.py CAND NAME` | strict verifier for one function (`--all --at A --end B` for a whole TU incl. its data) |
| `python tools/harvest.py DIR...` | check every candidate in worker dirs at once (`--promote`: supervisor only) |
| `python tools/promote.py ...` | single-writer promotion of EXACT functions / asm / units (`--unit`: rolled back unless `validate --image` passes) |
| `python tools/validate.py [--host dosbox] [--image [--fresh]]` | regression gate over everything claimed; `--image` also links the whole EXE; `--fresh` without object cache (freeze) |
| `python tools/image.py --mode canonical [--fresh]` | one WLINK run: canonical sources + explicit raw debt + GA libs -> must equal KE.EXE |
| `python tools/lift.py NAME --refine` | automatic `-d2` decompiler (first drafts; many EXACT) |
| `python tools/tu.py build --range A B` | synthesise a whole translation unit from matched members |
| `python tools/replan.py PLAN.json [--sandbox]` | apply a unit/rename plan atomically (sandbox: full gate in a new unique throwaway copy; renames touching host-pinned sources are refused) |
| `python tools/namefit.py PLAN.json --compile` | rename pre-check: which renames change an importer's object layout |
| `python tools/structure.py src/X.c --out DIR` | verified goto -> structured C rewriting (docs/compiler-notes.md control-flow rules) |
| `python tools/status.py` | progress from manifest.json |
| `tools/le.py`, `omf.py`, `omfwrite.py`, `libscan.py`, `inventory.py`, `tumap.py`, `show.py`, `dosrun.py`, `dosbox.py` | readers/writers and runners |

Docs: `docs/evidence.md` (what is proven), `docs/compiler-notes.md` (Watcom idioms), `docs/grinding.md` and
`docs/units.md` (worker workflows), `docs/bootstrap.md` (lessons from earlier projects).
