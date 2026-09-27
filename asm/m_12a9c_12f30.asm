.386
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN image_buffer_error_code:WORD
EXTRN image_color_depth:WORD
EXTRN draw_idx:WORD
EXTRN page2:WORD
EXTRN g_e2e4:DWORD
EXTRN active_image_buffer_pointer_2:DWORD
EXTRN image_buffer_cursor:DWORD
EXTRN vga_state:WORD
EXTRN u_e2E0:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
EXTRN render_sprite_record_kind_5_entry:NEAR
EXTRN render_sprite_record_kind_5_draw:NEAR
EXTRN restore_sprite_background_record:NEAR
EXTRN noop_sprite_callback_12680:NEAR
EXTRN noop_sprite_callback_12681:NEAR
EXTRN noop_sprite_callback_12682:NEAR
EXTRN render_sprite_record_kind_3_entry:NEAR
EXTRN render_sprite_record_kind_3_draw:NEAR
EXTRN restore_sprite_background_from_record:NEAR
EXTRN noop_sprite_callback_12970:NEAR
EXTRN noop_sprite_callback_12971:NEAR
EXTRN noop_sprite_callback_12972:NEAR
EXTRN render_transparent_sprite_record_entry:NEAR
EXTRN draw_transparent_sprite_rows:NEAR
EXTRN restore_sprite_rectangle:NEAR
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC process_sprite_update_list
        ; Process queued drawing records against the selected pair of video pages.
        PUBLIC process_sprite_update_list_entry
process_sprite_update_list_entry LABEL NEAR
process_sprite_update_list PROC NEAR
        pushad
L_12A9D:
        lea ebp, [esp + 1Ch]
L_12AA1:
        cmp byte ptr [vga_state+60h], 40h
L_12AA8:
        je short L_12ABB
L_12AAA:
        mov byte ptr [vga_state+60h], 40h
L_12AB1:
        mov ax, 4005h
L_12AB5:
        mov dx, 3CEh
L_12AB9:
        out dx, ax
L_12ABB:
        movzx ebx, word ptr [draw_idx]
L_12AC2:
        shl ebx, 2
L_12AC5:
        mov edi, dword ptr [ebx + vga_state+2h]
L_12ACB:
        add edi, dword ptr [ebx + vga_state+12h]
L_12AD1:
        add edi, dword ptr [ebx + vga_state+22h]
L_12AD7:
        movzx ebx, word ptr [page2]
L_12ADE:
        shl ebx, 2
L_12AE1:
        mov esi, dword ptr [ebx + vga_state+2h]
L_12AE7:
        add esi, dword ptr [ebx + vga_state+12h]
L_12AED:
        add esi, dword ptr [ebx + vga_state+22h]
L_12AF3:
        mov dword ptr [g_e2e4], edi
L_12AF9:
        mov dword ptr [u_e2E0], esi
L_12AFF:
        add edi, dword ptr [ebp + 8]
L_12B02:
        mov eax, dword ptr [vga_state+3Ah]
L_12B07:
        mul dword ptr [ebp + 0Ch]
L_12B0A:
        add edi, eax
L_12B0C:
        mov dword ptr [vga_draw_origin], edi
L_12B12:
        mov eax, dword ptr [ebp + 8]
L_12B15:
        mov dword ptr [sprite_origin_x], eax
L_12B1A:
        mov eax, dword ptr [ebp + 0Ch]
L_12B1D:
        mov dword ptr [sprite_origin_y], eax
L_12B22:
        mov edi, dword ptr [ebp + 14h]
L_12B25:
        mov dword ptr [sprite_record_cursor], edi
L_12B2B:
        mov edi, dword ptr [ebp + 10h]
L_12B2E:
        mov dword ptr [sprite_command_cursor], edi
L_12B34:
        jmp short L_12B5F
L_12B36:
        movsx ebx, word ptr [edi + 4]
L_12B3A:
        sub ebx, dword ptr [sprite_origin_x]
L_12B40:
        movsx edx, word ptr [edi + 6]
L_12B44:
        sub edx, dword ptr [sprite_origin_y]
L_12B4A:
        mov ax, word ptr [edi + 8]
L_12B4E:
        mov word ptr [sprite_record_flags], ax
L_12B54:
        call clip_and_dispatch_sprite_record
