.386
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN sprite_record_cursor:BYTE
EXTRN vga_row_advance:DWORD
EXTRN sprite_clip_top:DWORD
EXTRN sprite_clip_left:DWORD
EXTRN sprite_clip_right:DWORD
EXTRN render_page_base:DWORD
EXTRN vga_state:WORD
EXTRN screen_page_base:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ; Kind 1 renderer: zero bytes in the row stream leave destination pixels untouched.
        PUBLIC render_transparent_sprite_record_entry
render_transparent_sprite_record_entry LABEL NEAR
render_transparent_sprite_record PROC NEAR
        mov edx, dword ptr [sprite_record_cursor]
L_1297A:
        add dword ptr [sprite_record_cursor], 14h
L_12981:
        mov word ptr [edx], 1
L_12986:
        mov word ptr [edx + 4], bx
L_1298A:
        mov word ptr [edx + 2], bp
L_1298E:
        mov eax, edi
L_12990:
        sub eax, dword ptr [render_page_base]
L_12996:
        mov dword ptr [edx + 0Ah], eax
        ; Decode transparent sprite runs into the clipped destination rows.
        PUBLIC draw_transparent_sprite_rows
draw_transparent_sprite_rows LABEL NEAR
L_12999:
        mov eax, dword ptr [sprite_clip_top]
L_1299E:
        mov ecx, ebx
L_129A0:
        add ecx, dword ptr [sprite_clip_left]
L_129A6:
        add ecx, dword ptr [sprite_clip_right]
L_129AC:
        mul ecx
L_129AE:
        add esi, eax
L_129B0:
        mov edx, dword ptr [sprite_clip_left]
L_129B6:
        add esi, edx
L_129B8:
        add edx, dword ptr [sprite_clip_right]
L_129BE:
        mov eax, dword ptr [vga_state+3Ah]
L_129C3:
        sub eax, ebx
L_129C5:
        mov dword ptr [vga_row_advance], eax
L_129CA:
        neg ebp
L_129CC:
        mov ecx, ebx
L_129CE:
        shr ecx, 3
L_129D1:
        je short L_12A34
L_129D3:
        lodsb
L_129D4:
        or al, al
L_129D6:
        je short L_12A07
L_129D8:
        stosb
L_129D9:
        lodsb
L_129DA:
        or al, al
L_129DC:
        je short L_12A0D
L_129DE:
        stosb
L_129DF:
        lodsb
L_129E0:
        or al, al
L_129E2:
        je short L_12A13
L_129E4:
        stosb
L_129E5:
        lodsb
L_129E6:
        or al, al
L_129E8:
        je short L_12A19
L_129EA:
        stosb
L_129EB:
        lodsb
L_129EC:
        or al, al
L_129EE:
        je short L_12A1F
L_129F0:
        stosb
L_129F1:
        lodsb
L_129F2:
        or al, al
L_129F4:
        je short L_12A25
L_129F6:
        stosb
L_129F7:
        lodsb
L_129F8:
        or al, al
L_129FA:
        je short L_12A2B
L_129FC:
        stosb
L_129FD:
        lodsb
L_129FE:
        or al, al
L_12A00:
        je short L_12A31
L_12A02:
        stosb
L_12A03:
        loop L_129D3
L_12A05:
        jmp short L_12A34
L_12A07:
        inc edi
L_12A08:
        lodsb
L_12A09:
        or al, al
L_12A0B:
        jne short L_129DE
L_12A0D:
        inc edi
L_12A0E:
        lodsb
L_12A0F:
        or al, al
L_12A11:
        jne short L_129E4
L_12A13:
        inc edi
L_12A14:
        lodsb
L_12A15:
        or al, al
L_12A17:
        jne short L_129EA
L_12A19:
        inc edi
L_12A1A:
        lodsb
L_12A1B:
        or al, al
L_12A1D:
        jne short L_129F0
L_12A1F:
        inc edi
L_12A20:
        lodsb
L_12A21:
        or al, al
L_12A23:
        jne short L_129F6
L_12A25:
        inc edi
L_12A26:
        lodsb
L_12A27:
        or al, al
L_12A29:
        jne short L_129FC
L_12A2B:
        inc edi
L_12A2C:
        lodsb
L_12A2D:
        or al, al
L_12A2F:
        jne short L_12A02
L_12A31:
        inc edi
L_12A32:
        loop L_129D3
L_12A34:
        mov ecx, ebx
L_12A36:
        and ecx, 7
L_12A39:
        je short L_12A48
L_12A3B:
        lodsb
L_12A3C:
        or al, al
L_12A3E:
        je short L_12A45
L_12A40:
        stosb
L_12A41:
        loop L_12A3B
L_12A43:
        jmp short L_12A48
L_12A45:
        inc edi
L_12A46:
        loop L_12A3B
L_12A48:
        add esi, edx
L_12A4A:
        add edi, dword ptr [vga_row_advance]
L_12A50:
        inc ebp
L_12A51:
        jne near ptr L_129CC
L_12A57:
        ret
        ; Copy a saved rectangular patch from the backing image to the active page.
        PUBLIC restore_sprite_rectangle
restore_sprite_rectangle LABEL NEAR
L_12A58:
        mov esi, dword ptr [screen_page_base]
L_12A5E:
        mov edi, dword ptr [render_page_base]
L_12A64:
        mov eax, dword ptr [ebx + 0Ah]
L_12A67:
        add esi, eax
L_12A69:
        add edi, eax
L_12A6B:
        mov eax, esi
L_12A6D:
        and eax, 3
L_12A70:
        sub esi, eax
L_12A72:
        sub edi, eax
L_12A74:
        movzx ebp, word ptr [ebx + 4]
L_12A78:
        add ebp, eax
L_12A7A:
        add ebp, 3
L_12A7D:
        and ebp, 0FFFFFFFCh
L_12A80:
        mov eax, dword ptr [vga_state+3Ah]
L_12A85:
        sub eax, ebp
L_12A87:
        shr ebp, 2
L_12A8A:
        movzx edx, word ptr [ebx + 2]
L_12A8E:
        neg edx
L_12A90:
        mov ecx, ebp
L_12A92:
        rep movsd
L_12A94:
        add esi, eax
L_12A96:
        add edi, eax
L_12A98:
        inc edx
L_12A99:
        jne short L_12A90
L_12A9B:
        ret

render_transparent_sprite_record ENDP
_TEXT ENDS
        END
