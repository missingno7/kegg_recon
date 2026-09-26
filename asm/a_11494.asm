.386P
REFDATA SEGMENT PARA PUBLIC USE32 'DATA'
EXTRN g_7db4:DWORD
EXTRN g_7db8:WORD
EXTRN g_7dba:BYTE
EXTRN g_7dbb:BYTE
EXTRN g_7dbc:WORD
EXTRN g_7dbe:BYTE
EXTRN g_7dbf:BYTE
EXTRN g_7dc0:WORD
EXTRN g_7dc2:DWORD
EXTRN g_7dc6:BYTE
EXTRN g_7dc8:WORD
EXTRN g_7dca:DWORD
EXTRN g_7dce:DWORD
EXTRN g_7dd2:WORD
EXTRN g_7dd4:WORD
EXTRN g_7dd6:WORD
EXTRN g_7dd8:WORD
EXTRN g_7dda:WORD
EXTRN g_7ddc:DWORD
EXTRN g_7de0:WORD
EXTRN g_7de2:BYTE
EXTRN g_7e92:DWORD
EXTRN g_7eb2:BYTE
EXTRN g_7eb3:BYTE
EXTRN g_7eb4:BYTE
EXTRN g_7eb5:BYTE
EXTRN g_7f35:DWORD
EXTRN g_7f39:BYTE
EXTRN g_7f3a:BYTE
EXTRN g_7f3b:BYTE
EXTRN g_7f3c:BYTE
EXTRN g_7f6c:DWORD
EXTRN g_816c:DWORD
EXTRN g_81ec:DWORD
EXTRN g_826c:DWORD
EXTRN g_82ec:BYTE
EXTRN g_830c:DWORD
EXTRN g_8310:BYTE
EXTRN f_11b04:DWORD
EXTRN f_11b6e:DWORD
EXTRN f_11b74:DWORD
EXTRN f_11d7e:DWORD
REFDATA ENDS
DGROUP GROUP REFDATA
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT, DS:DGROUP
        PUBLIC a_11494
a_11494:
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
L_11530:
        enter 0, 0
L_11534:
        pushad
L_11535:
        mov edi, dword ptr [ebp + 8]
L_11538:
        mov ax, word ptr [ebp + 0Ch]
L_1153C:
        mov dx, word ptr [ebp + 10h]
L_11540:
        mov cl, byte ptr [ebp + 14h]
L_11543:
        mov ch, byte ptr [ebp + 18h]
L_11546:
        mov word ptr [g_7dbc], dx
L_1154D:
        mov byte ptr [g_7dbe], cl
L_11553:
        mov byte ptr [g_7dbf], ch
L_11559:
        mov word ptr [g_7dc0], ax
L_1155F:
        mov eax, 372C00h
L_11564:
        xor edx, edx
L_11566:
        shld edx, eax, 10h
L_1156A:
        shl eax, 10h
L_1156D:
        movzx ebx, word ptr [g_7dc0]
L_11574:
        div ebx
L_11576:
        mov dword ptr [g_7ddc], eax
L_1157B:
        mov dword ptr [g_830c], 602h
L_11585:
        call L_115F1
L_1158A:
        jb short L_115D2
L_1158C:
        mov dword ptr [g_830c], 603h
L_11596:
        call L_118B6
L_1159B:
        jb short L_115D2
L_1159D:
        mov dl, byte ptr [g_7f3b]
L_115A3:
        call L_119D7
L_115A8:
        call L_11C36
L_115AD:
        call L_11D12
L_115B2:
        mov dword ptr [g_830c], 0
L_115BC:
        call L_11BCE
L_115C1:
        jae short L_115D2
L_115C3:
        mov dword ptr [g_830c], 604h
L_115CD:
        call L_115DA
L_115D2:
        popad
L_115D3:
        mov eax, dword ptr [g_830c]
L_115D8:
        leave
L_115D9:
        ret
L_115DA:
        pushad
L_115DB:
        call L_11C1B
L_115E0:
        call L_11CBE
L_115E5:
        call L_11C2A
L_115EA:
        call L_1197E
L_115EF:
        popad
L_115F0:
        ret
L_115F1:
        pushad
L_115F2:
        mov word ptr [g_7de0], 4
L_115FB:
        cmp dword ptr [edi + 438h], 2E4B2E4Dh
L_11605:
        je short L_1162E
L_11607:
        cmp dword ptr [edi + 438h], 34544C46h
L_11611:
        je short L_1162E
