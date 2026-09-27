.386

; Packed C DisplayModeInfo fields shared with the video-mode unit.
DisplayModeInfo STRUC
    mode_state              DW ?
    page_base_addresses     DB 16 DUP (?)
    page_start_offsets      DD 4 DUP (?)
    page_display_offsets    DD 4 DUP (?)
    page_classes            DB 4 DUP (?)
    buffer_size             DD ?
    scanline_bytes          DD ?
    page_height             DD ?
    screen_width            DD ?
    screen_height           DD ?
    view_left               DD ?
    view_top                DD ?
    view_right              DD ?
    view_bottom             DD ?
    mode_flags              DB ?
    saved_reg_1             DB ?
    saved_reg_2             DB ?
    bios_mode               DB ?
    original_mode           DB ?
    saved_reg_3             DB ?
    current_reg_1           DB ?
    current_reg_2           DB ?
    current_reg_3           DB ?
    tail                    DB ?
DisplayModeInfo ENDS

VGA_STATE_PAGE_BASES       EQU 2
VGA_STATE_PAGE_STARTS      EQU 12h
VGA_STATE_PAGE_DISPLAYS    EQU 22h
VGA_STATE_SCANLINE_BYTES   EQU 3Ah
VGA_STATE_PAGE_HEIGHT      EQU 3Eh
VGA_STATE_VIEW_LEFT        EQU 4Ah
VGA_STATE_VIEW_TOP         EQU 4Eh
VGA_STATE_VIEW_RIGHT       EQU 52h
VGA_STATE_VIEW_BOTTOM      EQU 56h
VGA_STATE_CACHED_GC_MODE   EQU 60h
VGA_STATE_CACHED_SEQ_MASK  EQU 61h

; VGA indexed writes put the register index in AL and its value in AH.
VGA_GC_INDEX_DATA_PORT             EQU 3CEh
VGA_SEQ_INDEX_DATA_PORT            EQU 3C4h
VGA_GC_PLANAR_WRITE_MODE_0          EQU 4005h
VGA_GC_PLANAR_WRITE_MODE_1          EQU 4105h
VGA_SEQ_MAP_MASK_ALL_PLANES         EQU 0F02h
VGA_SEQ_MAP_MASK_REGISTER           EQU 2
VGA_MAP_MASK_PLANE_0                EQU 1
VGA_CACHED_GC_PLANAR_WRITE_MODE_0   EQU 40h
VGA_CACHED_GC_PLANAR_WRITE_MODE_1   EQU 41h
VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES  EQU 0Fh

; Sprite validation status values stored in image_buffer_error_code.
SPRITE_ERROR_DIMENSIONS_EXCEED_MODE EQU 401h
SPRITE_ERROR_ORIGIN_OUT_OF_RANGE   EQU 402h
SPRITE_ERROR_UNCLASSIFIED_403      EQU 403h
SPRITE_ERROR_UNCLASSIFIED_404      EQU 404h
SPRITE_ERROR_PAYLOAD_TOO_LARGE     EQU 405h
SPRITE_ERROR_UNSUPPORTED_MODE      EQU 406h
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN image_buffer_error_code:WORD
EXTRN image_color_depth:WORD
EXTRN drawpage:WORD
EXTRN page2:WORD
EXTRN render_page_base:DWORD
EXTRN active_video_page_buffer:DWORD
EXTRN image_buffer_cursor:DWORD
EXTRN vga_state:WORD
EXTRN screen_page_base:DWORD
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
        ; Args at [EBP+8..14h]: sprite origin X/Y, command stream, update-record stream.
        PUBLIC process_sprite_update_list_entry
