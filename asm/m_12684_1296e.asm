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
SPRITE_KIND_RUN_LENGTH EQU 3
VGA_STATE_ROW_STRIDE_OFFSET EQU 3Ah

DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN sprite_record_cursor:BYTE
EXTRN vga_row_advance:DWORD
EXTRN background_plane_delta:DWORD
EXTRN sprite_clip_top:DWORD
EXTRN sprite_clip_left:DWORD
EXTRN sprite_clip_right:DWORD
EXTRN visible_sprite_width:DWORD
EXTRN render_page_base:DWORD
EXTRN vga_state:WORD
EXTRN screen_page_base:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ; Kind 3 renderer: prepare a clipped sprite record and draw its encoded rows.
        PUBLIC render_sprite_record_kind_3_entry
render_sprite_record_kind_3_entry LABEL NEAR
render_sprite_record_kind_3 PROC NEAR
        sub eax, eax
        mov ecx, dword ptr [sprite_clip_top]
        jcxz kind3_record_after_top_clip
kind3_skip_top_rows:
        mov dl, byte ptr [esi]
        inc esi
kind3_top_skip_run_data:
        lodsb
        or al, al
        jl short kind3_top_skip_run_end
        add esi, eax
kind3_top_skip_run_end:
        dec dl
        jne short kind3_top_skip_run_data
        loop kind3_skip_top_rows
kind3_record_after_top_clip:
        mov edx, dword ptr [sprite_record_cursor]
        add dword ptr [sprite_record_cursor], SPRITE_RECORD_BYTES
        mov word ptr [edx].SPRITE_RECORD.record_kind, SPRITE_KIND_RUN_LENGTH
        mov word ptr [edx].SPRITE_RECORD.record_width_pixels, bx
        mov word ptr [edx].SPRITE_RECORD.record_height_rows, bp
        mov dword ptr [edx].SPRITE_RECORD.record_source_stream, esi
        mov eax, edi
        sub eax, dword ptr [render_page_base]
        mov dword ptr [edx].SPRITE_RECORD.record_page_offset, eax
        mov eax, dword ptr [sprite_clip_left]
        mov word ptr [edx].SPRITE_RECORD.record_clip_left, ax
        mov eax, dword ptr [visible_sprite_width]
        mov word ptr [edx].SPRITE_RECORD.record_clip_width, ax
        jmp short kind3_draw_after_top_clip
        ; Kind 3 draw entry shared with the sprite mode dispatch table.
        PUBLIC render_sprite_record_kind_3_draw
render_sprite_record_kind_3_draw LABEL NEAR
        sub eax, eax
        mov ecx, dword ptr [sprite_clip_top]
        jcxz kind3_draw_after_top_clip
kind3_draw_skip_top_rows:
        mov dl, byte ptr [esi]
        inc esi
kind3_draw_top_skip_run_data:
        lodsb
        or al, al
        jl short kind3_draw_top_skip_run_end
        add esi, eax
kind3_draw_top_skip_run_end:
        dec dl
        jne short kind3_draw_top_skip_run_data
        loop kind3_draw_skip_top_rows
kind3_draw_after_top_clip:
        mov edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        sub edx, ebx
        mov dword ptr [vga_row_advance], edx
        neg ebp
        cmp dword ptr [sprite_clip_left], 0
        jne short kind3_draw_left_clipped
        cmp dword ptr [sprite_clip_right], 0
        jne near ptr kind3_draw_right_clipped
        nop
        nop
kind3_draw_row:
        mov bl, byte ptr [esi]
        neg bl
        inc esi
kind3_draw_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short kind3_draw_skip_transparent_run
        test edi, 1
        je short kind3_draw_copy_words
        movsb
        dec ecx
kind3_draw_copy_words:
        shr ecx, 1
        rep movsw
        jae short kind3_draw_copy_final_byte
        movsb
kind3_draw_copy_final_byte:
        inc bl
        jne short kind3_draw_decode_run
        add edi, edx
        inc ebp
        jne short kind3_draw_row
        ret
        ALIGN 4
