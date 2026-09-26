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
| bin-lib-gaps | cstart (0x142D8..), ~0x14617-0x14705, ~0x15CA5-0x16292, ~0x1A7DD-0x1B2B1 do not match any 10.0a member -> differing module versions | PROVEN (that they differ); cause open | libscan gaps |
| bin-regions | obj1 0x10..~0x112F9: ~325 functions with Watcom `/od` shape; ~0x112FA..0x13B9B: hand-written 32-bit asm (pushad/`lea ebp,[esp]`/`enter` prologues); 0x13B9C..end: runtime library | STRONG | prologue scan; to be replaced by tools/inventory.py |
| bin-obj2 | obj2 = 4 real-mode IRQ handler templates (SB DSP x2, timer, keyboard), each paragraph-aligned, ORG-0 addressing, placeholder `mov ax,1234h` / `jmp far 1122:3344` patched at run time; no fixups | STRONG | 16-bit disassembly |
| bin-stub | stub is a Watcom WSTUB-like 16-bit program (CRT "1988-1993"), contains `dos4g.exe`, lacks the `WATCOM patch level .a` marker; differs from 9.5b (10386 B) and 10.0a (10822 B) WSTUB.EXE | PROVEN (difference); origin HYPOTHESIS | strings/size |
| bin-dos4gw | shipped DOS4GW.EXE is DOS/4GW 1.95 ("patch level .b"); 10.0a ships 1.97 | PROVEN | strings |

## Toolchain

| id | claim | level | evidence |
|---|---|---|---|
| tc-family | Watcom C/C++32 10.0-era compiler, runtime and linker | STRONG | bin-crt, bin-lib |
| tc-version | exact release is 10.0 GA (pre-'a') rather than 10.0a | HYPOTHESIS (leading) | bin-stub, bin-lib-gaps; 10.0 GA media not yet found |
| tc-95b | 9.5b runtime | EXCLUDED (runtime) | bin-lib |
| tc-3s-od | game C compiled `wcc386 -3s -od` (stack calling convention, no optimisation); `-s` (no `__CHK` calls) | STRONG | 10.0a `-3s -od -s` reproduces the exact prologue `push ebx/esi/edi/ebp; mov ebp,esp; sub esp,imm32` (even `sub esp,0`) and the for-loop layout seen at obj1:0x10 (main); no `__CHK` calls observed. No function matched yet |
| tc-host | NT-hosted and DOS/4GW-hosted runs of the same bound image produce identical objects | HYPOTHESIS | to be tested with DOSBox-X |
| tc-msdos-player | MS-DOS Player cannot run DOS/4GW-hosted Watcom tools | PROVEN | silent failure, no output (see tools/dosrun.py docstring) |
