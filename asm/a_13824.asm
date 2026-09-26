.386
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13824
a_13824:
        push ebp
L_13825:
        lea ebp, [esp]
L_13828:
        push eax
L_13829:
        push ecx
L_1382A:
        push edi
L_1382B:
        mov edi, dword ptr [ebp + 8]
L_1382E:
        sub eax, eax
L_13830:
        cmp edi, 0B0000h
L_13836:
        jge short L_13874
L_13838:
        cmp edi, 0A0000h
L_1383E:
        jl short L_13874
L_13840:
        cmp byte ptr [g_e385], 0Fh
L_13847:
        je short L_1385A
L_13849:
        mov byte ptr [g_e385], 0Fh
L_13850:
        mov ax, 0F02h
L_13854:
        mov dx, 3C4h
L_13858:
        out dx, ax
L_1385A:
        cmp byte ptr [g_e384], 40h
L_13861:
        je short L_13874
L_13863:
        mov byte ptr [g_e384], 40h
L_1386A:
        mov ax, 4005h
L_1386E:
        mov dx, 3CEh
L_13872:
        out dx, ax
L_13874:
        mov ecx, dword ptr [ebp + 0Ch]
L_13877:
        shr ecx, 2
L_1387A:
        rep stosd
L_1387C:
        mov ecx, dword ptr [ebp + 0Ch]
L_1387F:
        and ecx, 3
L_13882:
        rep stosb
L_13884:
        pop edi
L_13885:
        pop ecx
L_13886:
        pop eax
L_13887:
        pop ebp
L_13888:
        ret
ASM_TEXT ENDS
        END