process_sprite_update_list_entry LABEL NEAR
process_sprite_update_list PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        je short setup_update_list_video_pages
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        mov ax, VGA_GC_PLANAR_WRITE_MODE_0
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
setup_update_list_video_pages:
        movzx ebx, word ptr [drawpage]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        movzx ebx, word ptr [page2]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        mov dword ptr [render_page_base], edi
        mov dword ptr [screen_page_base], esi
        add edi, dword ptr [ebp + 8]
        mov eax, dword ptr [vga_state+VGA_STATE_SCANLINE_BYTES]
        mul dword ptr [ebp + 0Ch]
        add edi, eax
        mov dword ptr [vga_draw_origin], edi
        mov eax, dword ptr [ebp + 8]
        mov dword ptr [sprite_origin_x], eax
        mov eax, dword ptr [ebp + 0Ch]
        mov dword ptr [sprite_origin_y], eax
        mov edi, dword ptr [ebp + 14h]
        mov dword ptr [sprite_record_cursor], edi
        mov edi, dword ptr [ebp + 10h]
        mov dword ptr [sprite_command_cursor], edi
        jmp short update_list_next_command
update_list_process_record:
        movsx ebx, word ptr [edi + 4]
        sub ebx, dword ptr [sprite_origin_x]
        movsx edx, word ptr [edi + 6]
        sub edx, dword ptr [sprite_origin_y]
        mov ax, word ptr [edi + 8]
        mov word ptr [sprite_record_flags], ax
        call clip_and_dispatch_sprite_record
        mov edi, dword ptr [sprite_command_cursor]
update_list_next_command:
        add dword ptr [sprite_command_cursor], 0Ah
        mov esi, dword ptr [edi]
        cmp esi, 0
        jne short update_list_process_record
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
        mov eax, dword ptr [sprite_record_cursor]
        mov dword ptr [active_video_page_buffer], eax
        mov eax, dword ptr [sprite_command_cursor]
        mov dword ptr [image_buffer_cursor], eax
        popad
        ret
process_sprite_update_list ENDP
        PUBLIC replay_sprite_update_list
        ; Replay saved sprite operations and restore VGA register state on exit.
        ; Saved operation records are 14h bytes; the word at each record start selects its handler.
        PUBLIC replay_sprite_update_list_entry
replay_sprite_update_list_entry LABEL NEAR
replay_sprite_update_list PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        movzx ebx, word ptr [drawpage]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        movzx ebx, word ptr [page2]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        cmp word ptr [vga_state], 1
        jne short replay_update_chunky_register_setup
        cmp byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        je short replay_update_planar_register_setup
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
replay_update_planar_register_setup:
        cmp word ptr [vga_state], 1
        jne short replay_update_draw_records
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_1
        je short replay_update_draw_records
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_1
        mov ax, VGA_GC_PLANAR_WRITE_MODE_1
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 1.
replay_update_draw_records:
        jmp short replay_update_dispatch_records
replay_update_chunky_register_setup:
        cmp byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        je short replay_update_set_graphics_mode
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
replay_update_set_graphics_mode:
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        je short replay_update_dispatch_records
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        mov ax, VGA_GC_PLANAR_WRITE_MODE_0
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
replay_update_dispatch_records:
        mov dword ptr [render_page_base], edi
        mov dword ptr [screen_page_base], esi
        cmp word ptr [image_color_depth], 4
        jne short replay_update_restore_vga_state
        mov ebx, dword ptr [active_video_page_buffer]
        mov dword ptr [sprite_record_cursor], ebx
        jmp short replay_update_next_record
replay_update_call_record_handler:
        call dword ptr [ecx*4 + sprite_operation_dispatch_table]
        mov ebx, dword ptr [sprite_record_cursor]
replay_update_next_record:
        add dword ptr [sprite_record_cursor], 14h
        movzx ecx, word ptr [ebx]
        cmp ecx, 0
        jne short replay_update_call_record_handler
replay_update_restore_vga_state:
        cmp word ptr [vga_state], 0
        je short replay_update_done
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        mov ax, VGA_GC_PLANAR_WRITE_MODE_0
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
replay_update_done:
        popad
        ret
replay_sprite_update_list ENDP
        PUBLIC draw_bob_sprite
        ; BOB sprites carry dimensions and encoded pixels; this path clips and dispatches them.
        ; Args at [EBP+8..10h]: BOB record, screen X, screen Y.
        PUBLIC draw_bob_sprite_entry
