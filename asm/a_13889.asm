.386
EXTRN g_e324:WORD
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13889
a_13889:
        push ebp
L_1388A:
        lea ebp, [esp]
L_1388D:
        push ecx
L_1388E:
        push edx
L_1388F:
        push esi
L_13890:
        push edi
L_13891:
        mov esi, dword ptr [ebp + 8]
L_13894:
        mov edi, dword ptr [ebp + 0Ch]
L_13897:
        mov ecx, dword ptr [ebp + 10h]
L_1389A:
        cmp esi, edi
L_1389C:
        jge short L_138A5
L_1389E:
        std
L_1389F:
        add esi, ecx
L_138A1:
        add edi, ecx
L_138A3:
        dec esi
L_138A4:
        dec edi
L_138A5:
        cmp esi, 0B0000h
L_138AB:
        jge short L_138EB
L_138AD:
        cmp esi, 0A0000h
L_138B3:
        jl short L_138EB
L_138B5:
        cmp edi, 0B0000h
L_138BB:
        jge short L_138FB
L_138BD:
        cmp edi, 0A0000h
L_138C3:
        jl short L_138FB
L_138C5:
        cmp word ptr [g_e324], 1
L_138CD:
        jne short L_138E9
L_138CF:
        cmp byte ptr [g_e384], 41h
L_138D6:
        je short L_138E9
L_138D8:
        mov byte ptr [g_e384], 41h
L_138DF:
        mov ax, 4105h
L_138E3:
        mov dx, 3CEh
L_138E7:
        out dx, ax
L_138E9:
        jmp short L_1393A
L_138EB:
        cmp edi, 0B0000h
L_138F1:
        jge short L_1392F
L_138F3:
        cmp edi, 0A0000h
L_138F9:
        jl short L_1392F
L_138FB:
        cmp byte ptr [g_e385], 0Fh
L_13902:
        je short L_13915
L_13904:
        mov byte ptr [g_e385], 0Fh
L_1390B:
        mov ax, 0F02h
L_1390F:
        mov dx, 3C4h
L_13913:
        out dx, ax
L_13915:
        cmp byte ptr [g_e384], 40h
L_1391C:
        je short L_1392F
L_1391E:
        mov byte ptr [g_e384], 40h
L_13925:
        mov ax, 4005h
L_13929:
        mov dx, 3CEh
L_1392D:
        out dx, ax
L_1392F:
        shr ecx, 2
L_13932:
        rep movsd dword ptr es:[edi], dword ptr [esi]
L_13934:
        mov ecx, dword ptr [ebp + 10h]
L_13937:
        and ecx, 3
L_1393A:
        rep movsb byte ptr es:[edi], byte ptr [esi]
L_1393C:
        cld
L_1393D:
        pop edi
L_1393E:
        pop esi
L_1393F:
        pop edx
L_13940:
        pop ecx
L_13941:
        pop ebp
L_13942:
        ret
        ORG $+1 ; original zero fill to the next even code address
ASM_TEXT ENDS
        END
