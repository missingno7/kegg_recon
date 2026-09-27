.386
EXTRN f_11494:NEAR
EXTRN f_114a0:NEAR
EXTRN g_7486:BYTE
EXTRN g_7487:BYTE
EXTRN g_7db0:WORD
EXTRN g_7db2:BYTE
EXTRN g_7db3:BYTE
EXTRN g_7dba:BYTE
EXTRN g_7dbb:BYTE
EXTRN g_e2fc:WORD
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC a_112fa
        PUBLIC f_112fa
f_112fa LABEL NEAR
a_112fa PROC NEAR
        push eax
L_112FB:
        mov byte ptr [g_7db3], 14h
L_11302:
        call f_11420
L_11307:
        mov ax, word ptr [g_7db0]
L_1130D:
        dec ax
L_1130F:
        mov byte ptr [g_7db3], al
L_11314:
        call f_11420
L_11319:
        mov byte ptr [g_7db3], ah
L_1131F:
        call f_11420
L_11324:
        pop eax
L_11325:
        ret
L_11326:
        push eax
L_11327:
        mov byte ptr [g_7dba], 48h
L_1132E:
        mov al, byte ptr [g_7487]
L_11333:
        mov byte ptr [g_7dbb], al
L_11338:
        call f_114a0
L_1133D:
        pop eax
L_1133E:
        ret
        PUBLIC f_1133f
f_1133f LABEL NEAR
L_1133F:
        push eax
L_11340:
        mov byte ptr [g_7dba], 58h
L_11347:
        mov al, byte ptr [g_7487]
L_1134C:
        mov byte ptr [g_7dbb], al
L_11351:
        call f_114a0
L_11356:
        pop eax
L_11357:
        ret
        PUBLIC f_11358
f_11358 LABEL NEAR
L_11358:
        mov byte ptr [g_7db3], 0D0h
L_1135F:
        call f_11420
L_11364:
        ret
        PUBLIC f_11365
f_11365 LABEL NEAR
L_11365:
        push eax
L_11366:
        mov al, byte ptr [g_7487]
L_1136B:
        mov byte ptr [g_7dbb], al
L_11370:
        call f_11494
L_11375:
        pop eax
L_11376:
        ret
a_112fa ENDP
        ASSUME CS:_TEXT
        PUBLIC a_11377
        PUBLIC f_11377
f_11377 LABEL NEAR
a_11377 PROC NEAR
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
a_11377 ENDP
        ASSUME CS:_TEXT
        PUBLIC a_113bd
        PUBLIC f_113bd
f_113bd LABEL NEAR
a_113bd PROC NEAR
        push    eax
        mov     al,20h
        cmp     byte ptr g_7486,8
        jl      short L_113CB
        out     0A0h,al
L_113CB:
        out     20h,al
        pop     eax
        ret
a_113bd ENDP
        ASSUME CS:_TEXT
        PUBLIC a_113cf
        PUBLIC f_113cf
f_113cf LABEL NEAR
a_113cf PROC NEAR
        call    f_1144d
        mov     byte ptr g_7db3,0D1h
        call    f_11420
        ret
a_113cf ENDP
        ASSUME CS:_TEXT
        PUBLIC a_113e1
        PUBLIC f_113e1
f_113e1 LABEL NEAR
a_113e1 PROC NEAR
        call    f_11358
        call    f_1144d
        mov     byte ptr g_7db3,0D3h
        call    f_11420
        ret
a_113e1 ENDP
        ASSUME CS:_TEXT
        PUBLIC a_113f8
        PUBLIC f_113f8
f_113f8 LABEL NEAR
a_113f8 PROC NEAR
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
a_113f8 ENDP
        ASSUME CS:_TEXT
        PUBLIC a_11420
        PUBLIC f_11420
f_11420 LABEL NEAR
a_11420 PROC NEAR
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
a_11420 ENDP
        ASSUME CS:_TEXT
        PUBLIC a_1144d
        PUBLIC f_1144d
f_1144d LABEL NEAR
a_1144d PROC NEAR
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
a_1144d ENDP
        ASSUME CS:_TEXT
        PUBLIC a_11485
        PUBLIC f_11485
f_11485 LABEL NEAR
a_11485 PROC NEAR
        push    edx
        mov     dx,word ptr g_e2fc
        add     dx,0Eh
        in      al,dx
        pop     edx
        ret
a_11485 ENDP
_TEXT ENDS
        END
