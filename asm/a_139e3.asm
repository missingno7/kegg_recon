.386
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_139e3
a_139e3:
        push ebp
L_139E4:
        lea ebp, [esp]
L_139E7:
        push edx
L_139E8:
        mov ax, 1102h
L_139EC:
        mov edx, ecx
L_139EE:
        mov cl, byte ptr [ebp + 8]
L_139F1:
        rol ah, cl
L_139F3:
        mov ecx, edx
L_139F5:
        and ah, 0Fh
L_139F8:
        mov byte ptr [g_e385], ah
L_139FE:
        mov dx, 3C4h
L_13A02:
        out dx, ax
L_13A04:
        pop edx
L_13A05:
        pop ebp
L_13A06:
        shr eax, 8
L_13A09:
        ret
ASM_TEXT ENDS
        END
