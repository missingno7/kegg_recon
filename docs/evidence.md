# Evidence register

Levels: **PROVEN** (reproducible from the original bytes or a controlled experiment, command given),
**STRONG** (several independent observations agree, no contrary evidence, not yet a controlled proof),
**HYPOTHESIS** (plausible, untested or weakly tested). **EXCLUDED** = falsified. Update in place; keep
entries short and give the command/evidence that decides them.

## Original binary

| id | claim | level | evidence |
|---|---|---|---|
| bin-le | `KE.EXE` = MZ stub (10648 bytes incl. pad) + LE header at 0x2998; 3 objects, 38 pages of 4096, no trailing bytes | PROVEN | `python tools/le.py assets/KE.EXE` |
| bin-objects | obj1 code 32-bit R-X vsize 0x1BCD1 @0x10000; obj2 16-bit R-X vsize 0x149 @0x30000; obj3 data 32-bit RW vsize 0xF610 @0x40000, init 34927 bytes, rest BSS/stack; EIP obj1:0x142D8, ESP obj3:0xF610 | PROVEN | le.py |
| bin-fixups | 6296 fixups, all internal: 6292 off32 (5833 with 16-bit target offset, 459 with 32-bit), 4 sel16 (obj1 -> obj3). No imports | PROVEN | le.py fixup_histogram |
| bin-crt | Watcom C/C++32 runtime linked: BEGTEXT `int3; jmp $` thunk at obj1:0, `__nullarea` 01 01 01 00 at obj3:0, cstart at entry with "WATCOM C/C++32 Run-Time system ... 1988-1994" | PROVEN | disassembly of obj1:0 and 0x142D8 |
| bin-lib | 96 library code segments from **10.0a** CLIB3S/MATH387S/EMU387 match byte-exactly (fixups masked) in obj1 0x13B9C..0x1BCD1; 9.5b libraries match 2 trivial modules only | PROVEN | `python tools/libscan.py assets/KE.EXE <libs>` |
| bin-lib-gaps | cstart (0x142D8..), ~0x14617-0x14705, ~0x15CA5-0x16292, ~0x1A7DD-0x1B2B1 do not match any 10.0a member | PROVEN; explained by bin-lib-ga |
| bin-runtime | obj1 0x13B9C..0x1BCD1 = 104 Watcom 10.0 GA library members + 8 zero alignment fills, no gaps; 48 runtime data placements match; DGROUP order BEGDATA, CONST, CONST2, _DATA, XIB..YIE, _BSS, STACK; game contributions precede runtime in each class | PROVEN (masked code equality, data equality) | build/workers/crtdiff/REPORT.md, manifest.json `runtime` |
| bin-lib-ga | With **10.0 GA** libraries (clib3s/math387s/emu387) 100 members match exactly incl. cstrt386 at 0x142D8; remaining gaps are <=36 bytes | PROVEN | `python tools/libscan.py assets/KE.EXE C:/tools/watcom-10.0/LIB386/DOS/clib3s.lib C:/tools/watcom-10.0/LIB386/math387s.lib C:/tools/watcom-10.0/LIB386/DOS/emu387.lib` |
| bin-regions | obj1 0x10..~0x112F9: ~325 functions with Watcom `/od` shape; ~0x112FA..0x13B9B: hand-written 32-bit asm (pushad/`lea ebp,[esp]`/`enter` prologues); 0x13B9C..end: runtime library | STRONG | prologue scan; to be replaced by tools/inventory.py |
| bin-obj2 | obj2 = 4 real-mode IRQ handler templates (SB DSP x2, timer, keyboard), each paragraph-aligned, ORG-0 addressing, placeholder `mov ax,1234h` / `jmp far 1122:3344` patched at run time; no fixups | STRONG | 16-bit disassembly |
| bin-stub | stub load image (KE[0x60:0x2992]) == 10.0 GA `BINB/wstub.exe` load image (wstub[0x40:]); identical 6 relocations; header re-laid (relocs at 0x40, 6-paragraph header, e_lfanew 0x2998 at 0x3C), file padded 10642 -> 10648 | PROVEN | byte comparison (see git log for command) |
| bin-dos4gw | shipped DOS4GW.EXE (1.95) is byte-identical to Watcom 10.0 GA `BIN/dos4gw.exe`; 10.0a ships 1.97 | PROVEN | sha256 580f164b... both |