draw_bob_sprite_entry LABEL NEAR
draw_bob_sprite PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        je short draw_bob_setup_video_pages
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        mov ax, VGA_GC_PLANAR_WRITE_MODE_0
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
draw_bob_setup_video_pages:
        movzx ebx, word ptr [drawpage]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        movzx ebx, word ptr [page2]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        mov dword ptr [render_page_base], esi
        mov dword ptr [screen_page_base], edi
        mov dword ptr [vga_draw_origin], esi
        mov ebx, dword ptr [active_video_page_buffer]
        mov dword ptr [sprite_record_cursor], ebx
        mov ebx, dword ptr [ebp + 0Ch]
        mov edx, dword ptr [ebp + 10h]
        mov esi, dword ptr [ebp + 8]
        call clip_and_dispatch_sprite_record
        mov ebx, dword ptr [sprite_record_cursor]
        mov dword ptr [active_video_page_buffer], ebx
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
        popad
        ret
        ; Validate sprite dimensions, clip to the viewport, then select a mode handler.
        ; Uses ESI for the BOB record and EBX/EDX for the signed screen origin.
        PUBLIC clip_and_dispatch_sprite_record
clip_and_dispatch_sprite_record LABEL NEAR
        mov cx, word ptr [esi + 2]
        mov bp, word ptr [vga_state+VGA_STATE_SCANLINE_BYTES]
        add bp, bp
        cmp cx, bp
        ja near ptr sprite_error_dimensions_exceed_mode
        rol ecx, 10h
        mov cx, word ptr [esi + 4]
        mov bp, word ptr [vga_state+VGA_STATE_PAGE_HEIGHT]
        add bp, bp
        cmp cx, bp
        ja near ptr sprite_error_dimensions_exceed_mode
        movzx ebp, word ptr [esi + 8]
        test word ptr [sprite_record_flags], 1
        je short draw_bob_apply_flag_offsets
        test bp, 2
        je short draw_bob_check_origin_range
        movsx eax, word ptr [esi + 0Eh]
        add ebx, eax
        movsx eax, word ptr [esi + 10h]
        add edx, eax
        jmp short draw_bob_check_origin_range
draw_bob_apply_flag_offsets:
        test bp, 1
        je short draw_bob_check_origin_range
        movsx eax, word ptr [esi + 0Ah]
        add ebx, eax
        movsx eax, word ptr [esi + 0Ch]
        add edx, eax
draw_bob_check_origin_range:
        mov eax, ebx
        or eax, edx
        cmp eax, 7D00h
        jg short sprite_error_origin_out_of_range
        cmp eax, 0FFFF8300h
        jl short sprite_error_origin_out_of_range
        movzx eax, word ptr [esi + 6]
        cmp eax, 1000h
        jg short sprite_error_payload_too_large
        add esi, eax
        and ebp, 7
        cmp word ptr [vga_state], 0
        jne short dispatch_sprite_render_mode
        cmp ebp, 5
        je short sprite_error_unsupported_mode
dispatch_sprite_render_mode:
        shl ebp, 4
        lea edi, [ebp + sprite_render_mode_table]
        movsx ebp, word ptr [image_color_depth]
        mov ebp, dword ptr [ebp + edi]
        jmp dword ptr [edi]
sprite_error_dimensions_exceed_mode:
        mov word ptr [image_buffer_error_code], SPRITE_ERROR_DIMENSIONS_EXCEED_MODE
        jmp short sprite_reject_record
sprite_error_origin_out_of_range:
        mov word ptr [image_buffer_error_code], SPRITE_ERROR_ORIGIN_OUT_OF_RANGE
        jmp short sprite_reject_record
        mov word ptr [image_buffer_error_code], SPRITE_ERROR_UNCLASSIFIED_403
        jmp short sprite_reject_record
        mov word ptr [image_buffer_error_code], SPRITE_ERROR_UNCLASSIFIED_404
        jmp short sprite_reject_record
sprite_error_payload_too_large:
        mov word ptr [image_buffer_error_code], SPRITE_ERROR_PAYLOAD_TOO_LARGE
        jmp short sprite_reject_record
