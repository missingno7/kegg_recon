.386
EXTRN f_9e10:NEAR
EXTRN g_73d8:DWORD
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_9f64
a_9f64:
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
ASM_TEXT ENDS
        END
