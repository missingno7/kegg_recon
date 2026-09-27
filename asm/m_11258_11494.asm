.386
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN g_742c:WORD
EXTRN g_744c:DWORD
EXTRN g_7458:DWORD
EXTRN g_745c:DWORD
EXTRN g_7486:BYTE
EXTRN g_7487:BYTE
EXTRN g_7dba:BYTE
EXTRN g_7dbb:BYTE
EXTRN g_e2fc:WORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
EXTRN f_11494:NEAR
EXTRN f_114a0:NEAR
EXTRN f_c3fb:NEAR
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC f_11258
f_11258 LABEL NEAR
a_11258 PROC NEAR
        push eax
L_11259:
        push ecx
L_1125A:
        push edx
L_1125B:
        mov dx, ds
L_1125E:
        rol edx, 10h
L_11261:
        mov ax, SEG DGROUP
L_11265:
        mov ds, eax
L_11267:
        mov dx, word ptr [g_e2fc]
L_1126E:
        add dx, 0Eh
L_11272:
        in al, dx
L_11273:
        cmp dword ptr [g_7458], 0
L_1127A:
        je short L_112CF
L_1127C:
        mov eax, dword ptr [g_744c]
L_11281:
        cmp dword ptr [g_745c], eax
L_11287:
        jne short L_112BF
L_11289:
        add dx, -2
L_1128D:
        mov ecx, 3E8h
L_11292:
        in al, dx
L_11293:
        test al, 80h
L_11295:
        loopne L_11292
L_11297:
        mov al, 14h
L_11299:
        out dx, al
L_1129A:
        mov ecx, 3E8h
L_1129F:
        in al, dx
L_112A0:
        test al, 80h
L_112A2:
        loopne L_1129F
L_112A4:
        mov al, 7Fh
L_112A6:
        out dx, al
L_112A7:
        mov ecx, 3E8h
L_112AC:
        in al, dx
L_112AD:
        test al, 80h
L_112AF:
        loopne L_112AC
L_112B1:
        mov al, 2
L_112B3:
        out dx, al
L_112B4:
        mov word ptr [g_742c], 0FFFFh
L_112BD:
        jmp short L_112D8
L_112BF:
        mov dword ptr [g_745c], eax
L_112C4:
        push eax
L_112C5:
        call f_11377
L_112CA:
        add esp, 4
L_112CD:
        jmp short L_11289
L_112CF:
        mov word ptr [g_742c], 0
L_112D8:
        mov dx, es
L_112DB:
        cld
L_112DC:
        mov ax, SEG DGROUP
L_112E0:
        mov es, eax
L_112E2:
        pushad
L_112E3:
        sti
L_112E4:
        call f_113bd
L_112E9:
        call f_c3fb
L_112EE:
        popad
L_112EF:
        mov es, edx
L_112F1:
        rol edx, 10h
L_112F4:
        mov ds, edx
L_112F6:
        pop edx
L_112F7:
        pop ecx
L_112F8:
        pop eax
L_112F9:
        iretd
a_11258 ENDP

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
        PUBLIC a_113cf
        PUBLIC f_113cf
f_113cf LABEL NEAR
a_113cf PROC NEAR
        call    f_1144d
        mov     byte ptr g_7db3,0D1h
        call    f_11420
        ret
a_113cf ENDP
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
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
        PUBLIC g_7db0
g_7db0	DW 0
        PUBLIC g_7db2
g_7db2	DB 0
        PUBLIC g_7db3
g_7db3	DB 0
_DATA ENDS
        END
