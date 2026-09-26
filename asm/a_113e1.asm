.386
EXTRN f_11358:NEAR
EXTRN f_11420:NEAR
EXTRN f_1144d:NEAR
EXTRN g_7db3:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_113e1
a_113e1:
        call    f_11358
        call    f_1144d
        mov     byte ptr g_7db3,0D3h
        call    f_11420
        ret
ASM_TEXT ENDS
        END