L_12B59:
        mov edi, dword ptr [sprite_command_cursor]
L_12B5F:
        add dword ptr [sprite_command_cursor], 0Ah
L_12B66:
        mov esi, dword ptr [edi]
L_12B68:
        cmp esi, 0
L_12B6B:
        jne short L_12B36
L_12B6D:
        mov byte ptr [vga_state+61h], 0Fh
L_12B74:
        mov ax, 0F02h
L_12B78:
        mov dx, 3C4h
L_12B7C:
        out dx, ax
L_12B7E:
        mov eax, dword ptr [sprite_record_cursor]
L_12B83:
        mov dword ptr [active_image_buffer_pointer_2], eax
L_12B88:
        mov eax, dword ptr [sprite_command_cursor]
L_12B8D:
        mov dword ptr [image_buffer_cursor], eax
L_12B92:
        popad
L_12B93:
        ret
process_sprite_update_list ENDP
        PUBLIC replay_sprite_update_list
        ; Replay saved sprite operations and restore VGA register state on exit.
        PUBLIC replay_sprite_update_list_entry
replay_sprite_update_list_entry LABEL NEAR
replay_sprite_update_list PROC NEAR
        pushad
L_12B95:
        lea ebp, [esp + 1Ch]
L_12B99:
        movzx ebx, word ptr [draw_idx]
L_12BA0:
        shl ebx, 2
L_12BA3:
        mov edi, dword ptr [ebx + vga_state+2h]
L_12BA9:
        add edi, dword ptr [ebx + vga_state+12h]
L_12BAF:
        add edi, dword ptr [ebx + vga_state+22h]
L_12BB5:
        movzx ebx, word ptr [page2]
L_12BBC:
        shl ebx, 2
L_12BBF:
        mov esi, dword ptr [ebx + vga_state+2h]
L_12BC5:
        add esi, dword ptr [ebx + vga_state+12h]
L_12BCB:
        add esi, dword ptr [ebx + vga_state+22h]
L_12BD1:
        cmp word ptr [vga_state], 1
L_12BD9:
        jne short L_12C1B
L_12BDB:
        cmp byte ptr [vga_state+61h], 0Fh
L_12BE2:
        je short L_12BF5
L_12BE4:
        mov byte ptr [vga_state+61h], 0Fh
L_12BEB:
        mov ax, 0F02h
L_12BEF:
        mov dx, 3C4h
L_12BF3:
        out dx, ax
L_12BF5:
        cmp word ptr [vga_state], 1
L_12BFD:
        jne short L_12C19
L_12BFF:
        cmp byte ptr [vga_state+60h], 41h
L_12C06:
        je short L_12C19
L_12C08:
        mov byte ptr [vga_state+60h], 41h
L_12C0F:
        mov ax, 4105h
L_12C13:
        mov dx, 3CEh
L_12C17:
        out dx, ax
L_12C19:
        jmp short L_12C4F
L_12C1B:
        cmp byte ptr [vga_state+61h], 0Fh
L_12C22:
        je short L_12C35
L_12C24:
        mov byte ptr [vga_state+61h], 0Fh
L_12C2B:
        mov ax, 0F02h
L_12C2F:
        mov dx, 3C4h
L_12C33:
        out dx, ax
L_12C35:
        cmp byte ptr [vga_state+60h], 40h
L_12C3C:
        je short L_12C4F
L_12C3E:
        mov byte ptr [vga_state+60h], 40h
L_12C45:
        mov ax, 4005h
L_12C49:
        mov dx, 3CEh
L_12C4D:
        out dx, ax
L_12C4F:
        mov dword ptr [g_e2e4], edi
L_12C55:
        mov dword ptr [u_e2E0], esi
L_12C5B:
        cmp word ptr [image_color_depth], 4
L_12C63:
        jne short L_12C8F
L_12C65:
        mov ebx, dword ptr [active_image_buffer_pointer_2]
L_12C6B:
        mov dword ptr [sprite_record_cursor], ebx
L_12C71:
        jmp short L_12C80
L_12C73:
        call dword ptr [ecx*4 + sprite_operation_dispatch_table]
L_12C7A:
        mov ebx, dword ptr [sprite_record_cursor]
L_12C80:
        add dword ptr [sprite_record_cursor], 14h
