# Historical freeze: `historical-exact-clean-v1`

The canonical source at this tag is the permanent behavioural and historical oracle for Krypton Egg. It is the
cleanest representation of the original program found that still rebuilds the original `KE.EXE` byte for byte with
the historical toolchain. The SDL3 port is a separate branch (`portable-sdl3`) that starts from this point
(docs/porting.md); nothing in this tree contains platform abstractions.

## Identity

| Item | Value |
|---|---|
| Validated source commit | `3db695f57c44a3b61b17408c02edbf3393b8f55e` (the tag commit adds only this record) |
| Original `KE.EXE` SHA-256 | `5a465cc78d7ef797934f4a95029a278ff10c1ae547ce0cd067eb8c076f50b7ee` (pinned in tools/oracle.py) |
| Rebuilt `KE.EXE` SHA-256 | `5a465cc78d7ef797934f4a95029a278ff10c1ae547ce0cd067eb8c076f50b7ee` — identical |
| Linked objects | 71 (49 C units, 21 TASM units, 1 16-bit IRQ module) + Watcom 10.0 GA runtime libraries + GA wstub |
| Raw debt | 0 bytes (72 bytes of proven WLINK alignment fill are the only bytes not owned by an object) |
| Functions | 378/378 matching (325 C, 2 C interrupt handlers, 51 assembly) — 80,896 bytes of game code |

## Toolchain (pinned; `python toolchain/install.py --verify`)

| Tool | Identity |
|---|---|
| Watcom C/C++32 10.0 GA (`wc100`) | install tree `5498d143…8157` (2,965 files, dated 1994-05-31); `BINB/wcc386.exe a4f4da1b…bf65`, `BINNT/wlink.exe bed77d1c…d489`, `BINB/wstub.exe 44e0585e…99da`, `LIB386/DOS/clib3s.lib 894ade97…18cb0` (all key files in toolchain/toolchain.json) |
| C profiles | `game-c` = `wcc386 -3s -d2 -s`; `game-c-ot` = `-3s -ot -d2 -s`; `game-c-ot-dos` = same flags on the DOSBox-X host (T06, T08: `-ot` literal padding leaks source bytes, docs/compiler-notes.md) |
| Borland Turbo Assembler 3.1 | `TASM.EXE 80534cd6…74ea` (identical to Borland C++ 3.1 floppy media); profile `game-asm-tasm31` = `/ml` under MS-DOS Player — the only assembler (MASM-only builds proven to change the LE fixup order) |
| Linker | WLINK 10.0 GA, `system dos4g`, `op stub=wstub.exe`, `heapsize=20000` (toolchain/ke.lnk) |
| Runners | DOSBox-X `b028a4d3…2bf6`; MS-DOS Player `f7f6cb0a…bd02`; capstone 5.0.7 tree `65446f83…c800` |

## Freeze validation (fresh clone of the validated commit, D:/kgfreeze2)

| Command | Result |
|---|---|
| `python toolchain/install.py --verify` | all OK |
| `python tests/run.py` | all passed: 8 negative controls (exit codes), 4 positive controls, oracle tamper refused, whole-image negative (one case-flipped letter → `validate --image` fails, 1 byte differs) |
| `python tools/validate.py --image --fresh` | 70/70 units + 4/4 separate functions EXACT; image IDENTICAL; 71 objects compiled fresh, 0 cache hits; raw 0 |
| `python tools/validate.py --host dosbox` | 70/70 units + 4/4 separate functions EXACT on the independent DOSBox-X host |
| `python tools/portmap.py --check` | 378 manifest functions mapped exactly once |
| `python tools/structs.py` | 0 same-name type conflicts; 1 member-type variant (proven, below) |

## State of the source

| Debt measure (`python tools/debt.py`) | At cleanup start | At freeze |
|---|---|---|
| Address-derived identifiers | 3,721 | 9 (7 names, all justified) |
| Placeholder locals/params | 31 | 0 |
| gotos | 102 | 50 (all reviewed, layout-required) |
| Raw pointer-offset dereferences | 166 | 1 |
| Pointer casts | 750 | 282 |
| Hex literals | 1,820 | 1,390 (mostly data tables, hardware registers, packed constants) |

Reconstructed types: docs/types.md (one declaration per shared struct across all units). Portability: docs/porting.md
and docs/porting.json (KEEP / ADAPT / REIMPLEMENT for all 378 functions, keyed by object + start address;
`python tools/portmap.py --refresh` keeps names current).

## Intentional historical material

- **Assembly (all TASM 3.1):** m_0982c (asset checksum decode), m_09f64 (PIT channel-0 timing), m_0a284 (IFF/ILBM
  decode), m_0a958 (no-op renderer hooks), m_11258 / m_11494 (Sound Blaster IRQ, DMA, rate, speaker, PIC EOI),
  m_11530 (ProTracker MOD player), m_11df8 (GIF LZW decoder), m_12288 / m_12684 / m_12974 (sprite-record renderers),
  m_12a9c (sprite update lists, BOB drawing), m_12f30 / m_12f9c (scanline and screen-span copies), m_13324 /
  m_13712 (VGA pixel read/write/fill), m_137a8 (CPU environment probe, video clear, memory move), m_13944 / m_13a48
  (VGA register updates, DAC palette), irq.asm (16-bit real-mode IRQ templates in LE object 2). They were assembly
  in the original.
- **T06 / T08 anchor bytes:** CP437 0xDB bytes held at fixed source offsets in comment banners (T06: offset 10336;
  T08: 2146-2148, 2154-2156, 2162-2164, 2170-2172, 2178-2180) because `-ot` pads CONST literals with stale bytes of
  the compiler's source-read buffer (proven rule: docs/compiler-notes.md). T08 keeps CRLF line endings.
- **Remaining names, gotos and the type variant:** docs/unresolved.md — 7 address-derived names (unknown roles, one
  declaration required by the `_BSS` layout), 50 gotos whose jump layouts no structured form reproduces (confirmed by
  two independent reviews and tools/structure.py), and `EnemyProjectile.animation_sequence` declared `int` in U05966
  (proven: its byte-cursor arithmetic requires it).
- **Fitted spellings:** some identifiers have lengths/hashes chosen to preserve object layout (LEDATA chunking by
  imported-name length, `_BSS` hash order) — e.g. short names in U06b02 and `mov_mem`; `tools/namefit.py` and
  `tools/bssorder.py` explain each constraint.

## Verification-chain notes (audit, build/workers/audit + hardening)

The oracle is pinned; `--fresh` bypasses the object cache; every launched tool, payload and runner is hash-checked;
`install.py` fails closed; replan refuses textual edits to T06/T08 unless the plan includes them; promotion requires
the whole-image gate. Known limits: DOSBox-X/MS-DOS Player DLLs and configuration are not pinned; `validate --host
dosbox` re-verifies units on DOSBox-X but the whole-image link uses each profile's pinned host; the original
Watcom 10.0 GA retail media is not found (the install tree is corroborated by an independent copy).
