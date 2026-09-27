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
SPRITE_KIND_TRANSPARENT EQU 1
VGA_STATE_ROW_STRIDE_OFFSET EQU 3Ah

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
        add dword ptr [sprite_record_cursor], SPRITE_RECORD_BYTES
        mov word ptr [edx].SPRITE_RECORD.record_kind, SPRITE_KIND_TRANSPARENT
        mov word ptr [edx].SPRITE_RECORD.record_width_pixels, bx
        mov word ptr [edx].SPRITE_RECORD.record_height_rows, bp
        mov eax, edi
        sub eax, dword ptr [render_page_base]
        mov dword ptr [edx].SPRITE_RECORD.record_page_offset, eax
        ; Decode transparent sprite runs into the clipped destination rows.
        PUBLIC draw_transparent_sprite_rows
draw_transparent_sprite_rows LABEL NEAR
        mov eax, dword ptr [sprite_clip_top]
        mov ecx, ebx
        add ecx, dword ptr [sprite_clip_left]
        add ecx, dword ptr [sprite_clip_right]
        mul ecx
        add esi, eax
        mov edx, dword ptr [sprite_clip_left]
        add esi, edx
        add edx, dword ptr [sprite_clip_right]
        mov eax, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        sub eax, ebx
        mov dword ptr [vga_row_advance], eax
        neg ebp
transparent_row_start:
        mov ecx, ebx
        shr ecx, 3
        je short transparent_full_groups_done
transparent_decode_eight_samples:
        lodsb
        or al, al
        je short transparent_skip_sample_1
        stosb
        lodsb
        or al, al
        je short transparent_skip_sample_2
transparent_store_sample_2:
        stosb
        lodsb
        or al, al
        je short transparent_skip_sample_3
transparent_store_sample_3:
        stosb
        lodsb
        or al, al
        je short transparent_skip_sample_4
transparent_store_sample_4:
        stosb
        lodsb
        or al, al
        je short transparent_skip_sample_5
transparent_store_sample_5:
        stosb
        lodsb
        or al, al
        je short transparent_skip_sample_6
transparent_store_sample_6:
        stosb
        lodsb
        or al, al
        je short transparent_skip_sample_7
transparent_store_sample_7:
        stosb
        lodsb
        or al, al
        je short transparent_skip_sample_8
transparent_store_sample_8:
        stosb
        loop transparent_decode_eight_samples
        jmp short transparent_full_groups_done
transparent_skip_sample_1:
        inc edi
        lodsb
        or al, al
        jne short transparent_store_sample_2
transparent_skip_sample_2:
        inc edi
        lodsb
        or al, al
        jne short transparent_store_sample_3
transparent_skip_sample_3:
        inc edi
        lodsb
        or al, al
        jne short transparent_store_sample_4
transparent_skip_sample_4:
        inc edi
        lodsb
        or al, al
        jne short transparent_store_sample_5
transparent_skip_sample_5:
        inc edi
        lodsb
        or al, al
        jne short transparent_store_sample_6
transparent_skip_sample_6:
        inc edi
        lodsb
        or al, al
        jne short transparent_store_sample_7
transparent_skip_sample_7:
        inc edi
        lodsb
        or al, al
        jne short transparent_store_sample_8
transparent_skip_sample_8:
        inc edi
        loop transparent_decode_eight_samples
transparent_full_groups_done:
        mov ecx, ebx
        and ecx, 7
        je short transparent_advance_row
transparent_decode_tail_samples:
        lodsb
        or al, al
        je short transparent_skip_tail_sample
        stosb
        loop transparent_decode_tail_samples
        jmp short transparent_advance_row
transparent_skip_tail_sample:
        inc edi
        loop transparent_decode_tail_samples
transparent_advance_row:
        add esi, edx
        add edi, dword ptr [vga_row_advance]
        inc ebp
        jne near ptr transparent_row_start
        ret
        ; Copy a saved rectangular patch from the backing image to the active page.
        PUBLIC restore_sprite_rectangle
restore_sprite_rectangle LABEL NEAR
        mov esi, dword ptr [screen_page_base]
        mov edi, dword ptr [render_page_base]
        mov eax, dword ptr [ebx].SPRITE_RECORD.record_page_offset
        add esi, eax
        add edi, eax
        mov eax, esi
        and eax, 3
        sub esi, eax
        sub edi, eax
        movzx ebp, word ptr [ebx].SPRITE_RECORD.record_width_pixels
        add ebp, eax
        add ebp, 3
        and ebp, 0FFFFFFFCh
        mov eax, dword ptr [vga_state+VGA_STATE_ROW_STRIDE_OFFSET]
        sub eax, ebp
        shr ebp, 2
        movzx edx, word ptr [ebx].SPRITE_RECORD.record_height_rows
        neg edx
restore_rectangle_row:
        mov ecx, ebp
        rep movsd
        add esi, eax
        add edi, eax
        inc edx
        jne short restore_rectangle_row
        ret

render_transparent_sprite_record ENDP
_TEXT ENDS
        END