L_12C87:
        movzx ecx, word ptr [ebx]
L_12C8A:
        cmp ecx, 0
L_12C8D:
        jne short L_12C73
L_12C8F:
        cmp word ptr [vga_state], 0
L_12C97:
        je short L_12CBB
L_12C99:
        mov byte ptr [vga_state+61h], 0Fh
L_12CA0:
        mov ax, 0F02h
L_12CA4:
        mov dx, 3C4h
L_12CA8:
        out dx, ax
L_12CAA:
        mov byte ptr [vga_state+60h], 40h
L_12CB1:
        mov ax, 4005h
L_12CB5:
        mov dx, 3CEh
L_12CB9:
        out dx, ax
L_12CBB:
        popad
L_12CBC:
        ret
replay_sprite_update_list ENDP
        PUBLIC draw_bob_sprite
        ; BOB sprites carry dimensions and encoded pixels; this path clips and dispatches them.
        PUBLIC draw_bob_sprite_entry
draw_bob_sprite_entry LABEL NEAR
draw_bob_sprite PROC NEAR
        pushad
L_12CBE:
        lea ebp, [esp + 1Ch]
L_12CC2:
        cmp byte ptr [vga_state+60h], 40h
L_12CC9:
        je short L_12CDC
L_12CCB:
        mov byte ptr [vga_state+60h], 40h
L_12CD2:
        mov ax, 4005h
L_12CD6:
        mov dx, 3CEh
L_12CDA:
        out dx, ax
L_12CDC:
        movzx ebx, word ptr [draw_idx]
L_12CE3:
        shl ebx, 2
L_12CE6:
        mov esi, dword ptr [ebx + vga_state+2h]
L_12CEC:
        add esi, dword ptr [ebx + vga_state+12h]
L_12CF2:
        add esi, dword ptr [ebx + vga_state+22h]
L_12CF8:
        movzx ebx, word ptr [page2]
L_12CFF:
        shl ebx, 2
L_12D02:
        mov edi, dword ptr [ebx + vga_state+2h]
L_12D08:
        add edi, dword ptr [ebx + vga_state+12h]
L_12D0E:
        add edi, dword ptr [ebx + vga_state+22h]
L_12D14:
        mov dword ptr [g_e2e4], esi
L_12D1A:
        mov dword ptr [u_e2E0], edi
L_12D20:
        mov dword ptr [vga_draw_origin], esi
L_12D26:
        mov ebx, dword ptr [active_image_buffer_pointer_2]
L_12D2C:
        mov dword ptr [sprite_record_cursor], ebx
L_12D32:
        mov ebx, dword ptr [ebp + 0Ch]
L_12D35:
        mov edx, dword ptr [ebp + 10h]
L_12D38:
        mov esi, dword ptr [ebp + 8]
L_12D3B:
        call L_12D5F
L_12D40:
        mov ebx, dword ptr [sprite_record_cursor]
L_12D46:
        mov dword ptr [active_image_buffer_pointer_2], ebx
L_12D4C:
        mov byte ptr [vga_state+61h], 0Fh
L_12D53:
        mov ax, 0F02h
L_12D57:
        mov dx, 3C4h
L_12D5B:
        out dx, ax
L_12D5D:
        popad
L_12D5E:
        ret
        ; Validate sprite dimensions, clip to the viewport, then select a mode handler.
        PUBLIC clip_and_dispatch_sprite_record
clip_and_dispatch_sprite_record LABEL NEAR
L_12D5F:
        mov cx, word ptr [esi + 2]
L_12D63:
        mov bp, word ptr [vga_state+3Ah]
L_12D6A:
        add bp, bp
L_12D6D:
        cmp cx, bp
L_12D70:
        ja near ptr L_12E0E
L_12D76:
        rol ecx, 10h
L_12D79:
        mov cx, word ptr [esi + 4]
L_12D7D:
        mov bp, word ptr [vga_state+3Eh]
L_12D84:
        add bp, bp
L_12D87:
        cmp cx, bp
L_12D8A:
        ja near ptr L_12E0E
L_12D90:
        movzx ebp, word ptr [esi + 8]
L_12D94:
        test word ptr [sprite_record_flags], 1
L_12D9D:
        je short L_12DB4
L_12D9F:
        test bp, 2
L_12DA4:
        je short L_12DC7
