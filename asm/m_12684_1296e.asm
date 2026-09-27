.386
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN g_8360:BYTE
EXTRN g_836c:DWORD
EXTRN g_8370:DWORD
EXTRN g_8378:DWORD
EXTRN g_8380:DWORD
EXTRN g_8384:DWORD
EXTRN g_8388:DWORD
EXTRN g_e2e4:DWORD
EXTRN g_e324:WORD
EXTRN u_e2E0:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC f_12684
f_12684 LABEL NEAR
a_12684 PROC NEAR
        sub eax, eax
L_12686:
        mov ecx, dword ptr [g_8378]
L_1268C:
        jcxz L_1269F
L_1268F:
        mov dl, byte ptr [esi]
L_12691:
        inc esi
L_12692:
        lodsb
L_12693:
        or al, al
L_12695:
        jl short L_12699
L_12697:
        add esi, eax
L_12699:
        dec dl
L_1269B:
        jne short L_12692
L_1269D:
        loop L_1268F
L_1269F:
        mov edx, dword ptr [g_8360]
L_126A5:
        add dword ptr [g_8360], 14h
L_126AC:
        mov word ptr [edx], 3
L_126B1:
        mov word ptr [edx + 4], bx
L_126B5:
        mov word ptr [edx + 2], bp
L_126B9:
        mov dword ptr [edx + 6], esi
L_126BC:
        mov eax, edi
L_126BE:
        sub eax, dword ptr [g_e2e4]
L_126C4:
        mov dword ptr [edx + 0Ah], eax
L_126C7:
        mov eax, dword ptr [g_8380]
L_126CC:
        mov word ptr [edx + 0Eh], ax
L_126D0:
        mov eax, dword ptr [g_8388]
L_126D5:
        mov word ptr [edx + 10h], ax
L_126D9:
        jmp short L_126F6
        PUBLIC f_126db
f_126db LABEL NEAR
L_126DB:
        sub eax, eax
L_126DD:
        mov ecx, dword ptr [g_8378]
L_126E3:
        jcxz L_126F6
L_126E6:
        mov dl, byte ptr [esi]
L_126E8:
        inc esi
L_126E9:
        lodsb
L_126EA:
        or al, al
L_126EC:
        jl short L_126F0
L_126EE:
        add esi, eax
L_126F0:
        dec dl
L_126F2:
        jne short L_126E9
L_126F4:
        loop L_126E6
L_126F6:
        mov edx, dword ptr [g_e324+3Ah]
L_126FC:
        sub edx, ebx
L_126FE:
        mov dword ptr [g_836c], edx
L_12704:
        neg ebp
L_12706:
        cmp dword ptr [g_8380], 0
L_1270D:
        jne short L_12756
L_1270F:
        cmp dword ptr [g_8384], 0
L_12716:
        jne near ptr L_127B2
L_1271C:
        nop
L_1271D:
        nop
L_1271E:
        mov bl, byte ptr [esi]
L_12720:
        neg bl
L_12722:
        inc esi
L_12723:
        mov cl, byte ptr [esi]
L_12725:
        inc esi
L_12726:
        or cl, cl
L_12728:
        jl short L_12748
L_1272A:
        test edi, 1
L_12730:
        je short L_12734
L_12732:
        movsb
L_12733:
        dec ecx
L_12734:
        shr ecx, 1
L_12736:
        rep movsw
L_12739:
        jae short L_1273C
L_1273B:
        movsb
L_1273C:
        inc bl
L_1273E:
        jne short L_12723
L_12740:
        add edi, edx
L_12742:
        inc ebp
L_12743:
        jne short L_1271E
L_12745:
        ret
        ALIGN 4
L_12748:
        neg cl
L_1274A:
        add edi, ecx
L_1274C:
        inc bl
L_1274E:
        jne short L_12723
L_12750:
        add edi, edx
L_12752:
        inc ebp
L_12753:
        jne short L_1271E
L_12755:
        ret
L_12756:
        mov edx, edi
L_12758:
        mov edi, edx
L_1275A:
        mov ebx, dword ptr [g_8380]
L_12760:
        sub edi, ebx
L_12762:
        mov bh, byte ptr [esi]
L_12764:
        neg bh
L_12766:
        inc esi
L_12767:
        mov cl, byte ptr [esi]
L_12769:
        inc esi
L_1276A:
        or cl, cl
L_1276C:
        jl short L_127A0
L_1276E:
        mov eax, edi
L_12770:
        sub eax, edx
L_12772:
        jge short L_12786
