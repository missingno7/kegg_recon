.386
EXTRN a_113f8:NEAR
EXTRN g_e2fc:WORD
EXTRN g_7db2:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_1144d
a_1144d:
        push    edx
        mov     dx,word ptr g_e2fc
        add     dx,6
        mov     al,1
        out     dx,al
        push    eax
        mov     ax,0FFh
L_11461:
        dec     ax
        jne     short L_11461
        pop     eax
        mov     al,0
        out     dx,al
        pop     edx
        call    a_113f8
        mov     eax,0
        jb      short L_11484
        cmp     byte ptr g_7db2,0AAh
        je      short L_11484
        mov     eax,0FFFFFFFFh
L_11484:
        ret
ASM_TEXT ENDS
        END