L_12DA6:
        movsx eax, word ptr [esi + 0Eh]
L_12DAA:
        add ebx, eax
L_12DAC:
        movsx eax, word ptr [esi + 10h]
L_12DB0:
        add edx, eax
L_12DB2:
        jmp short L_12DC7
L_12DB4:
        test bp, 1
L_12DB9:
        je short L_12DC7
L_12DBB:
        movsx eax, word ptr [esi + 0Ah]
L_12DBF:
        add ebx, eax
L_12DC1:
        movsx eax, word ptr [esi + 0Ch]
L_12DC5:
        add edx, eax
L_12DC7:
        mov eax, ebx
L_12DC9:
        or eax, edx
L_12DCB:
        cmp eax, 7D00h
L_12DD0:
        jg short L_12E19
L_12DD2:
        cmp eax, 0FFFF8300h
L_12DD7:
        jl short L_12E19
L_12DD9:
        movzx eax, word ptr [esi + 6]
L_12DDD:
        cmp eax, 1000h
L_12DE2:
        jg short L_12E3A
L_12DE4:
        add esi, eax
L_12DE6:
        and ebp, 7
L_12DE9:
        cmp word ptr [vga_state], 0
L_12DF1:
        jne short L_12DF8
L_12DF3:
        cmp ebp, 5
L_12DF6:
        je short L_12E45
L_12DF8:
        shl ebp, 4
L_12DFB:
        lea edi, [ebp + sprite_render_mode_table]
L_12E01:
        movsx ebp, word ptr [image_color_depth]
L_12E08:
        mov ebp, dword ptr [ebp + edi]
L_12E0C:
        jmp dword ptr [edi]
L_12E0E:
        mov word ptr [image_buffer_error_code], 401h
L_12E17:
        jmp short L_12E50
L_12E19:
        mov word ptr [image_buffer_error_code], 402h
L_12E22:
        jmp short L_12E50
L_12E24:
        mov word ptr [image_buffer_error_code], 403h
L_12E2D:
        jmp short L_12E50
L_12E2F:
        mov word ptr [image_buffer_error_code], 404h
L_12E38:
        jmp short L_12E50
L_12E3A:
        mov word ptr [image_buffer_error_code], 405h
L_12E43:
        jmp short L_12E50
L_12E45:
        mov word ptr [image_buffer_error_code], 406h
L_12E4E:
        jmp short L_12E50
L_12E50:
        mov edi, dword ptr [sprite_command_cursor]
L_12E56:
        mov dword ptr [edi], 0
L_12E5C:
        ret
L_12E5D:
        ret
L_12E5E:
        sub eax, eax
L_12E60:
        mov dword ptr [sprite_clip_top], eax
L_12E65:
        mov dword ptr [sprite_clip_bottom], eax
L_12E6A:
        mov dword ptr [sprite_clip_left], eax
L_12E6F:
        mov dword ptr [sprite_clip_right], eax
L_12E74:
        mov dword ptr [g_8388], eax
L_12E79:
        cmp edx, dword ptr [vga_state+4Eh]
L_12E7F:
        jge short L_12E9C
L_12E81:
        mov eax, dword ptr [vga_state+4Eh]
L_12E86:
        sub eax, edx
L_12E88:
        sub cx, ax
L_12E8B:
        jle near ptr L_12F2E
L_12E91:
        mov edx, dword ptr [vga_state+4Eh]
L_12E97:
        mov dword ptr [sprite_clip_top], eax
L_12E9C:
        movsx eax, cx
L_12E9F:
        add eax, edx
L_12EA1:
        dec eax
L_12EA2:
        cmp eax, dword ptr [vga_state+56h]
L_12EA8:
        jle short L_12EBA
L_12EAA:
        sub eax, dword ptr [vga_state+56h]
L_12EB0:
        sub cx, ax
L_12EB3:
        jle short L_12F2E
L_12EB5:
        mov dword ptr [sprite_clip_bottom], eax
L_12EBA:
        cmp ebx, dword ptr [vga_state+4Ah]
L_12EC0:
        jge short L_12EE1
L_12EC2:
        mov eax, dword ptr [vga_state+4Ah]
L_12EC7:
        sub eax, ebx
L_12EC9:
        rol ecx, 10h
L_12ECC:
        sub cx, ax
