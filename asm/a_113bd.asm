.386
EXTRN g_7486:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_113bd
a_113bd:
        push    eax
        mov     al,20h
        cmp     byte ptr g_7486,8
        jl      short L_113CB
        out     0A0h,al
L_113CB:
        out     20h,al
        pop     eax
        ret
ASM_TEXT ENDS
        END
