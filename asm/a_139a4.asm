.386
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_139a4
a_139a4:
        push ebp
L_139A5:
        lea ebp, [esp]
L_139A8:
        push eax
L_139A9:
        push edx
L_139AA:
        mov dx, 3CEh
L_139AE:
        mov al, byte ptr [ebp + 8]
L_139B1:
        out dx, al
L_139B2:
        jmp short L_139B4
L_139B4:
        jmp short L_139B6
L_139B6:
        inc dx
L_139B8:
        in al, dx
L_139B9:
        and al, byte ptr [ebp + 0Ch]
L_139BC:
        or al, byte ptr [ebp + 10h]
L_139BF:
        out dx, al
L_139C0:
        pop edx
L_139C1:
        pop eax
L_139C2:
        pop ebp
L_139C3:
        ret
ASM_TEXT ENDS
        END
