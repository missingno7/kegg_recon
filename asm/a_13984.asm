.386
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13984
a_13984:
        push ebp
L_13985:
        lea ebp, [esp]
L_13988:
        push eax
L_13989:
        push edx
L_1398A:
        mov dx, 3C4h
L_1398E:
        mov al, byte ptr [ebp + 8]
L_13991:
        out dx, al
L_13992:
        jmp short L_13994
L_13994:
        jmp short L_13996
L_13996:
        inc dx
L_13998:
        in al, dx
L_13999:
        and al, byte ptr [ebp + 0Ch]
L_1399C:
        or al, byte ptr [ebp + 10h]
L_1399F:
        out dx, al
L_139A0:
        pop edx
L_139A1:
        pop eax
L_139A2:
        pop ebp
L_139A3:
        ret
ASM_TEXT ENDS
        END
