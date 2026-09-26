.386
EXTRN g_e2fc:WORD
EXTRN g_7db2:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_113f8
a_113f8:
        push    ecx
        push    edx
        mov     dx,word ptr g_e2fc
        add     dx,0Eh
        mov     ecx,3E8h
L_1140A:
        in      al,dx
        test    al,80h
        loope   short L_1140A
        stc
        jecxz   short L_1141D
        add     dx,-4
        in      al,dx
        mov     byte ptr g_7db2,al
        clc
L_1141D:
        pop     edx
        pop     ecx
        ret
ASM_TEXT ENDS
        END
