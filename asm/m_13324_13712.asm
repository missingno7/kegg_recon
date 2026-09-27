.386
; VGA indexed-register ports and the cached planar write settings.
VGA_A000_MEMORY_START       EQU 0A0000h
VGA_B000_MEMORY_START       EQU 0B0000h
VGA_SEQ_INDEX_PORT         EQU 03C4h
VGA_GC_INDEX_PORT          EQU 03CEh
VGA_SEQ_PLANE_MASK_INDEX   EQU 02h
VGA_GC_READ_PLANE_INDEX    EQU 04h
VGA_GC_MODE_INDEX          EQU 05h
VGA_ALL_PLANES_MASK        EQU 0Fh
VGA_GC_PLANAR_MODE_VALUE   EQU 040h
VGA_SEQ_ALL_PLANES_COMMAND EQU 0F02h
VGA_GC_PLANAR_MODE_COMMAND EQU 4005h
; Byte offsets in the shared renderer state block (all byte-packed).
VGA_STATE STRUC
vga_mode_flags           DB 2 DUP (?)
vga_page_origin_group0   DB 10h DUP (?)
vga_page_origin_group1   DB 10h DUP (?)
vga_page_origin_group2   DB 10h DUP (?)
vga_state_reserved_32    DB 8 DUP (?)
vga_screen_stride        DB 4 DUP (?)
vga_state_reserved_3e    DB 0Ch DUP (?)
vga_clip_left            DB 4 DUP (?)
vga_clip_top             DB 4 DUP (?)
vga_clip_right           DB 4 DUP (?)
vga_clip_bottom          DB 4 DUP (?)
vga_state_reserved_5a    DB 6 DUP (?)
vga_gc_mode               DB ?
vga_seq_plane_mask        DB ?
vga_gc_read_plane         DB ?
VGA_STATE ENDS
EXTRN fill_planar_video_rows:NEAR
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN disp_idx:WORD
EXTRN vga_state:WORD
EXTRN draw_idx:WORD
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC read_vga_pixel
        ; Read one palette-indexed pixel while preserving the caller's VGA mode.
        PUBLIC read_vga_pixel_entry
read_vga_pixel_entry LABEL NEAR
read_vga_pixel PROC NEAR
        push ebp
        lea ebp, [esp]
        push ebx
        push edx
        push esi
        mov eax, dword ptr [ebp + 8]
        mov ebx, dword ptr [ebp + 0Ch]
        cmp eax, dword ptr [vga_state+vga_clip_left]
        jge short pixel_x_clamp_low_complete
        mov eax, dword ptr [vga_state+vga_clip_left]
        mov dword ptr [ebp + 8], eax
pixel_x_clamp_low_complete:
        cmp eax, dword ptr [vga_state+vga_clip_right]
        jle short pixel_x_clamp_high_complete
        mov eax, dword ptr [vga_state+vga_clip_right]
        mov dword ptr [ebp + 8], eax
pixel_x_clamp_high_complete:
        cmp ebx, dword ptr [vga_state+vga_clip_top]
        jge short pixel_y_clamp_low_complete
        mov ebx, dword ptr [vga_state+vga_clip_top]
        mov dword ptr [ebp + 0Ch], ebx
pixel_y_clamp_low_complete:
        cmp ebx, dword ptr [vga_state+vga_clip_bottom]
        jle short pixel_y_clamp_high_complete
        mov ebx, dword ptr [vga_state+vga_clip_bottom]
        mov dword ptr [ebp + 0Ch], ebx
pixel_y_clamp_high_complete:
        cmp word ptr [vga_state], 0
        je short read_linear_vga_pixel
        movzx ebx, word ptr [disp_idx]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        mov eax, dword ptr [ebp + 0Ch]
        mul dword ptr [vga_state+vga_screen_stride]
        add eax, dword ptr [ebp + 8]
        add esi, eax
        mov eax, esi
        shr esi, 2
        shl eax, 8
        and ah, 3
        mov byte ptr [vga_state+vga_gc_read_plane], ah
        mov al, VGA_GC_READ_PLANE_INDEX
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
        lodsb
        pop esi
        pop edx
        pop ebx
        pop ebp
        ret
