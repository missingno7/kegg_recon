.386
EXTRN f_11420:NEAR
EXTRN f_1144d:NEAR
EXTRN g_7db3:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_113cf
a_113cf:
        call    f_1144d
        mov     byte ptr g_7db3,0D1h
        call    f_11420
        ret
ASM_TEXT ENDS
        END
