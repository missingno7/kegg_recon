.386
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13964
a_13964:
        push ebp
L_13965:
        lea ebp, [esp]
L_13968:
        push eax
L_13969:
        push edx
L_1396A:
        mov dx, 3D4h
L_1396E:
        mov al, byte ptr [ebp + 8]
L_13971:
        out dx, al
L_13972:
        jmp short L_13974
L_13974:
        jmp short L_13976
L_13976:
        inc dx
L_13978:
        in al, dx
L_13979:
        and al, byte ptr [ebp + 0Ch]
L_1397C:
        or al, byte ptr [ebp + 10h]
L_1397F:
        out dx, al
L_13980:
        pop edx
L_13981:
        pop eax
L_13982:
        pop ebp
L_13983:
        ret
ASM_TEXT ENDS
        END
