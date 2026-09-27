.386
; Sprite records are 20-byte clipped draws queued for the renderer.
SPRITE_RECORD STRUC
record_kind          DW ?
record_height_rows    DW ?
record_width_pixels   DW ?
record_source_stream  DD ?
record_page_offset    DD ?
record_clip_left      DW ?
record_clip_width     DW ?
record_clip_top_rows  DW ?
SPRITE_RECORD ENDS
SPRITE_RECORD_BYTES EQU 14h
SPRITE_KIND_VGA_PLANAR EQU 5
VGA_SEQUENCER_INDEX_PORT EQU 3C4h
VGA_SEQUENCER_MAP_MASK_INDEX EQU 2
VGA_STATE_ROW_STRIDE_OFFSET EQU 3Ah
VGA_PLANE_COUNT EQU 4

DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN sprite_record_cursor:BYTE
EXTRN vga_row_advance:DWORD
EXTRN background_plane_delta:DWORD
EXTRN sprite_source_column:WORD
EXTRN sprite_clip_top:DWORD
EXTRN sprite_clip_left:DWORD
EXTRN visible_sprite_width:DWORD
EXTRN vga_plane_index:DWORD
EXTRN sprite_row_width_remaining:DWORD
EXTRN current_vga_plane_mask:BYTE
EXTRN first_vga_plane_mask:BYTE
EXTRN render_page_base:DWORD
EXTRN vga_state:WORD
EXTRN screen_page_base:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ; Kind 5 renderer: record the sprite operation, then decode its clipped run stream.
        PUBLIC render_sprite_record_kind_5_entry
render_sprite_record_kind_5_entry LABEL NEAR
render_sprite_record_kind_5 PROC NEAR
        mov edx, dword ptr [sprite_record_cursor]
        add dword ptr [sprite_record_cursor], SPRITE_RECORD_BYTES
        mov word ptr [edx].SPRITE_RECORD.record_kind, SPRITE_KIND_VGA_PLANAR
        mov word ptr [edx].SPRITE_RECORD.record_width_pixels, bx
        mov word ptr [edx].SPRITE_RECORD.record_height_rows, bp
        movzx eax, word ptr [esi - 2]
        add eax, esi
        mov dword ptr [edx].SPRITE_RECORD.record_source_stream, eax
        mov eax, edi
        sub eax, dword ptr [render_page_base]
        mov dword ptr [edx].SPRITE_RECORD.record_page_offset, eax
        mov eax, dword ptr [sprite_clip_left]
        mov word ptr [edx].SPRITE_RECORD.record_clip_left, ax
        mov eax, dword ptr [visible_sprite_width]
        mov word ptr [edx].SPRITE_RECORD.record_clip_width, ax
        mov eax, dword ptr [sprite_clip_top]
        mov word ptr [edx].SPRITE_RECORD.record_clip_top_rows, ax
        ; Shared kind 5 drawing entry used by the mode dispatch table.
        PUBLIC render_sprite_record_kind_5_draw
render_sprite_record_kind_5_draw LABEL NEAR
        mov ecx, edi
        shr edi, 2
        and cl, 3
        mov ch, 11h
        rol ch, cl
        mov byte ptr [current_vga_plane_mask], ch
        mov byte ptr [first_vga_plane_mask], ch
        neg ebp
        cmp dword ptr [sprite_clip_left], 0
        jne near ptr draw_kind5_left_clipped
        cmp dword ptr [visible_sprite_width], 0
        jne near ptr draw_kind5_right_clipped
        mov word ptr [sprite_source_column], 0Ah
kind5_unclipped_plane_start:
        push ebx
        push ebp
        push esi
        push edi
        mov al, VGA_SEQUENCER_MAP_MASK_INDEX
        mov ah, byte ptr [current_vga_plane_mask]
        mov dx, VGA_SEQUENCER_INDEX_PORT
        ; Select VGA sequencer map-mask register 2 for the current plane.
        out dx, ax
        movzx eax, word ptr [sprite_source_column]
        sub esi, eax
        add ax, word ptr [esi]
        add esi, eax
        add word ptr [sprite_source_column], -2
        sub eax, eax
        mov ecx, dword ptr [sprite_clip_top]
        jcxz kind5_rows_after_top_clip
kind5_skip_top_rows:
        mov dl, byte ptr [esi]
        inc esi
kind5_top_skip_run_data:
        lodsb
        or al, al
        jl short kind5_top_skip_run_end
        add esi, eax
kind5_top_skip_run_end:
        dec dl
        jne short kind5_top_skip_run_data
        loop kind5_skip_top_rows
kind5_rows_after_top_clip:
        mov edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        sub edx, ebx
        shr edx, 2
        mov dword ptr [vga_row_advance], edx
kind5_decode_row:
        mov bh, byte ptr [esi]
        neg bh
        inc esi
kind5_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short kind5_skip_transparent_run
        test edi, 1
        je short kind5_copy_words
        movsb
        dec ecx
kind5_copy_words:
        shr ecx, 1
        rep movsw
        jae short kind5_copy_final_byte
        movsb