## Toolchain

| id | claim | level | evidence |
|---|---|---|---|
| tc-family | Watcom C/C++32 10.0-era compiler, runtime and linker | PROVEN | bin-crt, bin-lib |
| tc-version | runtime, stub and DOS/4GW are from Watcom **10.0 GA** (1994-05-31 files), not 10.0a | PROVEN for runtime/stub/extender | bin-lib-ga, bin-stub, bin-dos4gw |
| tc-compiler-version | game C compiled by the 10.0 GA wcc386 | STRONG (consistent; 10.0 and 10.0a give identical code on all controls so far — non-discriminating) | release consistency with tc-version |
| tc-95b | 9.5b runtime | EXCLUDED (runtime) | bin-lib |
| tc-3s-od | most game C compiled `wcc386 -3s -od -s` | STRONG | 9 unrelated functions EXACT (strict verifier) incl. calls, globals, string literal, compare/branch: f_4cd0 f_7032 f_6e13 f_105a7 f_ca51 f_1b7e f_2250 f_ca6a (+ -ot cluster near-miss). `-s`: no `__CHK` calls anywhere |
| tc-ot-cluster | functions in obj1 ~0x9960..0xA8BC are 4-aligned with 90/8BC0/8D4000 fillers: reproduced by `-3s -ot -od -s` (order matters: `-od -ot` optimises) | STRONG (shape reproduced; no EXACT yet) | probe p4.c; f_9ca4 differs only in stack-slot order |
| tc-host | NT-loader and DOS/4GW (DOSBox-X) runs of the same bound compiler/assembler give identical segment data and FIXUPPs (only THEADR/COMENT source-path metadata differs) | PROVEN | 30 C objects x 3 flag sets + WASM (build/workers/dosbox/REPORT.md); `python tools/validate.py --host dosbox` re-proves every match on DOSBox-X |
| tc-asm | hand-written asm assembled by **MASM 5.x** (5.00/5.10/5.10A indistinguishable so far; profile game-asm = MASM 5.10A /ML under MS-DOS Player): obj2 IRQ module and 14 obj1 routines EXACT; MASM 6.00 differs (a_a284), TASM 1.01/2.01 and WASM 10.0 do not reproduce the direction-bit encodings | STRONG | build/workers/asm/REPORT.md; asm/*.asm validate EXACT |
| tc-msdos-player | MS-DOS Player cannot run DOS/4GW-hosted Watcom tools | PROVEN | silent failure, no output (see tools/dosrun.py docstring) |

## Linker (WLINK 10.0)

| id | claim | level | evidence |
|---|---|---|---|
| link-stub | WLINK with `option stub=` 10.0 GA `wstub.exe` produces KE's stub byte-exactly: header +32 bytes, `e_lfanew` = align_up(MZ size + 32, 8) = 0x2998 | PROVEN | build/workers/wlink (stub-ga link, sha256 3d127845...) |
| link-header | module_flags 0x200, page 4096, zero checksums, resident name `ke` (from `name ke.exe`), empty entry table, auto-data = DGROUP object: WLINK `system dos4g` defaults; LE+0xA8 = 20000 requires `option heapsize=20000` | PROVEN | controlled links |
| link-objects | 3 objects = USE32 CODE (BEGTEXT first) / USE16 CODE / DGROUP(+BSS+STACK); bases 0x10000 then next 64 KiB boundary; file pages padded to 4096, BSS/STACK virtual only | PROVEN | controlled links reproduce KE's bases and page map |
| link-fixups | one record per site (no source lists); 32-bit target flag exactly for targets >= 0x10000; page-crossing fixup recorded on the next page with negative offset (KE has 6); per-page record order follows contribution order, not sorted | PROVEN | fixup stress links |
| link-stack | STACK segment is obj3 0xE610..0xF610 = 4096 bytes = WLINK/clib default (no `option stack` needed) | STRONG | runtime _BSS ends at 0xE610 (build/workers/crtdiff/runtime-ga.json) |