L_11613:
        mov word ptr [g_7de0], 8
L_1161C:
        cmp dword ptr [edi + 438h], 4E484338h
L_11626:
        je short L_1162E
L_11628:
        stc
L_11629:
        jmp near ptr L_116FE
L_1162E:
        mov byte ptr [g_7eb2], 0FFh
L_11635:
        mov byte ptr [g_7eb4], 40h
L_1163C:
        mov byte ptr [g_7f39], 6
L_11643:
        mov byte ptr [g_7f3a], 0
L_1164A:
        mov byte ptr [g_7f3b], 7Dh
L_11651:
        mov al, byte ptr [edi + 3B6h]
L_11657:
        mov byte ptr [g_7eb3], al
L_1165C:
        mov ecx, 80h
L_11661:
        xor ebx, ebx
L_11663:
        xor ah, ah
L_11665:
        mov al, byte ptr [edi + ebx + 3B8h]
L_1166C:
        mov byte ptr [ebx + g_7eb5], al
L_11672:
        cmp al, ah
L_11674:
        jb short L_11678
L_11676:
        mov ah, al
L_11678:
        inc ebx
L_11679:
        loop L_11665
L_1167B:
        movzx ecx, ah
L_1167E:
        inc ecx
L_1167F:
        xor ebx, ebx
L_11681:
        movzx eax, word ptr [g_7de0]
L_11688:
        shl eax, 8
L_1168B:
        mov esi, edi
L_1168D:
        add esi, 43Ch
L_11693:
        mov dword ptr [ebx*4 + g_7f6c], esi
L_1169A:
        add esi, eax
L_1169C:
        inc ebx
L_1169D:
        loop L_11693
L_1169F:
        mov ecx, 1Fh
L_116A4:
        lea edi, [edi + 14h]
L_116A7:
        xor ebx, ebx
L_116A9:
        inc ebx
L_116AA:
        mov al, byte ptr [edi + 19h]
L_116AD:
        mov byte ptr [ebx + g_82ec], al
L_116B3:
        movzx eax, word ptr [edi + 16h]
L_116B7:
        movzx edx, word ptr [edi + 1Ch]
L_116BB:
        movzx ebp, word ptr [edi + 1Ah]
L_116BF:
        xchg al, ah
L_116C1:
        xchg dl, dh
L_116C3:
        xchg ax, bp
L_116C5:
        xchg al, ah
L_116C7:
        xchg ax, bp
L_116C9:
        add eax, eax
L_116CB:
        add edx, edx
L_116CD:
        add ebp, ebp
L_116CF:
        cmp edx, 2
L_116D2:
        ja short L_116D8
L_116D4:
        xor edx, edx
L_116D6:
        mov ebp, eax
L_116D8:
        add edx, ebp
L_116DA:
        add eax, esi
L_116DC:
        add edx, esi
L_116DE:
        add ebp, esi
L_116E0:
        mov dword ptr [ebx*4 + g_816c], esi
L_116E7:
        mov dword ptr [ebx*4 + g_826c], edx
L_116EE:
        mov dword ptr [ebx*4 + g_81ec], ebp
L_116F5:
        mov esi, eax
L_116F7:
        add edi, 1Eh
L_116FA:
        inc ebx
L_116FB:
        loop L_116AA
L_116FD:
        clc
L_116FE:
        popad
L_116FF:
        ret
L_11700:
        pushad
L_11701:
        dec byte ptr [g_7f3a]
L_11707:
        jle short L_11724
L_11709:
        mov esi, OFFSET g_7f3c
L_1170E:
        xor ebx, ebx
L_11710:
        call L_11822
L_11715:
        add esi, 6
L_11718:
        inc ebx
L_11719:
        cmp bx, word ptr [g_7de0]
L_11720:
        jb short L_11710
L_11722:
        popad
L_11723:
        ret
L_11724:
        mov al, byte ptr [g_7f39]
L_11729:
        mov byte ptr [g_7f3a], al
L_1172E:
        inc byte ptr [g_7eb4]
L_11734:
        cmp byte ptr [g_7eb4], 40h
L_1173B:
        jb short L_11770
L_1173D:
        xor ebx, ebx
L_1173F:
        mov byte ptr [g_7eb4], bl
L_11745:
        mov bl, byte ptr [g_7eb2]
L_1174B:
        inc bl
L_1174D:
        cmp bl, byte ptr [g_7eb3]
L_11753:
        jb short L_11757
L_11755:
        xor bl, bl