read_linear_vga_pixel:
        movzx ebx, word ptr [disp_idx]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        mov eax, dword ptr [ebp + 0Ch]
        mul dword ptr [vga_state+vga_screen_stride]
        add eax, dword ptr [ebp + 8]
        add esi, eax
        lodsb
        pop esi
        pop edx
        pop ebx
        pop ebp
        ret
read_vga_pixel ENDP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC write_vga_pixel
        ; Write one palette-indexed pixel, selecting its VGA plane when needed.
        PUBLIC write_vga_pixel_entry
write_vga_pixel_entry LABEL NEAR
write_vga_pixel PROC NEAR
        push ebp
        lea ebp, [esp]
        push eax
        push ebx
        push edx
        push esi
        mov eax, dword ptr [ebp + 8]
        mov ebx, dword ptr [ebp + 0Ch]
        cmp eax, dword ptr [vga_state+vga_clip_left]
        jl near ptr write_pixel_return
        cmp eax, dword ptr [vga_state+vga_clip_right]
        jg near ptr write_pixel_return
        cmp ebx, dword ptr [vga_state+vga_clip_top]
        jl short write_pixel_return
        cmp ebx, dword ptr [vga_state+vga_clip_bottom]
        jg short write_pixel_return
        cmp word ptr [vga_state], 0
        je short write_pixel_linear_mode
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        je short write_pixel_planar_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
write_pixel_planar_mode_ready:
        movzx ebx, word ptr [draw_idx]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        mov eax, dword ptr [ebp + 0Ch]
        mul dword ptr [vga_state+vga_screen_stride]
        add eax, dword ptr [ebp + 8]
        add esi, eax
        mov edx, ecx
        mov ecx, esi
        shr esi, 2
        and ecx, 3
        mov ah, 1
        rol ah, cl
        mov ecx, edx
        mov byte ptr [vga_state+vga_seq_plane_mask], ah
        mov al, VGA_SEQ_PLANE_MASK_INDEX
        mov dx, 3C4h
        out dx, ax
        mov eax, dword ptr [ebp + 10h]
        mov byte ptr [esi], al
write_pixel_return:
        pop esi
        pop edx
        pop ebx
        pop eax
        pop ebp
        ret
write_pixel_linear_mode:
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        je short write_pixel_linear_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
write_pixel_linear_mode_ready:
        movzx ebx, word ptr [draw_idx]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add esi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        mov eax, dword ptr [ebp + 0Ch]
        mul dword ptr [vga_state+vga_screen_stride]
        add eax, dword ptr [ebp + 8]
        add esi, eax
        mov eax, dword ptr [ebp + 10h]
        mov byte ptr [esi], al
        pop esi
        pop edx
        pop ebx
        pop eax
        pop ebp
        ret
write_vga_pixel ENDP
        ASSUME CS:_TEXT
        PUBLIC fill_vga_span
        ; Fill a linear run of pixels on the selected page.
        PUBLIC fill_vga_span_entry
fill_vga_span_entry LABEL NEAR
fill_vga_span PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        cmp dword ptr [ebp + 10h], 0
        jg short fill_span_positive_length
        popad
        ret
fill_span_positive_length:
        cmp word ptr [vga_state], 0
        je short fill_linear_span
        cmp byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        je short fill_planar_span_plane_mask_ready
        mov byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        mov ax, VGA_SEQ_ALL_PLANES_COMMAND
        mov dx, 3C4h
        out dx, ax
fill_planar_span_plane_mask_ready:
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        je short fill_planar_span_graphics_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
fill_planar_span_graphics_mode_ready:
        mov ecx, dword ptr [ebp + 10h]
        shr ecx, 4
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        add edi, dword ptr [ebp + 0Ch]
        shr edi, 2
        mov ebx, dword ptr [ebp + 14h]
        mov bh, bl
        mov eax, ebx
        rol eax, 10h
        mov ax, bx
        rep stosd
        mov ecx, dword ptr [ebp + 10h]
        shr ecx, 2
        and ecx, 3
        rep stosb
        popad
        ret
fill_linear_span:
        cmp byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        je short fill_linear_span_plane_mask_ready
        mov byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        mov ax, VGA_SEQ_ALL_PLANES_COMMAND
        mov dx, 3C4h
        out dx, ax