L_12774:
        add eax, ecx
L_12776:
        jg short L_1277E
L_12778:
        add esi, ecx
L_1277A:
        add edi, ecx
L_1277C:
        jmp short L_12798
L_1277E:
        sub ecx, eax
L_12780:
        add esi, ecx
L_12782:
        add edi, ecx
L_12784:
        mov ecx, eax
L_12786:
        test edi, 1
L_1278C:
        je short L_12790
L_1278E:
        movsb
L_1278F:
        dec ecx
L_12790:
        shr ecx, 1
L_12792:
        rep movsw
L_12795:
        jae short L_12798
L_12797:
        movsb
L_12798:
        sub eax, eax
L_1279A:
        inc bh
L_1279C:
        jne short L_12767
L_1279E:
        jmp short L_127A8
L_127A0:
        neg cl
L_127A2:
        add edi, ecx
L_127A4:
        inc bh
L_127A6:
        jne short L_12767
L_127A8:
        add edx, dword ptr [g_e324+3Ah]
L_127AE:
        inc ebp
L_127AF:
        jne short L_12758
L_127B1:
        ret
L_127B2:
        mov edx, edi
L_127B4:
        mov edi, edx
L_127B6:
        mov bh, byte ptr [esi]
L_127B8:
        neg bh
L_127BA:
        inc esi
L_127BB:
        mov cl, byte ptr [esi]
L_127BD:
        inc esi
L_127BE:
        or cl, cl
L_127C0:
        jl short L_127F8
L_127C2:
        mov eax, edi
L_127C4:
        sub eax, edx
L_127C6:
        sub eax, dword ptr [g_8388]
L_127CC:
        jge short L_127D6
L_127CE:
        add eax, ecx
L_127D0:
        jl short L_127DA
L_127D2:
        sub ecx, eax
L_127D4:
        jmp short L_127DC
L_127D6:
        add esi, ecx
L_127D8:
        jmp short L_127F0
L_127DA:
        sub eax, eax
L_127DC:
        test edi, 1
L_127E2:
        je short L_127E6
L_127E4:
        movsb
L_127E5:
        dec ecx
L_127E6:
        shr ecx, 1
L_127E8:
        rep movsw
L_127EB:
        jae short L_127EE
L_127ED:
        movsb
L_127EE:
        add esi, eax
L_127F0:
        sub eax, eax
L_127F2:
        inc bh
L_127F4:
        jne short L_127BB
L_127F6:
        jmp short L_12800
L_127F8:
        neg cl
L_127FA:
        add edi, ecx
L_127FC:
        inc bh
L_127FE:
        jne short L_127BB
L_12800:
        add edx, dword ptr [g_e324+3Ah]
L_12806:
        inc ebp
L_12807:
        jne short L_127B4
L_12809:
        ret
        PUBLIC f_1280a
f_1280a LABEL NEAR
L_1280A:
        movzx eax, word ptr [ebx + 0Eh]
L_1280E:
        mov dword ptr [g_8380], eax
L_12813:
        movzx eax, word ptr [ebx + 10h]
L_12817:
        mov dword ptr [g_8384], eax
L_1281C:
        mov esi, dword ptr [ebx + 6]
L_1281F:
        mov edi, dword ptr [g_e2e4]
L_12825:
        mov eax, dword ptr [u_e2E0]
L_1282A:
        sub eax, edi
L_1282C:
        mov dword ptr [g_8370], eax
L_12831:
        add edi, dword ptr [ebx + 0Ah]
L_12834:
        movzx ebp, word ptr [ebx + 2]
L_12838:
        neg ebp
L_1283A:
        sub eax, eax
L_1283C:
        sub ecx, ecx
L_1283E:
        cmp dword ptr [g_8380], 0
L_12845:
        jne short L_128A0
L_12847:
        cmp dword ptr [g_8384], 0
L_1284E:
        jne near ptr L_12906
L_12854:
        mov edx, dword ptr [g_e324+3Ah]
L_1285A:
        sub dx, word ptr [ebx + 4]
L_1285E:
        mov eax, esi
L_12860:
        mov bh, byte ptr [eax]
L_12862:
        neg bh
L_12864:
        inc eax
L_12865:
        mov cl, byte ptr [eax]
L_12867:
        inc eax
L_12868:
        or cl, cl
L_1286A:
        jl short L_12892
L_1286C:
        add eax, ecx
L_1286E:
        mov esi, dword ptr [g_8370]
L_12874:
        add esi, edi
L_12876:
        test edi, 1