L_11757:
        mov byte ptr [g_7eb2], bl
L_1175D:
        mov bl, byte ptr [ebx + g_7eb5]
L_11763:
        mov edi, dword ptr [ebx*4 + g_7f6c]
L_1176A:
        mov dword ptr [g_7f35], edi
L_11770:
        mov edi, dword ptr [g_7f35]
L_11776:
        mov esi, OFFSET g_7f3c
L_1177B:
        xor ebx, ebx
L_1177D:
        call L_1179A
L_11782:
        add esi, 6
L_11785:
        add edi, 4
L_11788:
        inc ebx
L_11789:
        cmp bx, word ptr [g_7de0]
L_11790:
        jb short L_1177D
L_11792:
        mov dword ptr [g_7f35], edi
L_11798:
        popad
L_11799:
        ret
L_1179A:
        mov al, byte ptr [edi + 2]
L_1179D:
        shr al, 4
L_117A0:
        mov ah, byte ptr [edi]
L_117A2:
        and ah, 0F0h
L_117A5:
        or al, ah
L_117A7:
        test al, al
L_117A9:
        je short L_117BF
L_117AB:
        mov byte ptr [esi + 2], al
L_117AE:
        movzx eax, al
L_117B1:
        mov al, byte ptr [eax + g_82ec]
L_117B7:
        mov byte ptr [esi + 3], al
L_117BA:
        call L_119CA
L_117BF:
        mov ax, word ptr [edi]
L_117C2:
        xchg al, ah
L_117C4:
        and ax, 0FFFh
L_117C8:
        test ax, ax
L_117CB:
        je short L_117FA
L_117CD:
        mov word ptr [esi], ax
L_117D0:
        movzx ecx, ax
L_117D3:
        call L_119AE
L_117D8:
        push esi
L_117D9:
        push edi
L_117DA:
        movzx eax, byte ptr [esi + 2]
L_117DE:
        mov edx, dword ptr [eax*4 + g_816c]
L_117E5:
        mov esi, dword ptr [eax*4 + g_81ec]
L_117EC:
        mov edi, dword ptr [eax*4 + g_826c]
L_117F3:
        call L_1199C
L_117F8:
        pop edi
L_117F9:
        pop esi
L_117FA:
        mov ax, word ptr [edi + 2]
L_117FE:
        xchg al, ah
L_11800:
        and ax, 0FFFh
L_11804:
        mov word ptr [esi + 4], ax
L_11808:
        cmp ah, 0Ch
L_1180B:
        je short L_1182C
L_1180D:
        cmp ah, 0Fh
L_11810:
        je short L_11832
L_11812:
        cmp ah, 0Bh
L_11815:
        je short L_11852
L_11817:
        cmp ah, 0Dh
L_1181A:
        je short L_11861
L_1181C:
        cmp ah, 9
L_1181F:
        je short L_1188F
L_11821:
        ret
L_11822:
        mov ax, word ptr [esi + 4]
L_11826:
        cmp ah, 0Ah
L_11829:
        je short L_11869
L_1182B:
        ret
L_1182C:
        call L_119CA
L_11831:
        ret
L_11832:
        test al, al
L_11834:
        je short L_11831
L_11836:
        cmp al, 20h
L_11838:
        jae short L_11845
L_1183A:
        mov byte ptr [g_7f39], al
L_1183F:
        mov byte ptr [g_7f3a], al
L_11844:
        ret
L_11845:
        mov byte ptr [g_7f3b], al
L_1184A:
        mov dl, al
L_1184C:
        call L_119D7
L_11851:
        ret
L_11852:
        dec al
L_11854:
        mov byte ptr [g_7eb2], al
L_11859:
        mov byte ptr [g_7eb4], 40h
L_11860:
        ret
L_11861:
        mov byte ptr [g_7eb4], 40h
L_11868:
        ret
L_11869:
        mov ah, al
L_1186B:
        mov al, byte ptr [esi + 3]
L_1186E:
        test ah, 0F0h
L_11871:
        je short L_11887
L_11873:
        shr ah, 4
L_11876:
        add al, ah
L_11878:
        cmp al, 40h
L_1187A:
        jbe short L_1187E
L_1187C:
        mov al, 40h
L_1187E:
        mov byte ptr [esi + 3], al
L_11881:
        call L_119CA
L_11886:
        ret
L_11887:
        sub al, ah
L_11889:
        jge short L_1187E
L_1188B:
        xor al, al
L_1188D:
        jmp short L_1187E