fill_linear_span_plane_mask_ready:
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        je short fill_linear_span_graphics_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
fill_linear_span_graphics_mode_ready:
        mov ecx, dword ptr [ebp + 10h]
        shr ecx, 2
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        mov ebx, dword ptr [ebp + 14h]
        mov bh, bl
        mov eax, ebx
        rol eax, 10h
        mov ax, bx
        rep stosd
        mov ecx, dword ptr [ebp + 10h]
        and ecx, 3
        rep stosb
        popad
        ret
        ; Normalize and clip a rectangle, then fill it with one palette index.
        PUBLIC fill_clipped_vga_rectangle
fill_clipped_vga_rectangle LABEL NEAR
        push ebp
        mov ebp, esp
        sub esp, 14h
        pushad
        mov eax, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 14h]
        mov ecx, dword ptr [ebp + 10h]
        mov edx, dword ptr [ebp + 18h]
        cmp eax, ebx
        jle short rectangle_x_ordered
        xchg eax, ebx
        mov dword ptr [ebp + 0Ch], eax
        mov dword ptr [ebp + 14h], ebx
rectangle_x_ordered:
        cmp ecx, edx
        jle short rectangle_y_ordered
        xchg ecx, edx
        mov dword ptr [ebp + 10h], ecx
        mov dword ptr [ebp + 18h], edx
rectangle_y_ordered:
        cmp eax, dword ptr [vga_state+vga_clip_left]
        jge short rectangle_left_clamped
        mov eax, dword ptr [vga_state+vga_clip_left]
        mov dword ptr [ebp + 0Ch], eax
rectangle_left_clamped:
        cmp ebx, dword ptr [vga_state+vga_clip_right]
        jle short rectangle_right_clamped
        mov ebx, dword ptr [vga_state+vga_clip_right]
        mov dword ptr [ebp + 14h], ebx
rectangle_right_clamped:
        cmp ecx, dword ptr [vga_state+vga_clip_top]
        jge short rectangle_top_clamped
        mov ecx, dword ptr [vga_state+vga_clip_top]
        mov dword ptr [ebp + 10h], ecx
rectangle_top_clamped:
        cmp edx, dword ptr [vga_state+vga_clip_bottom]
        jle short rectangle_bottom_clamped
        mov edx, dword ptr [vga_state+vga_clip_bottom]
        mov dword ptr [ebp + 18h], edx
rectangle_bottom_clamped:
        sub ebx, eax
        inc ebx
        mov dword ptr [ebp - 10h], ebx
        sub edx, ecx
        inc edx
        mov dword ptr [ebp - 14h], edx
        cmp dword ptr [ebp - 10h], 0
        jle short rectangle_empty
        cmp dword ptr [ebp - 14h], 0
        jg short rectangle_has_area
rectangle_empty:
        popad
        mov esp, ebp
        pop ebp
        ret
rectangle_has_area:
        cmp word ptr [vga_state], 0
        je near ptr fill_planar_video_rows
        cmp byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        je short rectangle_plane_mask_ready
        mov byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        mov ax, VGA_SEQ_ALL_PLANES_COMMAND
        mov dx, 3C4h
        out dx, ax
rectangle_plane_mask_ready:
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        je short rectangle_graphics_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
rectangle_graphics_mode_ready:
        mov ecx, dword ptr [vga_state+vga_screen_stride]
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        mov eax, dword ptr [ebp + 10h]
        mul ecx
        add edi, eax
        mov edx, edi
        and edx, 3
        add edi, dword ptr [ebp + 0Ch]
        shr edi, 2
        mov eax, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 14h]
        add eax, edx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        sub ebx, eax
        inc ebx
        shr ecx, 2
        mov esi, ecx
        sub esi, ebx
        mov edx, dword ptr [ebp - 14h]
        mov eax, dword ptr [ebp + 1Ch]
rectangle_fill_next_row:
        mov ecx, ebx
        rep stosb
        add edi, esi
        dec edx
        jne short rectangle_fill_next_row
        popad
        mov esp, ebp
        pop ebp
        ret
fill_vga_span ENDP
_TEXT ENDS
END
