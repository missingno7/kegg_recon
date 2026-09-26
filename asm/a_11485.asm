.386
EXTRN g_e2fc:WORD
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_11485
a_11485:
        push    edx
        mov     dx,word ptr g_e2fc
        add     dx,0Eh
        in      al,dx
        pop     edx
        ret
ASM_TEXT ENDS
        END
