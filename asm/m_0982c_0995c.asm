.386
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
        PUBLIC g_739c
        PUBLIC g_739e
        PUBLIC g_73a2
g_739c  DW 0
g_739e  DD 0
g_73a2  DW 0
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ASSUME CS:_TEXT
        PUBLIC a_982c
        PUBLIC f_982c
f_982c LABEL NEAR
a_982c PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        mov     esi,[ebp+8]
        mov     edi,esi
        mov     ecx,[ebp+0Ch]
        mov     word ptr g_739c,0
        mov     dword ptr g_739e,0
        mov     word ptr g_73a2,0FFFFh
        cmp     dword ptr [esi+ecx-4],30444F43h
        je      L_98FD
        cmp     dword ptr [esi+ecx-4],31444F43h
        je      L_98FD
        cmp     dword ptr [esi+ecx-4],32444F43h
        je      short L_9898
        popad
        mov     eax,0FFFFFFFFh
        ret
L_9882:
        popad
        mov     eax,dword ptr g_739e
        cmp     word ptr g_739c,0
        je      short L_9897
        mov     eax,0FFFFFFFFh
L_9897:
        ret
L_9898:
        mov     eax,8
        mov     word ptr g_73a2,2
        mov     dword ptr g_739e,eax
        sub     ecx,eax
        lea     ebx,[esi+ecx]
        mov     ax,word ptr [ebx]
        mov     word ptr g_739c,ax
        mov     ax,word ptr [ebx+2]
        ror     ax,7
        shl     eax,10h
        mov     ax,word ptr [ebx+2]
        rol     ax,3
        mov     ebx,eax
        cmp     ecx,400h
        jle     short L_98DB
        mov     ecx,400h
L_98DB:
        shr     ecx,2
        sub     ebp,ebp
L_98E0:
        lodsd
        xor     eax,ebx
        stosd
        xor     ebp,eax
        rol     ebx,1
        loop    short L_98E0
        xor     word ptr g_739c,bp
        shr     ebp,10h
        xor     word ptr g_739c,bp
        jmp     short L_9882
L_98FD:
        mov     eax,0Ah
        mov     word ptr g_73a2,1
        mov     dword ptr g_739e,eax
        sub     ecx,eax
        lea     ebx,[esi+ecx]
        mov     ax,word ptr [ebx]
        mov     word ptr g_739c,ax
        mov     ebx,dword ptr [ebx+2]
        ror     ebx,7
        shr     ecx,2
        mov     ebp,1234h
L_992C:
        lodsd
        xor     eax,ebx
        stosd
        xor     ebp,eax
        rol     ebx,1
        loop    short L_992C
        xor     word ptr g_739c,bp
        shr     ebp,10h
        xor     word ptr g_739c,bp
        jmp     near ptr L_9882
L_994C:
        mov     dword ptr g_739e,0
        mov     eax,0
        ret
a_982c ENDP
_TEXT ENDS
        END