kind5_copy_final_byte:
        inc bh
        jne short kind5_decode_run
        add edi, edx
        inc ebp
        jne short kind5_decode_row
        jmp short kind5_plane_done
kind5_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short kind5_decode_run
        add edi, edx
        inc ebp
        jne short kind5_decode_row
kind5_plane_done:
        pop edi
        pop esi
        pop ebp
        pop ebx
        dec ebx
        rol byte ptr [current_vga_plane_mask], 1
        adc edi, 0
        mov dl, byte ptr [current_vga_plane_mask]
        cmp dl, byte ptr [first_vga_plane_mask]
        jne near ptr kind5_unclipped_plane_start
        ret
draw_kind5_left_clipped:
        mov edx, dword ptr [sprite_clip_left]
        and edx, 3
        mov dword ptr [vga_plane_index], edx
        sar dword ptr [sprite_clip_left], 2
        mov edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        shr edx, 2
        mov dword ptr [vga_row_advance], edx
kind5_left_plane_start:
        push ebx
        push ebp
        push esi
        push edi
        mov al, VGA_SEQUENCER_MAP_MASK_INDEX
        mov ah, byte ptr [current_vga_plane_mask]
        mov dx, VGA_SEQUENCER_INDEX_PORT
        ; Select VGA sequencer map-mask register 2 for the current plane.
        out dx, ax
        mov edx, 0FFFFFFFBh
        add edx, dword ptr [vga_plane_index]
        add edx, edx
        movzx edx, word ptr [esi + edx]
        add esi, edx
        sub eax, eax
        mov ecx, dword ptr [sprite_clip_top]
        jcxz kind5_left_rows_after_top_clip
kind5_left_skip_top_rows:
        mov dl, byte ptr [esi]
        inc esi
kind5_left_top_skip_run_data:
        lodsb
        or al, al
        jl short kind5_left_top_skip_run_end
        add esi, eax
kind5_left_top_skip_run_end:
        dec dl
        jne short kind5_left_top_skip_run_data
        loop kind5_left_skip_top_rows
kind5_left_rows_after_top_clip:
        mov edx, edi
kind5_left_row_start:
        mov edi, edx
        mov ebx, dword ptr [sprite_clip_left]
        sub edi, ebx
        mov bh, byte ptr [esi]
        neg bh
        inc esi
kind5_left_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short kind5_left_skip_transparent_run
        mov eax, edi
        sub eax, edx
        jge short kind5_left_copy_visible_run
        add eax, ecx
        jg short kind5_left_run_crosses_clip_edge
        add esi, ecx
        add edi, ecx
        jmp short kind5_left_empty_copy
kind5_left_run_crosses_clip_edge:
        sub ecx, eax
        add esi, ecx
        add edi, ecx
        mov ecx, eax
kind5_left_copy_visible_run:
        test edi, 1
        je short kind5_left_copy_words
        movsb
        dec ecx
kind5_left_copy_words:
        shr ecx, 1
        rep movsw
        jae short kind5_left_empty_copy
        movsb
kind5_left_empty_copy:
        sub eax, eax
        inc bh
        jne short kind5_left_decode_run
        jmp short kind5_left_next_row
kind5_left_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short kind5_left_decode_run
kind5_left_next_row:
        add edx, dword ptr [vga_row_advance]
        inc ebp
        jne short kind5_left_row_start
        inc dword ptr [vga_plane_index]
        cmp dword ptr [vga_plane_index], VGA_PLANE_COUNT
        jne short kind5_left_plane_done
        mov dword ptr [vga_plane_index], 0
        inc dword ptr [sprite_clip_left]
kind5_left_plane_done:
        pop edi
        pop esi
        pop ebp
        pop ebx
        rol byte ptr [current_vga_plane_mask], 1
        adc edi, 0
        mov dl, byte ptr [current_vga_plane_mask]
        cmp dl, byte ptr [first_vga_plane_mask]
        jne near ptr kind5_left_plane_start
        ret
draw_kind5_right_clipped:
        mov word ptr [sprite_source_column], 0Ah
        mov edx, dword ptr [visible_sprite_width]
        mov dword ptr [sprite_row_width_remaining], edx
        mov edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        shr edx, 2
        mov dword ptr [vga_row_advance], edx
kind5_right_plane_start:
        push ebx
        push ebp
        push esi
        push edi
        mov al, VGA_SEQUENCER_MAP_MASK_INDEX
        mov ah, byte ptr [current_vga_plane_mask]
        mov dx, VGA_SEQUENCER_INDEX_PORT
        ; Select VGA sequencer map-mask register 2 for the current plane.
        out dx, ax
        movzx eax, word ptr [sprite_source_column]
        sub esi, eax
        add ax, word ptr [esi]
        add esi, eax
        add word ptr [sprite_source_column], -2
        sub eax, eax
        mov ecx, dword ptr [sprite_clip_top]
        jcxz kind5_right_rows_after_top_clip
kind5_right_skip_top_rows:
        mov dl, byte ptr [esi]
        inc esi
kind5_right_top_skip_run_data:
        lodsb
        or al, al
        jl short kind5_right_top_skip_run_end
        add esi, eax
