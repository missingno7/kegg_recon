.386
EXTRN g_e2fc:WORD
EXTRN g_7db3:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_11420
a_11420:
        push    eax
        push    edx
        mov     dx,word ptr g_e2fc
        add     dx,0Ch
        in      al,dx
        test    al,80h
        je      short L_11443
        push    ecx
        mov     ecx,3E8h
L_11438:
        in      al,dx
        test    al,80h
        loopne  short L_11438
        stc
        or      ecx,ecx
        pop     ecx
        je      short L_1144A
L_11443:
        mov     al,byte ptr g_7db3
        out     dx,al
        clc
L_1144A:
        pop     edx
        pop     eax
        ret
ASM_TEXT ENDS
        END
