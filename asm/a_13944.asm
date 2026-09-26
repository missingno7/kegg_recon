.386
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13944
a_13944:
        push ebp
L_13945:
        lea ebp, [esp]
L_13948:
        push eax
L_13949:
        push edx
L_1394A:
        mov dx, 3C0h
L_1394E:
        mov al, byte ptr [ebp + 8]
L_13951:
        or al, 20h
L_13953:
        out dx, al
L_13954:
        jmp short L_13956
L_13956:
        jmp short L_13958
L_13958:
        in al, dx
L_13959:
        and al, byte ptr [ebp + 0Ch]
L_1395C:
        or al, byte ptr [ebp + 10h]
L_1395F:
        out dx, al
L_13960:
        pop edx
L_13961:
        pop eax
L_13962:
        pop ebp
L_13963:
        ret
ASM_TEXT ENDS
        END