kind5_right_top_skip_run_end:
        dec dl
        jne short kind5_right_top_skip_run_data
        loop kind5_right_skip_top_rows
kind5_right_rows_after_top_clip:
        mov edx, dword ptr [sprite_row_width_remaining]
        add edx, 3
        sar edx, 2
        mov dword ptr [visible_sprite_width], edx
        dec dword ptr [sprite_row_width_remaining]
        mov edx, edi
kind5_right_row_start:
        mov edi, edx
        mov bh, byte ptr [esi]
        neg bh
        inc esi
kind5_right_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short kind5_right_skip_transparent_run
        mov eax, edi
        sub eax, edx
        sub eax, dword ptr [visible_sprite_width]
        jge short kind5_right_run_starts_beyond_clip
        add eax, ecx
        jl short kind5_right_run_ends_before_clip
        sub ecx, eax
        jmp short kind5_right_copy_visible_run
kind5_right_run_starts_beyond_clip:
        add esi, ecx
        jmp short kind5_right_run_done
kind5_right_run_ends_before_clip:
        sub eax, eax
kind5_right_copy_visible_run:
        test edi, 1
        je short kind5_right_copy_words
        movsb
        dec ecx
kind5_right_copy_words:
        shr ecx, 1
        rep movsw
        jae short kind5_right_skip_source_remainder
        movsb
kind5_right_skip_source_remainder:
        add esi, eax
kind5_right_run_done:
        sub eax, eax
        inc bh
        jne short kind5_right_decode_run
        jmp short kind5_right_next_row
kind5_right_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short kind5_right_decode_run
kind5_right_next_row:
        add edx, dword ptr [vga_row_advance]
        inc ebp
        jne short kind5_right_row_start
        pop edi
        pop esi
        pop ebp
        pop ebx
        rol byte ptr [current_vga_plane_mask], 1
        adc edi, 0
        mov dl, byte ptr [current_vga_plane_mask]
        cmp dl, byte ptr [first_vga_plane_mask]
        jne near ptr kind5_right_plane_start
        ret
        ; Restore a saved rectangle or replay its encoded source, according to record flags.
        PUBLIC restore_sprite_background_record
restore_sprite_background_record LABEL NEAR
        mov cx, word ptr [ebx].SPRITE_RECORD.record_clip_left
        or cx, word ptr [ebx].SPRITE_RECORD.record_clip_width
        jcxz kind5_restore_encoded_background
        mov esi, dword ptr [screen_page_base]
        mov edi, dword ptr [render_page_base]
        mov eax, dword ptr [ebx].SPRITE_RECORD.record_page_offset
        add esi, eax
        add edi, eax
        mov edx, edi
        and edx, 3
        movzx eax, word ptr [ebx].SPRITE_RECORD.record_width_pixels
        add edx, eax
        add edx, 3
        shr edx, 2
        mov eax, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        shr eax, 2
        sub eax, edx
        shr esi, 2
        shr edi, 2
        movzx ebp, word ptr [ebx].SPRITE_RECORD.record_height_rows
        neg ebp
kind5_restore_saved_rectangle_row:
        mov ecx, edx
        rep movsb
        add esi, eax
        add edi, eax
        inc ebp
        jne short kind5_restore_saved_rectangle_row
        ret
kind5_restore_encoded_background:
        mov eax, dword ptr [render_page_base]
        mov edi, dword ptr [screen_page_base]
        sub edi, eax
        mov dword ptr [background_plane_delta], edi
        mov ebp, dword ptr [ebx].SPRITE_RECORD.record_page_offset
        add ebp, eax
        mov dx, word ptr [ebx].SPRITE_RECORD.record_height_rows
        neg dl
        sub eax, eax
        mov esi, dword ptr [ebx].SPRITE_RECORD.record_source_stream
        movzx ecx, word ptr [ebx].SPRITE_RECORD.record_clip_top_rows
        jcxz kind5_restore_encoded_row
kind5_restore_skip_top_row:
        lodsb
        add esi, eax
        loop kind5_restore_skip_top_row
kind5_restore_encoded_row:
        mov ebx, esi
kind5_restore_row_start:
        mov ecx, ebp
        mov dh, byte ptr [ebx]
        neg dh
        inc ebx
kind5_restore_decode_run:
        mov al, byte ptr [ebx]
        inc ebx
        or al, al
        jl short kind5_restore_skip_transparent_run
        mov edi, ecx
        and ecx, 3
        add ecx, eax
        add ecx, 3
        shr ecx, 2
        add eax, edi
        mov esi, dword ptr [background_plane_delta]
        add esi, edi
        shr esi, 2
        shr edi, 2
        rep movsb
        xchg eax, ecx
        inc dh
        jne short kind5_restore_decode_run
        add ebp, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        inc dl
        jne short kind5_restore_row_start
        ret
kind5_restore_skip_transparent_run:
        neg al
        add ecx, eax
        inc dh
        jne short kind5_restore_decode_run
        add ebp, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        inc dl
        jne short kind5_restore_row_start
        ret
render_sprite_record_kind_5 ENDP
_TEXT ENDS
        END
