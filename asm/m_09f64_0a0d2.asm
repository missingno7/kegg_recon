.386
EXTRN f_9e10:NEAR
EXTRN f_9e54:NEAR
EXTRN g_73a8:DWORD
EXTRN g_73d8:DWORD
EXTRN g_e1b4_35:DWORD
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC a_9f64
        PUBLIC f_9f64
f_9f64 LABEL NEAR
a_9f64 PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        pushfd
        cli
        in      al,70h
        mov     ah,al
        and     ah,80h
        or      al,80h
        jmp     short L_9F76
L_9F76:
        jmp     short L_9F78
L_9F78:
        jmp     short L_9F7A
L_9F7A:
        out     70h,al
        shl     eax,8
        in      al,21h
        mov     ah,al
        in      al,0A1h
        push    eax
        mov     al,0FFh
        out     21h,al
        out     0A1h,al
        call    f_9e10
        call    f_9e10
        mov     al,34h
        out     43h,al
        jmp     short L_9F9C
L_9F9C:
        jmp     short L_9F9E
L_9F9E:
        jmp     short L_9FA0
L_9FA0:
        mov     al,0
        out     40h,al
        jmp     short L_9FA6
L_9FA6:
        jmp     short L_9FA8
L_9FA8:
        jmp     short L_9FAA
L_9FAA:
        out     40h,al
        push    ecx
        push    eax
        mov     ecx,0FFFFFC18h
L_9FB3:
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        inc     ecx
        jne     short L_9FB3
        pop     eax
        pop     ecx
        call    f_9e10
        out     43h,al
        jmp     short L_9FFD
L_9FFD:
        jmp     short L_9FFF
L_9FFF:
        jmp     short L_A001
L_A001:
        in      al,40h
        jmp     short L_A005
L_A005:
        jmp     short L_A007
L_A007:
        jmp     short L_A009
L_A009:
        mov     ah,al
        in      al,40h
        xchg    ah,al
        movzx   eax,ax
        mov     ebx,10000h
        sub     ebx,eax
        mov     dword ptr g_73d8,ebx
        pop     eax
        out     0A1h,al
        mov     al,ah
        out     21h,al
        shr     eax,8
        in      al,70h
        and     al,7Fh
        or      al,ah
        jmp     short L_A031
L_A031:
        jmp     short L_A033
L_A033:
        jmp     short L_A035
L_A035:
        out     70h,al
        popfd
        popad
        mov     eax,dword ptr g_73d8
        ret
a_9f64 ENDP
        ASSUME CS:_TEXT
        PUBLIC a_a03f
        PUBLIC f_a03f
f_a03f LABEL NEAR
a_a03f PROC NEAR
        push    ebp
        lea     ebp,[esp]
        push    eax
        push    ebx
        pushfd
        cli
        mov     ebx,[ebp+8]
        mov     al,34h
        out     43h,al
        jmp     short L_A050
L_A050:
        jmp     short L_A052
L_A052:
        jmp     short L_A054
L_A054:
        mov     al,bl
        out     40h,al
        jmp     short L_A05A
L_A05A:
        jmp     short L_A05C
L_A05C:
        jmp     short L_A05E
L_A05E:
        mov     al,bh
        out     40h,al
        popfd
        pop     ebx
        pop     eax
        pop     ebp
        ret
a_a03f ENDP
        ASSUME CS:_TEXT
        PUBLIC a_a067
        PUBLIC f_a067
f_a067 LABEL NEAR
a_a067 PROC NEAR
        push    eax
        push    edx
        mov     al,34h
        out     43h,al
        mov     ax,0FFFFh
        jmp     short L_A073
L_A073:
        jmp     short L_A075
L_A075:
        jmp     short L_A077
L_A077:
        out     40h,al
        jmp     short L_A07B
L_A07B:
        jmp     short L_A07D
L_A07D:
        jmp     short L_A07F
L_A07F:
        mov     al,ah
        out     40h,al
        mov     dx,ds
        rol     edx,10h
        mov     dx,es
        push    edx
        cld
        mov     ax,SEG DGROUP
        mov     ds,eax
        mov     es,eax
        call    f_9e10
        mov     al,34h
        out     43h,al
        mov     eax,dword ptr g_73a8
        jmp     short L_A0A6
L_A0A6:
        jmp     short L_A0A8
L_A0A8:
        jmp     short L_A0AA
L_A0AA:
        out     40h,al
        jmp     short L_A0AE
L_A0AE:
        jmp     short L_A0B0
L_A0B0:
        jmp     short L_A0B2
L_A0B2:
        mov     al,ah
        out     40h,al
        pushad
        inc     dword ptr g_e1b4_35
        call    f_9e54
        popad
        mov     al,20h
        out     20h,al
        pop     edx
        mov     es,edx
        rol     edx,10h
        mov     ds,edx
        pop     edx
        pop     eax
        iretd
a_a067 ENDP
_TEXT ENDS
        END
