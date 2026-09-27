.386P
EXTRN g_8418:DWORD
EXTRN g_841c:DWORD
EXTRN g_8420:DWORD
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
EXTRN g_e324:WORD
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
_DATA ENDS
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
DGROUP GROUP _DATA
        ASSUME CS:_TEXT
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC a_137a8
        PUBLIC f_137a8
f_137a8 LABEL NEAR
a_137a8 PROC NEAR
        pushad
L_137A9:
        lea ebp, [esp + 1Ch]
L_137AD:
        cli
L_137AE:
        mov dword ptr [g_8418], 386h
L_137B8:
        pushfd
L_137B9:
        mov ebp, esp
L_137BB:
        and sp, 0FFFCh
L_137BF:
        pushfd
L_137C0:
        cli
L_137C1:
        pop eax
L_137C2:
        mov ebx, eax
L_137C4:
        xor eax, 40000h
L_137C9:
        push eax
L_137CA:
        popfd
L_137CB:
        pushfd
L_137CC:
        pop eax
L_137CD:
        xor eax, ebx
L_137CF:
        mov esp, ebp
L_137D1:
        popfd
L_137D2:
        test eax, 40000h
L_137D7:
        je short L_137E3
L_137D9:
        mov dword ptr [g_8418], 486h
L_137E3:
        mov dword ptr [g_841c], 0
L_137ED:
        smsw ax
L_137F0:
        test al, 1
L_137F2:
        je short L_13811
L_137F4:
        mov dword ptr [g_841c], 1
L_137FE:
        pushfd
L_137FF:
        pop eax
L_13800:
        test eax, 20000h
L_13805:
        je short L_13811
L_13807:
        mov dword ptr [g_841c], 2
L_13811:
        pushfd
L_13812:
        pop eax
L_13813:
        shr eax, 0Ch
L_13816:
        and eax, 3
L_13819:
        mov dword ptr [g_8420], eax
L_1381E:
        sti
L_1381F:
        popad
L_13820:
        ret
        ORG $+3 ; original zero fill to the next aligned entry at 13824h
a_137a8 ENDP
        ASSUME CS:_TEXT
        PUBLIC a_13824
        PUBLIC f_13824
f_13824 LABEL NEAR
a_13824 PROC NEAR
        push ebp
L_13825:
        lea ebp, [esp]
L_13828:
        push eax
L_13829:
        push ecx
L_1382A:
        push edi
L_1382B:
        mov edi, dword ptr [ebp + 8]
L_1382E:
        sub eax, eax
L_13830:
        cmp edi, 0B0000h
L_13836:
        jge short L_13874
L_13838:
        cmp edi, 0A0000h
L_1383E:
        jl short L_13874
L_13840:
        cmp byte ptr [g_e385], 0Fh
L_13847:
        je short L_1385A
L_13849:
        mov byte ptr [g_e385], 0Fh
L_13850:
        mov ax, 0F02h
L_13854:
        mov dx, 3C4h
L_13858:
        out dx, ax
L_1385A:
        cmp byte ptr [g_e384], 40h
L_13861:
        je short L_13874
L_13863:
        mov byte ptr [g_e384], 40h
L_1386A:
        mov ax, 4005h
L_1386E:
        mov dx, 3CEh
L_13872:
        out dx, ax
L_13874:
        mov ecx, dword ptr [ebp + 0Ch]
L_13877:
        shr ecx, 2
L_1387A:
        rep stosd
L_1387C:
        mov ecx, dword ptr [ebp + 0Ch]
L_1387F:
        and ecx, 3
L_13882:
        rep stosb
L_13884:
        pop edi
L_13885:
        pop ecx
L_13886:
        pop eax
L_13887:
        pop ebp
L_13888:
        ret
a_13824 ENDP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC a_13889
        PUBLIC f_13889
f_13889 LABEL NEAR
a_13889 PROC NEAR
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
a_13889 ENDP
_TEXT ENDS
END