L_1188F:
        xor edx, edx
L_11891:
        mov dh, al
L_11893:
        push esi
L_11894:
        push edi
L_11895:
        movzx eax, byte ptr [esi + 2]
L_11899:
        add edx, dword ptr [eax*4 + g_816c]
L_118A0:
        mov esi, dword ptr [eax*4 + g_81ec]
L_118A7:
        mov edi, dword ptr [eax*4 + g_826c]
L_118AE:
        call L_1199C
L_118B3:
        pop edi
L_118B4:
        pop esi
L_118B5:
        ret
L_118B6:
        pushad
L_118B7:
        xor ax, ax
L_118BA:
        mov word ptr [g_7dd4], ax
L_118C0:
        mov word ptr [g_7dd8], ax
L_118C6:
        mov word ptr [g_7dda], ax
L_118CC:
        mov word ptr [g_7dc8], ax
L_118D2:
        mov ax, 100h
L_118D6:
        mov bx, 434h
L_118DA:
        int 31h
L_118DC:
        jb near ptr L_1197C
L_118E2:
        mov word ptr [g_7dc8], dx
L_118E9:
        mov ecx, 140h
L_118EE:
        movzx edi, ax
L_118F1:
        shl edi, 4
L_118F4:
        mov esi, edi
L_118F6:
        add esi, ecx
L_118F8:
        shl ax, 4
L_118FC:
        neg ax
L_118FF:
        cmp ax, cx
L_11902:
        jae short L_1190C
L_11904:
        mov esi, edi
L_11906:
        add edi, 4200h
L_1190C:
        add esi, 0FFh
L_11912:
        and esi, 0FFFFFF00h
L_11918:
        mov dword ptr [g_7dca], esi
L_1191E:
        mov dword ptr [g_7dce], edi
L_11924:
        mov ecx, 120h
L_11929:
        mov word ptr [g_7dd2], cx
L_11930:
        mov [g_8310], es
L_11936:
        push dword ptr [g_8310]
L_1193C:
        mov ax, ds
L_1193F:
        mov es, eax
L_11941:
        cld
L_11942:
        mov al, 80h
L_11944:
        rep stosb
L_11946:
        pop dword ptr [g_8310]
L_1194C:
        mov es, [g_8310]
L_11952:
        mov edi, dword ptr [g_7dca]
L_11958:
        mov cx, word ptr [g_7de0]
L_1195F:
        shr cx, 3
L_11963:
        xor bx, bx
L_11966:
        mov al, bl
L_11968:
        imul bh
L_1196A:
        sar ax, cl
L_1196D:
        mov byte ptr [edi], ah
L_1196F:
        inc edi
L_11970:
        inc bl
L_11972:
        jne short L_11966
L_11974:
        inc bh
L_11976:
        cmp bh, 40h
L_11979:
        jbe short L_11966
L_1197B:
        clc
L_1197C:
        popad
L_1197D:
        ret
L_1197E:
        pushad
L_1197F:
        mov ax, 101h
L_11983:
        mov dx, word ptr [g_7dc8]
L_1198A:
        test dx, dx
L_1198D:
        je short L_1199A
L_1198F:
        int 31h
L_11991:
        mov word ptr [g_7dc8], 0
L_1199A:
        popad
L_1199B:
        ret
L_1199C:
        push ebx
L_1199D:
        mov ebx, dword ptr [ebx*4 + g_7e92]
L_119A4:
        mov dword ptr [ebx], edx
L_119A6:
        mov dword ptr [ebx + 8], esi
L_119A9:
        mov dword ptr [ebx + 0Ch], edi
L_119AC:
        pop ebx
L_119AD:
        ret
L_119AE:
        push eax
L_119AF:
        push ebx
L_119B0:
        push edx
L_119B1:
        jecxz L_119C6
L_119B3:
        mov ebx, dword ptr [ebx*4 + g_7e92]
L_119BA:
        mov eax, dword ptr [g_7ddc]
L_119BF:
        xor edx, edx
L_119C1:
        div ecx
L_119C3:
        mov dword ptr [ebx + 10h], eax
L_119C6:
        pop edx
L_119C7:
        pop ebx
L_119C8:
        pop eax
L_119C9:
        ret
L_119CA:
        push ebx
L_119CB:
        mov ebx, dword ptr [ebx*4 + g_7e92]
L_119D2:
        mov byte ptr [ebx + 14h], al
L_119D5:
        pop ebx
L_119D6:
        ret