sprite_error_unsupported_mode:
        mov word ptr [image_buffer_error_code], SPRITE_ERROR_UNSUPPORTED_MODE
        jmp short sprite_reject_record
sprite_reject_record:
        mov edi, dword ptr [sprite_command_cursor]
        mov dword ptr [edi], 0
        ret
sprite_render_noop:
        ret
clip_sprite_record:
        sub eax, eax
        mov dword ptr [sprite_clip_top], eax
        mov dword ptr [sprite_clip_bottom], eax
        mov dword ptr [sprite_clip_left], eax
        mov dword ptr [sprite_clip_right], eax
        mov dword ptr [visible_sprite_width], eax
        cmp edx, dword ptr [vga_state+VGA_STATE_VIEW_TOP]
        jge short clip_sprite_check_bottom
        mov eax, dword ptr [vga_state+VGA_STATE_VIEW_TOP]
        sub eax, edx
        sub cx, ax
        jle near ptr clip_sprite_fully_clipped
        mov edx, dword ptr [vga_state+VGA_STATE_VIEW_TOP]
        mov dword ptr [sprite_clip_top], eax
clip_sprite_check_bottom:
        movsx eax, cx
        add eax, edx
        dec eax
        cmp eax, dword ptr [vga_state+VGA_STATE_VIEW_BOTTOM]
        jle short clip_sprite_check_left
        sub eax, dword ptr [vga_state+VGA_STATE_VIEW_BOTTOM]
        sub cx, ax
        jle short clip_sprite_fully_clipped
        mov dword ptr [sprite_clip_bottom], eax
clip_sprite_check_left:
        cmp ebx, dword ptr [vga_state+VGA_STATE_VIEW_LEFT]
        jge short clip_sprite_check_right
        mov eax, dword ptr [vga_state+VGA_STATE_VIEW_LEFT]
        sub eax, ebx
        rol ecx, 10h
        sub cx, ax
        jle short clip_sprite_fully_clipped
        rol ecx, 10h
        mov dword ptr [sprite_clip_left], eax
        mov ebx, dword ptr [vga_state+VGA_STATE_VIEW_LEFT]
        jmp short clip_sprite_finish_bounds
clip_sprite_check_right:
        mov eax, ecx
        shr eax, 10h
        add eax, ebx
        dec eax
        cmp eax, dword ptr [vga_state+VGA_STATE_VIEW_RIGHT]
        jle short clip_sprite_finish_bounds
        sub eax, dword ptr [vga_state+VGA_STATE_VIEW_RIGHT]
        rol ecx, 10h
        sub cx, ax
        jle short clip_sprite_fully_clipped
        mov dword ptr [sprite_clip_right], eax
        movsx eax, cx
        mov dword ptr [visible_sprite_width], eax
        rol ecx, 10h
clip_sprite_finish_bounds:
        jmp short clip_sprite_dispatch_handler
clip_sprite_dispatch_handler:
        mov eax, dword ptr [vga_state+VGA_STATE_SCANLINE_BYTES]
        mul edx
        add eax, ebx
        mov edi, dword ptr [vga_draw_origin]
        add edi, eax
        mov ebx, ecx
        xchg ecx, ebp
        shr ebx, 10h
        movzx ebp, bp
        jmp ecx
clip_sprite_fully_clipped:
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
        PUBLIC visible_sprite_width
visible_sprite_width	DD 0
        PUBLIC vga_plane_index
vga_plane_index	DD 0
        PUBLIC sprite_row_width_remaining
sprite_row_width_remaining LABEL DWORD
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
        DD clip_sprite_record
        DD render_transparent_sprite_record_entry
        DD draw_transparent_sprite_rows
        DD 0
        DD sprite_render_noop
        DD noop_sprite_callback_12970
        DD noop_sprite_callback_12971
        DD 0
        DD clip_sprite_record
        DD render_sprite_record_kind_3_entry
        DD render_sprite_record_kind_3_draw
        DD 0
        DD sprite_render_noop
        DD noop_sprite_callback_12680
        DD noop_sprite_callback_12681
        DD 0
        DD clip_sprite_record
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
