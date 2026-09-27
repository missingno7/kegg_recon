.386
EXTRN g_7b14:WORD
EXTRN g_e324:WORD
EXTRN g_e326:DWORD
EXTRN g_e336:DWORD
EXTRN g_e346:DWORD
EXTRN g_e35e:DWORD
EXTRN g_e36e:DWORD
EXTRN g_e372:DWORD
EXTRN g_e376:DWORD
EXTRN g_e37a:DWORD
EXTRN g_e386:BYTE
EXTRN g_7b16:WORD
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        PUBLIC L_13712
L_13712:
        cmp byte ptr [g_e385], 0Fh
L_13719:
        je short L_1372C
L_1371B:
        mov byte ptr [g_e385], 0Fh
L_13722:
        mov ax, 0F02h
L_13726:
        mov dx, 3C4h
L_1372A:
        out dx, ax
L_1372C:
        cmp byte ptr [g_e384], 40h
L_13733:
        je short L_13746
L_13735:
        mov byte ptr [g_e384], 40h
L_1373C:
        mov ax, 4005h
L_13740:
        mov dx, 3CEh
L_13744:
        out dx, ax
L_13746:
        mov ecx, dword ptr [g_e35e]
L_1374C:
        mov eax, dword ptr [ebp + 10h]
L_1374F:
        mul ecx
L_13751:
        mov edi, eax
L_13753:
        add edi, dword ptr [ebp + 0Ch]
L_13756:
        mov ebx, dword ptr [ebp + 8]
L_13759:
        shl ebx, 2
L_1375C:
        add edi, dword ptr [ebx + g_e326]
L_13762:
        add edi, dword ptr [ebx + g_e336]
L_13768:
        add edi, dword ptr [ebx + g_e346]
L_1376E:
        mov edx, dword ptr [ebp - 10h]
L_13771:
        mov ebx, edx
L_13773:
        shr edx, 2
L_13776:
        mov dword ptr [ebp - 8], edx
L_13779:
        and ebx, 3
L_1377C:
        mov dword ptr [ebp - 0Ch], ebx
L_1377F:
        sub ecx, dword ptr [ebp - 10h]
L_13782:
        mov esi, ecx
L_13784:
        mov ebx, dword ptr [ebp - 14h]
L_13787:
        mov edx, dword ptr [ebp + 1Ch]
L_1378A:
        mov dh, dl
L_1378C:
        mov eax, edx
L_1378E:
        rol eax, 10h
L_13791:
        mov ax, dx
L_13794:
        mov ecx, dword ptr [ebp - 8]
L_13797:
        rep stosd
L_13799:
        mov ecx, dword ptr [ebp - 0Ch]
L_1379C:
        rep stosb
L_1379E:
        add edi, esi
L_137A0:
        dec ebx
L_137A1:
        jne short L_13794
L_137A3:
        popad
L_137A4:
        mov esp, ebp
L_137A6:
        pop ebp
L_137A7:
        ret
_TEXT ENDS
END