L_119D7:
        push eax
L_119D8:
        push ecx
L_119D9:
        push edx
L_119DA:
        mov ch, dl
L_119DC:
        xor cl, cl
L_119DE:
        mov ax, word ptr [g_7dc0]
L_119E4:
        mov dx, 280h
L_119E8:
        mul dx
L_119EB:
        div cx
L_119EE:
        mov word ptr [g_7dda], ax
L_119F4:
        pop edx
L_119F5:
        pop ecx
L_119F6:
        pop eax
L_119F7:
        ret
L_119F8:
        movzx edi, word ptr [g_7dd4]
L_119FF:
        add edi, dword ptr [g_7dce]
L_11A05:
        movzx ecx, word ptr [g_7dd2]
L_11A0C:
        shr ecx, 1
L_11A0E:
        mov word ptr [g_7dd6], cx
L_11A15:
        xor word ptr [g_7dd4], cx
L_11A1C:
        push edi
L_11A1D:
        mov [g_8310], es
L_11A23:
        push dword ptr [g_8310]
L_11A29:
        mov ax, ds
L_11A2C:
        mov es, eax
L_11A2E:
        mov al, 80h
L_11A30:
        cld
L_11A31:
        rep stosb
L_11A33:
        pop dword ptr [g_8310]
L_11A39:
        mov es, [g_8310]
L_11A3F:
        pop edi
L_11A40:
        cmp word ptr [g_7dd8], 0
L_11A48:
        jg short L_11A5C
L_11A4A:
        call L_11700
L_11A4F:
        mov ax, word ptr [g_7dda]
L_11A55:
        add word ptr [g_7dd8], ax
L_11A5C:
        mov ax, word ptr [g_7dd6]
L_11A62:
        mov cx, word ptr [g_7dd8]
L_11A69:
        add cx, 3Fh
L_11A6D:
        and cx, 0FFC0h
L_11A71:
        cmp ax, cx
L_11A74:
        jle short L_11A79
L_11A76:
        mov ax, cx
L_11A79:
        sub word ptr [g_7dd6], ax
L_11A80:
        sub word ptr [g_7dd8], ax
L_11A87:
        movzx ecx, ax
L_11A8A:
        mov ebx, OFFSET g_7de2
L_11A8F:
        mov dx, word ptr [g_7de0]
L_11A96:
        push ebx
L_11A97:
        push ecx
L_11A98:
        push edx
L_11A99:
        push edi
L_11A9A:
        call L_11AB7
L_11A9F:
        pop edi
L_11AA0:
        pop edx
L_11AA1:
        pop ecx
L_11AA2:
        pop ebx
L_11AA3:
        add ebx, 16h
L_11AA6:
        dec dx
L_11AA8:
        jg short L_11A96
L_11AAA:
        add edi, ecx
L_11AAC:
        cmp word ptr [g_7dd6], 0
L_11AB4:
        jg short L_11A40
L_11AB6:
        ret
L_11AB7:
        push ebx
L_11AB8:
        mov eax, dword ptr [ebx + 0Ch]
L_11ABB:
        mov dword ptr [f_11b04], eax
L_11AC0:
        mov dword ptr [f_11b74], eax
L_11AC5:
        sub eax, dword ptr [ebx + 8]
L_11AC8:
        mov dword ptr [f_11b6e], eax
L_11ACD:
        mov esi, dword ptr [ebx]
L_11ACF:
        mov ebp, dword ptr [ebx + 4]
L_11AD2:
        mov eax, dword ptr [ebx + 10h]
L_11AD5:
        xor edx, edx
L_11AD7:
        shld edx, eax, 10h
L_11ADB:
        shl eax, 10h
L_11ADE:
        mov bh, byte ptr [ebx + 14h]
L_11AE1:
        and ebx, 0FF00h
L_11AE7:
        add ebx, dword ptr [g_7dca]
L_11AED:
        test ecx, 7
L_11AF3:
        jne short L_11AED
L_11AF5:
        shr ecx, 3
L_11AF8:
        jecxz L_11B65
L_11AFA:
        neg ecx
L_11AFC:
        jmp short L_11B02
L_11AFE:
        xchg ebx, ebx
L_11B00:
        nop
L_11B01:
        nop
L_11B02:
        cmp esi, 12345678h
L_11B08:
        jae short L_11B6C
L_11B0A:
        mov bl, byte ptr [esi]
L_11B0C:
        add ebp, eax
