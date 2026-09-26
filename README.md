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
python tools/dosrun.py wcc386 -3s -od -s foo.c      # pinned tool, clean environment
```

## Tools

- `tools/le.py` — LE reader (header, objects, pages, fixups). `python tools/le.py assets/KE.EXE`
- `tools/omf.py` — OMF object/library reader (Watcom 32-bit, Easy OMF).
- `tools/libscan.py` — find runtime library members inside the original code object.
- `tools/dosrun.py` — run a historical tool deterministically.
