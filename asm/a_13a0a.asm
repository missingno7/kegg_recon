.386
EXTRN g_e386:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13a0a
a_13a0a:
        push ebp
L_13A0B:
        lea ebp, [esp]
L_13A0E:
        push edx
L_13A0F:
        mov al, 4
L_13A11:
        mov ah, byte ptr [ebp + 8]
L_13A14:
        and ah, 3
L_13A17:
        mov byte ptr [g_e386], ah
L_13A1D:
        mov dx, 3CEh
L_13A21:
        out dx, ax
L_13A23:
        pop edx
L_13A24:
        pop ebp
L_13A25:
        shr eax, 8
L_13A28:
        ret
ASM_TEXT ENDS
        END