L_11B0E:
        mov bl, byte ptr [ebx]
L_11B10:
        adc esi, edx
L_11B12:
        add byte ptr [edi], bl
L_11B14:
        inc edi
L_11B15:
        mov bl, byte ptr [esi]
L_11B17:
        add ebp, eax
L_11B19:
        mov bl, byte ptr [ebx]
L_11B1B:
        adc esi, edx
L_11B1D:
        add byte ptr [edi], bl
L_11B1F:
        inc edi
L_11B20:
        mov bl, byte ptr [esi]
L_11B22:
        add ebp, eax
L_11B24:
        mov bl, byte ptr [ebx]
L_11B26:
        adc esi, edx
L_11B28:
        add byte ptr [edi], bl
L_11B2A:
        inc edi
L_11B2B:
        mov bl, byte ptr [esi]
L_11B2D:
        add ebp, eax
L_11B2F:
        mov bl, byte ptr [ebx]
L_11B31:
        adc esi, edx
L_11B33:
        add byte ptr [edi], bl
L_11B35:
        inc edi
L_11B36:
        mov bl, byte ptr [esi]
L_11B38:
        add ebp, eax
L_11B3A:
        mov bl, byte ptr [ebx]
L_11B3C:
        adc esi, edx
L_11B3E:
        add byte ptr [edi], bl
L_11B40:
        inc edi
L_11B41:
        mov bl, byte ptr [esi]
L_11B43:
        add ebp, eax
L_11B45:
        mov bl, byte ptr [ebx]
L_11B47:
        adc esi, edx
L_11B49:
        add byte ptr [edi], bl
L_11B4B:
        inc edi
L_11B4C:
        mov bl, byte ptr [esi]
L_11B4E:
        add ebp, eax
L_11B50:
        mov bl, byte ptr [ebx]
L_11B52:
        adc esi, edx
L_11B54:
        add byte ptr [edi], bl
L_11B56:
        inc edi
L_11B57:
        mov bl, byte ptr [esi]
L_11B59:
        add ebp, eax
L_11B5B:
        mov bl, byte ptr [ebx]
L_11B5D:
        adc esi, edx
L_11B5F:
        add byte ptr [edi], bl
L_11B61:
        inc edi
L_11B62:
        inc ecx
L_11B63:
        jne short L_11B02
L_11B65:
        pop ebx
L_11B66:
        mov dword ptr [ebx], esi
L_11B68:
        mov dword ptr [ebx + 4], ebp
L_11B6B:
        ret
L_11B6C:
        sub esi, 12345678h
L_11B72:
        cmp esi, 12345678h
L_11B78:
        jb short L_11B0A
L_11B7A:
        jmp short L_11B65
L_11B7C:
        push eax
L_11B7D:
        push ecx
L_11B7E:
        push edx
L_11B7F:
        mov dx, word ptr [g_7dbc]
L_11B86:
        add dx, 0Ch
L_11B8A:
        mov ecx, 10000h
L_11B8F:
        mov ah, al
L_11B91:
        in al, dx
L_11B92:
        and al, 80h
L_11B94:
        loopne L_11B91
L_11B96:
        mov al, ah
L_11B98:
        out dx, al
L_11B99:
        pop edx
L_11B9A:
        pop ecx
L_11B9B:
        pop eax
L_11B9C:
        ret
L_11B9D:
        pushad
L_11B9E:
        mov dx, word ptr [g_7dbc]
L_11BA5:
        add dx, 6
L_11BA9:
        mov al, 1
L_11BAB:
        out dx, al
L_11BAC:
        in al, dx
L_11BAD:
        in al, dx
L_11BAE:
        in al, dx
L_11BAF:
        in al, dx
L_11BB0:
        mov al, 0
L_11BB2:
        out dx, al
L_11BB3:
        add dx, 8
L_11BB7:
        mov ecx, 10000h
L_11BBC:
        in al, dx
L_11BBD:
        and al, 80h
L_11BBF:
        loope L_11BBC
L_11BC1:
        sub dx, 4
L_11BC5:
        in al, dx
L_11BC6:
        cmp al, 0AAh
L_11BC8:
        clc
L_11BC9:
        je short L_11BCC
L_11BCB:
        stc
L_11BCC:
        popad
L_11BCD:
        ret
L_11BCE:
        pushad
L_11BCF:
        call L_11B9D
L_11BD4:
        jb short L_11C19
L_11BD6:
        mov al, 0D1h
L_11BD8:
        call L_11B7C
