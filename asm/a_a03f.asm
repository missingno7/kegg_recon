.386
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_a03f
a_a03f:
        push    ebp
        lea     ebp,[esp]
        push    eax
        push    ebx
        pushfd
        cli
        mov     ebx,[ebp+8]
        mov     al,34h
        out     43h,al
        jmp     short L_A050
L_A050:
        jmp     short L_A052
L_A052:
        jmp     short L_A054
L_A054:
        mov     al,bl
        out     40h,al
        jmp     short L_A05A
L_A05A:
        jmp     short L_A05C
L_A05C:
        jmp     short L_A05E
L_A05E:
        mov     al,bh
        out     40h,al
        popfd
        pop     ebx
        pop     eax
        pop     ebp
        ret
ASM_TEXT ENDS
        END