L_1287C:
        je short L_12880
L_1287E:
        movsb
L_1287F:
        dec ecx
L_12880:
        shr ecx, 1
L_12882:
        rep movsw
L_12885:
        jae short L_12888
L_12887:
        movsb
L_12888:
        inc bh
L_1288A:
        jne short L_12865
L_1288C:
        add edi, edx
L_1288E:
        inc ebp
L_1288F:
        jne short L_12860
L_12891:
        ret
L_12892:
        neg cl
L_12894:
        add edi, ecx
L_12896:
        inc bh
L_12898:
        jne short L_12865
L_1289A:
        add edi, edx
L_1289C:
        inc ebp
L_1289D:
        jne short L_12860
L_1289F:
        ret
L_128A0:
        mov edx, edi
L_128A2:
        mov edi, edx
L_128A4:
        mov ebx, dword ptr [g_8380]
L_128AA:
        sub edi, ebx
L_128AC:
        mov bh, byte ptr [esi]
L_128AE:
        neg bh
L_128B0:
        inc esi
L_128B1:
        mov cl, byte ptr [esi]
L_128B3:
        inc esi
L_128B4:
        or cl, cl
L_128B6:
        jl short L_128F4
L_128B8:
        add esi, ecx
L_128BA:
        mov eax, edi
L_128BC:
        sub eax, edx
L_128BE:
        jge short L_128CE
L_128C0:
        add eax, ecx
L_128C2:
        jg short L_128C8
L_128C4:
        add edi, ecx
L_128C6:
        jmp short L_128EC
L_128C8:
        sub ecx, eax
L_128CA:
        add edi, ecx
L_128CC:
        mov ecx, eax
L_128CE:
        mov eax, esi
L_128D0:
        mov esi, dword ptr [g_8370]
L_128D6:
        add esi, edi
L_128D8:
        test edi, 1
L_128DE:
        je short L_128E2
L_128E0:
        movsb
L_128E1:
        dec ecx
L_128E2:
        shr ecx, 1
L_128E4:
        rep movsw
L_128E7:
        jae short L_128EA
L_128E9:
        movsb
L_128EA:
        mov esi, eax
L_128EC:
        sub eax, eax
L_128EE:
        inc bh
L_128F0:
        jne short L_128B1
L_128F2:
        jmp short L_128FC
L_128F4:
        neg cl
L_128F6:
        add edi, ecx
L_128F8:
        inc bh
L_128FA:
        jne short L_128B1
L_128FC:
        add edx, dword ptr [g_e324+3Ah]
L_12902:
        inc ebp
L_12903:
        jne short L_128A2
L_12905:
        ret
L_12906:
        mov edx, edi
L_12908:
        mov edi, edx
L_1290A:
        mov bh, byte ptr [esi]
L_1290C:
        neg bh
L_1290E:
        inc esi
L_1290F:
        mov cl, byte ptr [esi]
L_12911:
        inc esi
L_12912:
        or cl, cl
L_12914:
        jl short L_1295C
L_12916:
        add esi, ecx
L_12918:
        mov eax, edi
L_1291A:
        sub eax, edx
L_1291C:
        sub eax, dword ptr [g_8384]
L_12922:
        jge short L_1292C
L_12924:
        add eax, ecx
L_12926:
        jl short L_1292E
L_12928:
        sub ecx, eax
L_1292A:
        jmp short L_1292E
L_1292C:
        jmp short L_1294C
L_1292E:
        mov eax, esi
L_12930:
        mov esi, dword ptr [g_8370]
L_12936:
        add esi, edi
L_12938:
        test edi, 1
L_1293E:
        je short L_12942
L_12940:
        movsb
L_12941:
        dec ecx
L_12942:
        shr ecx, 1
L_12944:
        rep movsw
L_12947:
        jae short L_1294A
L_12949:
        movsb
L_1294A:
        mov esi, eax
L_1294C:
        sub eax, eax
L_1294E:
        inc bh
L_12950:
        jne short L_1290F
L_12952:
        add edx, dword ptr [g_e324+3Ah]
L_12958:
        inc ebp
L_12959:
        jne short L_12908
L_1295B:
        ret
L_1295C:
        neg cl
L_1295E:
        add edi, ecx
L_12960:
        inc bh
L_12962:
        jne short L_1290F
L_12964:
        add edx, dword ptr [g_e324+3Ah]
L_1296A:
        inc ebp
L_1296B:
        jne short L_12908
L_1296D:
        ret
a_12684 ENDP
_TEXT ENDS
        END
