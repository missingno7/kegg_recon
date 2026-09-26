.386
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_12f30
a_12f30:
        pushad
L_12F31:
        lea ebp, [esp + 1Ch]
L_12F35:
        cmp byte ptr [g_e384], 40h
L_12F3C:
        je short L_12F4F
L_12F3E:
        mov byte ptr [g_e384], 40h
L_12F45:
        mov ax, 4005h
L_12F49:
        mov dx, 3CEh
L_12F4D:
        out dx, ax
L_12F4F:
        mov ah, 1
L_12F51:
        mov byte ptr [g_e385], ah
L_12F57:
        mov al, 2
L_12F59:
        mov dx, 3C4h
L_12F5D:
        out dx, ax
L_12F5F:
        mov esi, dword ptr [ebp + 8]
L_12F62:
        mov edi, dword ptr [ebp + 0Ch]
L_12F65:
        mov ecx, dword ptr [ebp + 10h]
L_12F68:
        shr ecx, 2
L_12F6B:
        mov edx, 3
L_12F70:
        movsb byte ptr es:[edi], byte ptr [esi]
L_12F71:
        add esi, edx
L_12F73:
        loop L_12F70
L_12F75:
        inc dword ptr [ebp + 8]
L_12F78:
        add ah, ah
L_12F7A:
        cmp ah, 10h
L_12F7D:
        jne short L_12F51
L_12F7F:
        cmp byte ptr [g_e385], 0Fh
L_12F86:
        je short L_12F99
L_12F88:
        mov byte ptr [g_e385], 0Fh
L_12F8F:
        mov ax, 0F02h
L_12F93:
        mov dx, 3C4h
L_12F97:
        out dx, ax
L_12F99:
        popad
L_12F9A:
        ret
        ORG $+1 ; original zero fill to the next even code address
ASM_TEXT ENDS
        END
