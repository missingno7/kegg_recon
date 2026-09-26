.386
EXTRN g_7b16:WORD
EXTRN g_7b18:WORD
EXTRN g_8360:DWORD
EXTRN g_8368:DWORD
EXTRN g_8374:WORD
EXTRN g_8406:DWORD
EXTRN g_840a:DWORD
EXTRN g_840e:DWORD
EXTRN g_e2e0:DWORD
EXTRN g_e2e4:DWORD
EXTRN g_e2e8:DWORD
EXTRN g_e2ec:DWORD
EXTRN g_e326:DWORD
EXTRN g_e336:DWORD
EXTRN g_e346:DWORD
EXTRN g_e35e:DWORD
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
EXTRN f_12d5f:NEAR
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_12a9c
a_12a9c:
        pushad
L_12A9D:
        lea ebp, [esp + 1Ch]
L_12AA1:
        cmp byte ptr [g_e384], 40h
L_12AA8:
        je short L_12ABB
L_12AAA:
        mov byte ptr [g_e384], 40h
L_12AB1:
        mov ax, 4005h
L_12AB5:
        mov dx, 3CEh
L_12AB9:
        out dx, ax
L_12ABB:
        movzx ebx, word ptr [g_7b16]
L_12AC2:
        shl ebx, 2
L_12AC5:
        mov edi, dword ptr [ebx + g_e326]
L_12ACB:
        add edi, dword ptr [ebx + g_e336]
L_12AD1:
        add edi, dword ptr [ebx + g_e346]
L_12AD7:
        movzx ebx, word ptr [g_7b18]
L_12ADE:
        shl ebx, 2
L_12AE1:
        mov esi, dword ptr [ebx + g_e326]
L_12AE7:
        add esi, dword ptr [ebx + g_e336]
L_12AED:
        add esi, dword ptr [ebx + g_e346]
L_12AF3:
        mov dword ptr [g_e2e4], edi
L_12AF9:
        mov dword ptr [g_e2e0], esi
L_12AFF:
        add edi, dword ptr [ebp + 8]
L_12B02:
        mov eax, dword ptr [g_e35e]
L_12B07:
        mul dword ptr [ebp + 0Ch]
L_12B0A:
        add edi, eax
L_12B0C:
        mov dword ptr [g_8368], edi
L_12B12:
        mov eax, dword ptr [ebp + 8]
L_12B15:
        mov dword ptr [g_8406], eax
L_12B1A:
        mov eax, dword ptr [ebp + 0Ch]
L_12B1D:
        mov dword ptr [g_840a], eax
L_12B22:
        mov edi, dword ptr [ebp + 14h]
L_12B25:
        mov dword ptr [g_8360], edi
L_12B2B:
        mov edi, dword ptr [ebp + 10h]
L_12B2E:
        mov dword ptr [g_840e], edi
L_12B34:
        jmp short L_12B5F
L_12B36:
        movsx ebx, word ptr [edi + 4]
L_12B3A:
        sub ebx, dword ptr [g_8406]
L_12B40:
        movsx edx, word ptr [edi + 6]
L_12B44:
        sub edx, dword ptr [g_840a]
L_12B4A:
        mov ax, word ptr [edi + 8]
L_12B4E:
        mov word ptr [g_8374], ax
L_12B54:
        call f_12d5f
L_12B59:
        mov edi, dword ptr [g_840e]
L_12B5F:
        add dword ptr [g_840e], 0Ah
L_12B66:
        mov esi, dword ptr [edi]
L_12B68:
        cmp esi, 0
L_12B6B:
        jne short L_12B36
L_12B6D:
        mov byte ptr [g_e385], 0Fh
L_12B74:
        mov ax, 0F02h
L_12B78:
        mov dx, 3C4h
L_12B7C:
        out dx, ax
L_12B7E:
        mov eax, dword ptr [g_8360]
L_12B83:
        mov dword ptr [g_e2e8], eax
L_12B88:
        mov eax, dword ptr [g_840e]
L_12B8D:
        mov dword ptr [g_e2ec], eax
L_12B92:
        popad
L_12B93:
        ret
ASM_TEXT ENDS
        END