L_11BDD:
        mov al, 40h
L_11BDF:
        call L_11B7C
L_11BE4:
        mov ax, 3E8h
L_11BE8:
        mul ax
L_11BEB:
        div word ptr [g_7dc0]
L_11BF2:
        neg ax
L_11BF5:
        call L_11B7C
L_11BFA:
        mov al, 14h
L_11BFC:
        call L_11B7C
L_11C01:
        mov ax, word ptr [g_7dd2]
L_11C07:
        shr ax, 1
L_11C0A:
        dec ax
L_11C0C:
        call L_11B7C
L_11C11:
        mov al, ah
L_11C13:
        call L_11B7C
L_11C18:
        clc
L_11C19:
        popad
L_11C1A:
        ret
L_11C1B:
        pushad
L_11C1C:
        call L_11B9D
L_11C21:
        mov al, 0D3h
L_11C23:
        call L_11B7C
L_11C28:
        popad
L_11C29:
        ret
L_11C2A:
        pushad
L_11C2B:
        mov al, byte ptr [g_7dbf]
L_11C30:
        or al, 4
L_11C32:
        out 0Ah, al
L_11C34:
        popad
L_11C35:
        ret
L_11C36:
        pushad
L_11C37:
        mov cl, byte ptr [g_7dbf]
L_11C3D:
        mov al, cl
L_11C3F:
        or al, 4
L_11C41:
        out 0Ah, al
L_11C43:
        out 0Ch, al
L_11C45:
        mov al, cl
L_11C47:
        or al, 58h
L_11C49:
        out 0Bh, al
L_11C4B:
        movzx dx, cl
L_11C4F:
        add dx, dx
L_11C52:
        mov eax, dword ptr [g_7dce]
L_11C57:
        out dx, al
L_11C58:
        mov al, ah
L_11C5A:
        out dx, al
L_11C5B:
        inc dx
L_11C5D:
        mov ax, word ptr [g_7dd2]
L_11C63:
        dec ax
L_11C65:
        out dx, al
L_11C66:
        mov al, ah
L_11C68:
        out dx, al
L_11C69:
        mov edx, 82818387h
L_11C6E:
        shl cl, 3
L_11C71:
        shr edx, cl
L_11C73:
        xor dh, dh
L_11C75:
        shr cl, 3
L_11C78:
        shr eax, 10h
L_11C7B:
        out dx, al
L_11C7C:
        mov al, cl
L_11C7E:
        out 0Ah, al
L_11C80:
        popad
L_11C81:
        ret
L_11C82:
        push ecx
L_11C83:
        push edx
L_11C84:
        movzx dx, byte ptr [g_7dbf]
L_11C8C:
        add dx, dx
L_11C8F:
        inc dx
L_11C91:
        in al, dx
L_11C92:
        mov ah, al
L_11C94:
        in al, dx
L_11C95:
        xchg al, ah
L_11C97:
        mov cx, ax
L_11C9A:
        in al, dx
L_11C9B:
        mov ah, al
L_11C9D:
        in al, dx
L_11C9E:
        xchg al, ah
L_11CA0:
        sub cx, ax
L_11CA3:
        cmp cx, 10h
L_11CA7:
        jg short L_11C97
L_11CA9:
        cmp cx, -10h
L_11CAD:
        jl short L_11C97
L_11CAF:
        neg ax
L_11CB2:
        add ax, word ptr [g_7dd2]
L_11CB9:
        dec ax
L_11CBB:
        pop edx
L_11CBC:
        pop ecx
L_11CBD:
        ret
L_11CBE:
        pushad
L_11CBF:
        mov ax, ds
L_11CC2:
        push eax
L_11CC3:
        in al, 0A1h
L_11CC5:
        mov ah, al
L_11CC7:
        in al, 21h
L_11CC9:
        mov dx, 1
L_11CCD:
        mov cl, byte ptr [g_7dbe]
L_11CD3:
        shl dx, cl
L_11CD6:
        or ax, dx
L_11CD9:
        out 21h, al
L_11CDB:
        mov al, ah
L_11CDD:
        out 0A1h, al
L_11CDF:
        mov ah, 25h
L_11CE1:
        mov al, cl
L_11CE3:
        cmp al, 8
L_11CE5:
        jb short L_11CE9
L_11CE7:
        add al, 60h
L_11CE9:
        add al, 8
L_11CEB:
        lds edx, fword ptr [g_7dc2]
L_11CF1:
        xor ebx, ebx
