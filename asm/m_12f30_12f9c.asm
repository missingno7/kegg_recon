.386
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN vga_state:WORD
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC copy_chunky_scanline_to_vga
        ; Split chunky scanline pixels across the four VGA planes.
        PUBLIC copy_chunky_scanline_to_vga_entry
copy_chunky_scanline_to_vga_entry LABEL NEAR
copy_chunky_scanline_to_vga PROC NEAR
        pushad
L_12F31:
        lea ebp, [esp + 1Ch]
L_12F35:
        cmp byte ptr [vga_state+60h], 40h
L_12F3C:
        je short L_12F4F
L_12F3E:
        mov byte ptr [vga_state+60h], 40h
L_12F45:
        mov ax, 4005h
L_12F49:
        mov dx, 3CEh
L_12F4D:
        out dx, ax
L_12F4F:
        mov ah, 1
L_12F51:
        mov byte ptr [vga_state+61h], ah
L_12F57:
        mov al, 2
L_12F59:
        mov dx, 3C4h
L_12F5D:
        out dx, ax
L_12F5F:
        mov esi, dword ptr [ebp + 8]
L_12F62:
        mov edi, dword ptr [ebp + 0Ch]
L_12F65:
        mov ecx, dword ptr [ebp + 10h]
L_12F68:
        shr ecx, 2
L_12F6B:
        mov edx, 3
L_12F70:
        movsb
L_12F71:
        add esi, edx
L_12F73:
        loop L_12F70
L_12F75:
        inc dword ptr [ebp + 8]
L_12F78:
        add ah, ah
L_12F7A:
        cmp ah, 10h
L_12F7D:
        jne short L_12F51
L_12F7F:
        cmp byte ptr [vga_state+61h], 0Fh
L_12F86:
        je short L_12F99
L_12F88:
        mov byte ptr [vga_state+61h], 0Fh
L_12F8F:
        mov ax, 0F02h
L_12F93:
        mov dx, 3C4h
L_12F97:
        out dx, ax
L_12F99:
        popad
L_12F9A:
        ret
        ORG $+1 ; original zero fill to the next even code address
copy_chunky_scanline_to_vga ENDP
_TEXT ENDS
        END