kind3_draw_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bl
        jne short kind3_draw_decode_run
        add edi, edx
        inc ebp
        jne short kind3_draw_row
        ret
kind3_draw_left_clipped:
        mov edx, edi
kind3_left_row_start:
        mov edi, edx
        mov ebx, dword ptr [sprite_clip_left]
        sub edi, ebx
        mov bh, byte ptr [esi]
        neg bh
        inc esi
kind3_left_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short kind3_left_skip_transparent_run
        mov eax, edi
        sub eax, edx
        jge short kind3_left_copy_visible_run
        add eax, ecx
        jg short kind3_left_run_crosses_clip_edge
        add esi, ecx
        add edi, ecx
        jmp short kind3_left_run_done
kind3_left_run_crosses_clip_edge:
        sub ecx, eax
        add esi, ecx
        add edi, ecx
        mov ecx, eax
kind3_left_copy_visible_run:
        test edi, 1
        je short kind3_left_copy_words
        movsb
        dec ecx
kind3_left_copy_words:
        shr ecx, 1
        rep movsw
        jae short kind3_left_run_done
        movsb
kind3_left_run_done:
        sub eax, eax
        inc bh
        jne short kind3_left_decode_run
        jmp short kind3_left_next_row
kind3_left_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short kind3_left_decode_run
kind3_left_next_row:
        add edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        inc ebp
        jne short kind3_left_row_start
        ret
kind3_draw_right_clipped:
        mov edx, edi
kind3_right_row_start:
        mov edi, edx
        mov bh, byte ptr [esi]
        neg bh
        inc esi
kind3_right_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short kind3_right_skip_transparent_run
        mov eax, edi
        sub eax, edx
        sub eax, dword ptr [visible_sprite_width]
        jge short kind3_right_run_starts_beyond_clip
        add eax, ecx
        jl short kind3_right_run_ends_before_clip
        sub ecx, eax
        jmp short kind3_right_copy_visible_run
kind3_right_run_starts_beyond_clip:
        add esi, ecx
        jmp short kind3_right_run_done
kind3_right_run_ends_before_clip:
        sub eax, eax
kind3_right_copy_visible_run:
        test edi, 1
        je short kind3_right_copy_words
        movsb
        dec ecx
kind3_right_copy_words:
        shr ecx, 1
        rep movsw
        jae short kind3_right_after_copy_words
        movsb
kind3_right_after_copy_words:
        add esi, eax
kind3_right_run_done:
        sub eax, eax
        inc bh
        jne short kind3_right_decode_run
        jmp short kind3_right_next_row
kind3_right_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short kind3_right_decode_run
kind3_right_next_row:
        add edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        inc ebp
        jne short kind3_right_row_start
        ret
        ; Restore a saved sprite rectangle from the backing image.
        PUBLIC restore_sprite_background_from_record
restore_sprite_background_from_record LABEL NEAR
        movzx eax, word ptr [ebx].SPRITE_RECORD.record_clip_left
        mov dword ptr [sprite_clip_left], eax
        movzx eax, word ptr [ebx].SPRITE_RECORD.record_clip_width
        mov dword ptr [sprite_clip_right], eax
        mov esi, dword ptr [ebx].SPRITE_RECORD.record_source_stream
        mov edi, dword ptr [render_page_base]
        mov eax, dword ptr [screen_page_base]
        sub eax, edi
        mov dword ptr [background_plane_delta], eax
        add edi, dword ptr [ebx].SPRITE_RECORD.record_page_offset
        movzx ebp, word ptr [ebx].SPRITE_RECORD.record_height_rows
        neg ebp
        sub eax, eax
        sub ecx, ecx
        cmp dword ptr [sprite_clip_left], 0
        jne short restore_kind3_left_clipped
        cmp dword ptr [sprite_clip_right], 0
        jne near ptr restore_kind3_right_clipped
        mov edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        sub dx, word ptr [ebx].SPRITE_RECORD.record_width_pixels
        mov eax, esi
restore_kind3_row:
        mov bh, byte ptr [eax]
        neg bh
        inc eax
