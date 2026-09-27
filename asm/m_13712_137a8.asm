.386
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN disp_idx:WORD
EXTRN vga_state:WORD
EXTRN draw_idx:WORD
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT
        PUBLIC fill_planar_video_rows
; Fill successive VGA scan-line spans with one repeated pixel value.
fill_planar_video_rows:
        cmp byte ptr [vga_state+61h], 0Fh
L_13719:
        je short L_1372C
L_1371B:
        mov byte ptr [vga_state+61h], 0Fh
L_13722:
        mov ax, 0F02h
L_13726:
        mov dx, 3C4h
L_1372A:
        out dx, ax
L_1372C:
        cmp byte ptr [vga_state+60h], 40h
L_13733:
        je short L_13746
L_13735:
        mov byte ptr [vga_state+60h], 40h
L_1373C:
        mov ax, 4005h
L_13740:
        mov dx, 3CEh
L_13744:
        out dx, ax
L_13746:
        mov ecx, dword ptr [vga_state+3Ah]
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
        add edi, dword ptr [ebx + vga_state+2h]
L_13762:
        add edi, dword ptr [ebx + vga_state+12h]
L_13768:
        add edi, dword ptr [ebx + vga_state+22h]
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