L_12ECF:
        jle short L_12F2E
L_12ED1:
        rol ecx, 10h
L_12ED4:
        mov dword ptr [sprite_clip_left], eax
L_12ED9:
        mov ebx, dword ptr [vga_state+4Ah]
L_12EDF:
        jmp short L_12F0F
L_12EE1:
        mov eax, ecx
L_12EE3:
        shr eax, 10h
L_12EE6:
        add eax, ebx
L_12EE8:
        dec eax
L_12EE9:
        cmp eax, dword ptr [vga_state+52h]
L_12EEF:
        jle short L_12F0F
L_12EF1:
        sub eax, dword ptr [vga_state+52h]
L_12EF7:
        rol ecx, 10h
L_12EFA:
        sub cx, ax
L_12EFD:
        jle short L_12F2E
L_12EFF:
        mov dword ptr [sprite_clip_right], eax
L_12F04:
        movsx eax, cx
L_12F07:
        mov dword ptr [g_8388], eax
L_12F0C:
        rol ecx, 10h
L_12F0F:
        jmp short L_12F11
L_12F11:
        mov eax, dword ptr [vga_state+3Ah]
L_12F16:
        mul edx
L_12F18:
        add eax, ebx
L_12F1A:
        mov edi, dword ptr [vga_draw_origin]
L_12F20:
        add edi, eax
L_12F22:
        mov ebx, ecx
L_12F24:
        xchg ecx, ebp
L_12F26:
        shr ebx, 10h
L_12F29:
        movzx ebp, bp
L_12F2C:
        jmp ecx
L_12F2E:
        ret
        ORG $+1 ; original zero fill to the next even code address
draw_bob_sprite ENDP
_TEXT ENDS
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
        ; Renderer scratch state, VGA plane caches, and sprite-operation dispatch tables.
        PUBLIC sprite_record_cursor
sprite_record_cursor	DD 2 DUP (0)
        PUBLIC vga_draw_origin
vga_draw_origin	DD 0
        PUBLIC vga_row_advance
vga_row_advance	DD 0
        PUBLIC background_plane_delta
background_plane_delta	DD 0
        PUBLIC sprite_record_flags
sprite_record_flags	DW 0
        PUBLIC sprite_source_column
sprite_source_column	DW 0
        PUBLIC sprite_clip_top
sprite_clip_top	DD 0
        PUBLIC sprite_clip_bottom
sprite_clip_bottom	DD 0
        PUBLIC sprite_clip_left
sprite_clip_left	DD 0
        PUBLIC sprite_clip_right
sprite_clip_right	DD 0
        PUBLIC g_8388
g_8388	DD 0
        PUBLIC vga_plane_index
vga_plane_index	DD 0
        PUBLIC g_8390
g_8390 LABEL DWORD
        DB 18 DUP (0)
        PUBLIC sprite_operation_dispatch_table
sprite_operation_dispatch_table LABEL DWORD
        DB 0h, 0h
        PUBLIC current_vga_plane_mask
current_vga_plane_mask	DB 0
        PUBLIC first_vga_plane_mask
first_vga_plane_mask	DB 0
        DD restore_sprite_rectangle
        PUBLIC sprite_render_mode_table
sprite_render_mode_table	DD noop_sprite_callback_12972
        DD restore_sprite_background_from_record
        DD noop_sprite_callback_12682
        DD restore_sprite_background_record
        DD L_12E5E
        DD render_transparent_sprite_record_entry
        DD draw_transparent_sprite_rows
        DD 0
        DD L_12E5D
        DD noop_sprite_callback_12970
        DD noop_sprite_callback_12971
        DD 0
        DD L_12E5E
        DD render_sprite_record_kind_3_entry
        DD render_sprite_record_kind_3_draw
        DD 0
        DD L_12E5D
        DD noop_sprite_callback_12680
        DD noop_sprite_callback_12681
        DD 0
        DD L_12E5E
        DD render_sprite_record_kind_5_entry
        DD render_sprite_record_kind_5_draw
        PUBLIC sprite_origin_x
sprite_origin_x	DD 0
        PUBLIC sprite_origin_y
sprite_origin_y	DD 0
        PUBLIC sprite_command_cursor
sprite_command_cursor LABEL DWORD
        DB 10 DUP (0)
_DATA ENDS
        END