L_11CF3:
        mov bx, ds
L_11CF6:
        or ebx, edx
L_11CF8:
        test ebx, ebx
L_11CFA:
        je short L_11CFE
L_11CFC:
        int 21h
L_11CFE:
        pop eax
L_11CFF:
        mov ds, eax
L_11D01:
        xor ebx, ebx
L_11D03:
        mov dword ptr [g_7dc2], ebx
L_11D09:
        mov word ptr [g_7dc6], bx
L_11D10:
        popad
L_11D11:
        ret
L_11D12:
        pushad
L_11D13:
        mov ax, ds
L_11D16:
        rol eax, 10h
L_11D19:
        mov ax, es
L_11D1C:
        push eax
L_11D1D:
        mov ah, 35h
L_11D1F:
        mov al, byte ptr [g_7dbe]
L_11D24:
        cmp al, 8
L_11D26:
        jb short L_11D2A
L_11D28:
        add al, 60h
L_11D2A:
        add al, 8
L_11D2C:
        int 21h
L_11D2E:
        mov dword ptr [g_7dc2], ebx
L_11D34:
        mov [g_7dc6], es
L_11D3A:
        mov ah, 25h
L_11D3C:
        mov al, byte ptr [g_7dbe]
L_11D41:
        cmp al, 8
L_11D43:
        jb short L_11D47
L_11D45:
        add al, 60h
L_11D47:
        add al, 8
L_11D49:
        mov dx, cs
L_11D4C:
        mov ds, edx
L_11D4E:
        mov edx, OFFSET f_11d7e
L_11D53:
        int 21h
L_11D55:
        pop eax
L_11D56:
        mov es, eax
L_11D58:
        rol eax, 10h
L_11D5B:
        mov ds, eax
L_11D5D:
        in al, 0A1h
L_11D5F:
        mov al, ah
L_11D61:
        in al, 21h
L_11D63:
        mov dx, 1
L_11D67:
        mov cl, byte ptr [g_7dbe]
L_11D6D:
        shl dx, cl
L_11D70:
        not dx
L_11D73:
        and ax, dx
L_11D76:
        out 21h, al
L_11D78:
        mov al, ah
L_11D7A:
        out 0A1h, al
L_11D7C:
        popad
L_11D7D:
        ret
L_11D7E:
        push eax
L_11D7F:
        mov ax, ds
L_11D82:
        push eax
L_11D83:
        mov ax, SEG DGROUP
L_11D87:
        mov ds, eax
L_11D89:
        push eax
L_11D8A:
        push ecx
L_11D8B:
        push edx
L_11D8C:
        mov dx, word ptr [g_7dbc]
L_11D93:
        add dx, 0Eh
L_11D97:
        in al, dx
L_11D98:
        add dx, -2
L_11D9C:
        mov ecx, 10000h
L_11DA1:
        mov ah, al
L_11DA3:
        in al, dx
L_11DA4:
        and al, 80h
L_11DA6:
        loopne L_11DA3
L_11DA8:
        mov al, 14h
L_11DAA:
        out dx, al
L_11DAB:
        mov ecx, 10000h
L_11DB0:
        in al, dx
L_11DB1:
        and al, 80h
L_11DB3:
        loopne L_11DB0
L_11DB5:
        mov ax, word ptr [g_7dd2]
L_11DBB:
        shr ax, 1
L_11DBE:
        dec ax
L_11DC0:
        out dx, al
L_11DC1:
        mov ecx, 10000h
L_11DC6:
        in al, dx
L_11DC7:
        and al, 80h
L_11DC9:
        loopne L_11DC6
L_11DCB:
        mov ax, word ptr [g_7dd2]
L_11DD1:
        shr ax, 1
L_11DD4:
        dec ax
L_11DD6:
        mov al, ah
L_11DD8:
        out dx, al
L_11DD9:
        pop edx
L_11DDA:
        pop ecx
L_11DDB:
        pop eax
L_11DDC:
        mov al, 20h
L_11DDE:
        cmp byte ptr [g_7dbe], 8
L_11DE5:
        jl short L_11DE9
L_11DE7:
        out 0A0h, al
L_11DE9:
        out 20h, al
L_11DEB:
        sti
L_11DEC:
        pushad
L_11DED:
        call L_119F8
L_11DF2:
        popad
L_11DF3:
        pop eax
L_11DF4:
        mov ds, eax
L_11DF6:
        pop eax
L_11DF7:
        iretd
ASM_TEXT ENDS
        END
