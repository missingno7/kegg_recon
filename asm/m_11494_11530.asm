.386P
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
        PUBLIC g_7db4
g_7db4	DD 0
        PUBLIC g_7db8
g_7db8	DW 0
        PUBLIC g_7dba
g_7dba	DB 0
        PUBLIC g_7dbb
g_7dbb	DB 0
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC a_11494
        PUBLIC f_11494
f_11494 LABEL NEAR
a_11494 PROC NEAR
        push eax
L_11495:
        mov al, byte ptr [g_7dbb]
L_1149A:
        or al, 4
L_1149C:
        out 0Ah, al
L_1149E:
        pop eax
L_1149F:
        ret
        PUBLIC f_114a0
f_114a0 LABEL NEAR
L_114A0:
        push eax
L_114A1:
        push ecx
L_114A2:
        push edx
L_114A3:
        mov cl, byte ptr [g_7dbb]
L_114A9:
        mov al, cl
L_114AB:
        or al, 4
L_114AD:
        out 0Ah, al
L_114AF:
        out 0Ch, al
L_114B1:
        mov al, cl
L_114B3:
        or al, byte ptr [g_7dba]
L_114B9:
        out 0Bh, al
L_114BB:
        movzx dx, cl
L_114BF:
        add dx, dx
L_114C2:
        mov eax, dword ptr [g_7db4]
L_114C7:
        out dx, al
L_114C8:
        mov al, ah
L_114CA:
        out dx, al
L_114CB:
        inc dx
L_114CD:
        mov ax, word ptr [g_7db8]
L_114D3:
        dec ax
L_114D5:
        out dx, al
L_114D6:
        mov al, ah
L_114D8:
        out dx, al
L_114D9:
        mov edx, 82818387h
L_114DE:
        shl cl, 3
L_114E1:
        shr edx, cl
L_114E3:
        xor dh, dh
L_114E5:
        shr cl, 3
L_114E8:
        shr eax, 10h
L_114EB:
        out dx, al
L_114EC:
        mov al, cl
L_114EE:
        out 0Ah, al
L_114F0:
        pop edx
L_114F1:
        pop ecx
L_114F2:
        pop eax
L_114F3:
        ret
L_114F4:
        push ecx
L_114F5:
        push edx
L_114F6:
        movzx dx, byte ptr [g_7dbb]
L_114FE:
        add dx, dx
L_11501:
        inc dx
L_11503:
        in al, dx
L_11504:
        mov ah, al
L_11506:
        in al, dx
L_11507:
        xchg al, ah
L_11509:
        mov cx, ax
L_1150C:
        in al, dx
L_1150D:
        mov ah, al
L_1150F:
        in al, dx
L_11510:
        xchg al, ah
L_11512:
        sub cx, ax
L_11515:
        cmp cx, 10h
L_11519:
        jg short L_11509
L_1151B:
        cmp cx, -10h
L_1151F:
        jl short L_11509
L_11521:
        neg ax
L_11524:
        add ax, word ptr [g_7db8]
L_1152B:
        dec ax
L_1152D:
        pop edx
L_1152E:
        pop ecx
L_1152F:
        ret
a_11494 ENDP
_TEXT ENDS
        END