restore_kind3_decode_run:
        mov cl, byte ptr [eax]
        inc eax
        or cl, cl
        jl short restore_kind3_skip_transparent_run
        add eax, ecx
        mov esi, dword ptr [background_plane_delta]
        add esi, edi
        test edi, 1
        je short restore_kind3_copy_words
        movsb
        dec ecx
restore_kind3_copy_words:
        shr ecx, 1
        rep movsw
        jae short restore_kind3_copy_final_byte
        movsb
restore_kind3_copy_final_byte:
        inc bh
        jne short restore_kind3_decode_run
        add edi, edx
        inc ebp
        jne short restore_kind3_row
        ret
restore_kind3_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short restore_kind3_decode_run
        add edi, edx
        inc ebp
        jne short restore_kind3_row
        ret
restore_kind3_left_clipped:
        mov edx, edi
restore_kind3_left_row_start:
        mov edi, edx
        mov ebx, dword ptr [sprite_clip_left]
        sub edi, ebx
        mov bh, byte ptr [esi]
        neg bh
        inc esi
restore_kind3_left_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short restore_kind3_left_skip_transparent_run
        add esi, ecx
        mov eax, edi
        sub eax, edx
        jge short restore_kind3_left_copy_visible_run
        add eax, ecx
        jg short restore_kind3_left_run_crosses_clip_edge
        add edi, ecx
        jmp short restore_kind3_left_run_done
restore_kind3_left_run_crosses_clip_edge:
        sub ecx, eax
        add edi, ecx
        mov ecx, eax
restore_kind3_left_copy_visible_run:
        mov eax, esi
        mov esi, dword ptr [background_plane_delta]
        add esi, edi
        test edi, 1
        je short restore_kind3_left_copy_words
        movsb
        dec ecx
restore_kind3_left_copy_words:
        shr ecx, 1
        rep movsw
        jae short restore_kind3_left_after_copy_words
        movsb
restore_kind3_left_after_copy_words:
        mov esi, eax
restore_kind3_left_run_done:
        sub eax, eax
        inc bh
        jne short restore_kind3_left_decode_run
        jmp short restore_kind3_left_next_row
restore_kind3_left_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short restore_kind3_left_decode_run
restore_kind3_left_next_row:
        add edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        inc ebp
        jne short restore_kind3_left_row_start
        ret
restore_kind3_right_clipped:
        mov edx, edi
restore_kind3_right_row_start:
        mov edi, edx
        mov bh, byte ptr [esi]
        neg bh
        inc esi
restore_kind3_right_decode_run:
        mov cl, byte ptr [esi]
        inc esi
        or cl, cl
        jl short restore_kind3_right_skip_transparent_run
        add esi, ecx
        mov eax, edi
        sub eax, edx
        sub eax, dword ptr [sprite_clip_right]
        jge short restore_kind3_right_run_starts_beyond_clip
        add eax, ecx
        jl short restore_kind3_right_copy_visible_run
        sub ecx, eax
        jmp short restore_kind3_right_copy_visible_run
restore_kind3_right_run_starts_beyond_clip:
        jmp short restore_kind3_right_run_done
restore_kind3_right_copy_visible_run:
        mov eax, esi
        mov esi, dword ptr [background_plane_delta]
        add esi, edi
        test edi, 1
        je short restore_kind3_right_copy_words
        movsb
        dec ecx
restore_kind3_right_copy_words:
        shr ecx, 1
        rep movsw
        jae short restore_kind3_right_after_copy_words
        movsb
restore_kind3_right_after_copy_words:
        mov esi, eax
restore_kind3_right_run_done:
        sub eax, eax
        inc bh
        jne short restore_kind3_right_decode_run
        add edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        inc ebp
        jne short restore_kind3_right_row_start
        ret
restore_kind3_right_skip_transparent_run:
        neg cl
        add edi, ecx
        inc bh
        jne short restore_kind3_right_decode_run
        add edx, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        inc ebp
        jne short restore_kind3_right_row_start
        ret
render_sprite_record_kind_3 ENDP
_TEXT ENDS
        END
