.386
EXTRN f_11420:NEAR
EXTRN g_7db3:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_11377
a_11377:
        push    ebp
        lea     ebp,[esp]
        push    eax
        push    ebx
        push    edx
        sub     dx,dx
        mov     ax,0F42h
        mov     ebx,[ebp+8]
        add     bx,7Fh
        shr     bx,8
        or      bx,bx
        jz      short L_113B8
        div     bx
        mov     bx,100h
        sub     bx,ax
        mov     byte ptr g_7db3,40h
        call    f_11420
        jc      short L_113B8
        mov     byte ptr g_7db3,bl
        call    f_11420
L_113B8:
        pop     edx
        pop     ebx
        pop     eax
        pop     ebp
        ret
ASM_TEXT ENDS
        END
