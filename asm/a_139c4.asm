.386
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_139c4
a_139c4:
        push ebp
L_139C5:
        lea ebp, [esp]
L_139C8:
        push edx
L_139C9:
        mov al, 2
L_139CB:
        mov ah, byte ptr [ebp + 8]
L_139CE:
        and ah, 0Fh
L_139D1:
        mov byte ptr [g_e385], ah
L_139D7:
        mov dx, 3C4h
L_139DB:
        out dx, ax
L_139DD:
        pop edx
L_139DE:
        pop ebp
L_139DF:
        shr eax, 8
L_139E2:
        ret
ASM_TEXT ENDS
        END
