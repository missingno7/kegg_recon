.386
EXTRN f_9e10:NEAR
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
_DATA ENDS
DGROUP GROUP _DATA
EXTRN f_9e54:NEAR
EXTRN g_73a8:DWORD
EXTRN g_e1b4:DWORD
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_a067
a_a067:
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
        inc     dword ptr g_e1b4
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
ASM_TEXT ENDS
        END
