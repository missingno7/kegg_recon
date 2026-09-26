.386P
REFDATA SEGMENT PARA PUBLIC USE32 'DATA'
EXTRN g_8418:DWORD
EXTRN g_841c:DWORD
EXTRN g_8420:DWORD
REFDATA ENDS
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT, DS:REFDATA
        PUBLIC a_137a8
a_137a8:
        pushad
L_137A9:
        lea ebp, [esp + 1Ch]
L_137AD:
        cli
L_137AE:
        mov dword ptr [g_8418], 386h
L_137B8:
        pushfd
L_137B9:
        mov ebp, esp
L_137BB:
        and sp, 0FFFCh
L_137BF:
        pushfd
L_137C0:
        cli
L_137C1:
        pop eax
L_137C2:
        mov ebx, eax
L_137C4:
        xor eax, 40000h
L_137C9:
        push eax
L_137CA:
        popfd
L_137CB:
        pushfd
L_137CC:
        pop eax
L_137CD:
        xor eax, ebx
L_137CF:
        mov esp, ebp
L_137D1:
        popfd
L_137D2:
        test eax, 40000h
L_137D7:
        je short L_137E3
L_137D9:
        mov dword ptr [g_8418], 486h
L_137E3:
        mov dword ptr [g_841c], 0
L_137ED:
        smsw ax
L_137F0:
        test al, 1
L_137F2:
        je short L_13811
L_137F4:
        mov dword ptr [g_841c], 1
L_137FE:
        pushfd
L_137FF:
        pop eax
L_13800:
        test eax, 20000h
L_13805:
        je short L_13811
L_13807:
        mov dword ptr [g_841c], 2
L_13811:
        pushfd
L_13812:
        pop eax
L_13813:
        shr eax, 0Ch
L_13816:
        and eax, 3
L_13819:
        mov dword ptr [g_8420], eax
L_1381E:
        sti
L_1381F:
        popad
L_13820:
        ret
        ORG $+3 ; original zero fill to the next aligned entry at 13824h
ASM_TEXT ENDS
        END
