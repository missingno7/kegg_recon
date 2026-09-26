.386
EXTRN g_e384:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13a29
a_13a29:
        push ebp
L_13A2A:
        lea ebp, [esp]
L_13A2D:
        push edx
L_13A2E:
        mov al, 5
L_13A30:
        mov ah, byte ptr [ebp + 8]
L_13A33:
        mov byte ptr [g_e384], ah
L_13A39:
        mov dx, 3CEh
L_13A3D:
        out dx, ax
L_13A3F:
        pop edx
L_13A40:
        pop ebp
L_13A41:
        shr eax, 8
L_13A44:
        ret
        ORG $+3 ; original zero fill to the next aligned entry at 13A48h
ASM_TEXT ENDS
        END
