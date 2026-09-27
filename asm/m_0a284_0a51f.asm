.386
DGROUP GROUP _DATA
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN g_e2c4:DWORD
EXTRN g_e2c8:DWORD
EXTRN g_e2cc:DWORD
EXTRN g_e2d0:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC a_a284
        PUBLIC f_a284
f_a284 LABEL NEAR
a_a284 PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        mov     ebx,[ebp+0Ch]
        mov     esi,[ebp+8]
        mov     dword ptr g_73e0,esi
        mov     dword ptr g_73e4,ebx
        cmp     dword ptr [esi],4D524F46h
        je      short L_A2B4
        mov     al,1
L_A2A5:
        mov     ah,8
        movzx   eax,ax
        mov     dword ptr g_73dc,eax
        jmp     near ptr L_A47E
L_A2B4:
        mov     edx,424F4459h
        call    a_a4e1
        mov     al,2
        jb      short L_A2A5
        mov     eax,[ebx]
        xchg    al,ah
        rol     eax,10h
        xchg    al,ah
        lea     esi,[ebx+4]
        mov     edi,dword ptr g_73e4
        mov     edx,424D4844h
        call    a_a4e1
        mov     al,3
        jb      short L_A2A5
        movzx   eax,word ptr [ebx+4]
        movzx   ecx,word ptr [ebx+6]
        xchg    al,ah
        xchg    cl,ch
        mov     dword ptr g_e2c4,eax
        mov     dword ptr g_e2c8,ecx
        mul     ecx
        mov     dword ptr g_73e8,eax
        lea     ecx,[edi+eax]
        mov     dword ptr g_73f0,ecx
        cmp     byte ptr [ebx+0Eh],0
        je      short L_A34B
        sub     ecx,ecx
L_A311:
        mov     cl,byte ptr [esi]
        inc     esi
        cmp     cl,80h
        jb      short L_A336
        ja      short L_A31D
        jmp     short L_A311
L_A31D:
        neg     cl
        inc     cl
        lodsb
        rep     stosb
        cmp     edi,dword ptr g_73f0
        jl      short L_A311
        mov     al,5
        jne     near ptr L_A2A5
        jmp     short L_A356
L_A336:
        inc     ecx
        rep     movsb
        cmp     edi,dword ptr g_73f0
        jl      short L_A311
        mov     al,5
        jne     near ptr L_A2A5
        jmp     short L_A356
L_A34B:
        mov     ecx,dword ptr g_73e8
        shr     ecx,2
        rep     movsd
L_A356:
        mov     edx,434D4150h
        call    a_a4e1
        mov     al,4
        jb      near ptr L_A2A5
        mov     esi,ebx
        lodsd
        mov     ecx,eax
        xchg    cl,ch
        rol     ecx,10h
        xchg    cl,ch
        mov     dword ptr g_73ec,ecx
L_A37A:
        lodsb
        shr     al,2
        stosb
        loop    short L_A37A
        sub     edi,dword ptr g_73e4
        mov     esi,dword ptr g_73e0
        cmp     dword ptr [esi+8],4D424C49h
        jne     near ptr L_A47E
        mov     esi,dword ptr g_73e4
        mov     edi,dword ptr g_73e0
        mov     ecx,dword ptr g_e2c8
L_A3AC:
        push    ecx
        mov     ecx,dword ptr g_e2c4
        shr     ecx,4
L_A3B6:
        push    ecx
        mov     ecx,2
L_A3BC:
        push    ecx
        mov     ecx,8
L_A3C2:
        sub     al,al
        dec     ecx
        mov     edx,dword ptr g_e2c4
        add     esi,edx
        shr     edx,3
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        inc     ecx
        stosb
        dec     cx
        je      short L_A428
        jmp     short L_A3C2
L_A428:
        inc     esi
        pop     ecx
        dec     cx
        je      short L_A430
        jmp     short L_A3BC
L_A430:
        pop     ecx
        dec     cx
        je      short L_A43A
        jmp     near ptr L_A3B6
L_A43A:
        mov     edx,dword ptr g_e2c4
        mov     ecx,edx
        shr     ecx,3
        sub     edx,ecx
        add     esi,edx
        pop     ecx
        dec     cx
        je      short L_A453
        jmp     near ptr L_A3AC
L_A453:
        mov     eax,dword ptr g_e2c4
        mul     dword ptr g_e2c8
        mov     ecx,eax
        mov     edi,dword ptr g_73e4
        mov     esi,dword ptr g_73e0
        std
        add     edi,ecx
        add     esi,ecx
        dec     esi
        dec     edi
        rep     movsb
        cld
        mov     edi,eax
        add     edi,dword ptr g_73ec
L_A47E:
        mov     dword ptr g_e2cc,edi
        mov     edi,dword ptr g_73e8
        mov     dword ptr g_e2d0,edi
        mov     ebx,[ebp+10h]
        mov     eax,[ebp+0Ch]
        mov     [ebx],eax
        mov     eax,dword ptr g_e2c4
        mul     dword ptr g_e2c8
        mov     [ebx+20h],eax
        add     eax,[ebp+0Ch]
        mov     [ebx+4],eax
        mov     eax,dword ptr g_e2c4
        mov     [ebx+8],eax
        mov     eax,dword ptr g_e2c8
        mov     [ebx+0Ch],eax
        mov     dword ptr [ebx+14h],100h
        mov     dword ptr [ebx+1Ch],300h
        mov     eax,[ebx+1Ch]
        add     eax,[ebx+20h]
        mov     [ebx+18h],eax
        mov     dword ptr [ebx+10h],1
        popad
        mov     eax,dword ptr g_73dc
        ret
a_a284 ENDP
        PUBLIC a_a4e1
        PUBLIC f_a4e1
f_a4e1 LABEL NEAR
a_a4e1 PROC NEAR
        push    esi
        push    edi
        xchg    dl,dh
        rol     edx,10h
        xchg    dl,dh
        mov     edi,dword ptr g_73e0
        mov     ecx,[edi+4]
        xchg    cl,ch
        rol     ecx,10h
        xchg    cl,ch
        lea     esi,[edi+0Ch]
        add     ecx,esi
L_A4FF:
        lodsd
        cmp     eax,edx
        je      short L_A519
        lodsd
        xchg    al,ah
        rol     eax,10h
        xchg    al,ah
        add     esi,eax
        inc     esi
        and     esi,-2
        cmp     esi,ecx
        jl      short L_A4FF
        stc
        jmp     short L_A51C
L_A519:
        clc
        mov     ebx,esi
L_A51C:
        pop     edi
        pop     esi
        ret
a_a4e1 ENDP
_TEXT ENDS
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
        PUBLIC g_73dc
g_73dc	DD 0
        PUBLIC g_73e0
g_73e0	DD 0
        PUBLIC g_73e4
g_73e4	DD 0
        PUBLIC g_73e8
g_73e8	DD 0
        PUBLIC g_73ec
g_73ec	DD 0
        PUBLIC g_73f0
g_73f0	DD 5 DUP (0)
_DATA ENDS
        END
