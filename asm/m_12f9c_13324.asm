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
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN vga_state:WORD
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC copy_screen_span
        ; Copy one linear framebuffer span, using planar or packed VGA access.
        ; Args at [EBP+8..18h]: source page/offset, destination page/offset, byte count.
        PUBLIC copy_screen_span_entry
copy_screen_span_entry LABEL NEAR
copy_screen_span PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        cmp dword ptr [ebp + 18h], 0
        jg short copy_span_select_vga_mode
        popad
        ret
copy_span_select_vga_mode:
        cmp word ptr [vga_state], 0
        je near ptr copy_span_prepare_chunky_transfer
        cmp byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        je short copy_span_select_transfer_mode
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
copy_span_select_transfer_mode:
        cmp word ptr [vga_state], 1
        jne short copy_span_planar_transfer
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_1
        je short copy_span_planar_transfer
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_1
        mov ax, VGA_GC_PLANAR_WRITE_MODE_1
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 1.
copy_span_planar_transfer:
        mov ecx, dword ptr [ebp + 18h]
        shr ecx, 2
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        add esi, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 10h]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        add edi, dword ptr [ebp + 14h]
        shr esi, 2
        shr edi, 2
        rep movsb
        popad
        ret
copy_span_prepare_chunky_transfer:
        cmp byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        je short copy_span_chunky_transfer
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
copy_span_chunky_transfer:
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        je short copy_span_calculate_source_address
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        mov ax, VGA_GC_PLANAR_WRITE_MODE_0
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
copy_span_calculate_source_address:
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        mov esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        add esi, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 10h]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        add edi, dword ptr [ebp + 14h]
        mov ecx, dword ptr [ebp + 18h]
        shr ecx, 2
        rep movsd
        mov ecx, dword ptr [ebp + 18h]
        and ecx, 3
        rep movsb
        popad
        ret
        ; Clip source and destination bounds before copying a screen rectangle.
        ; Args at [EBP+8..24h]: source page/bounds, destination page and destination origin.
        PUBLIC copy_clipped_screen_rectangle
copy_clipped_screen_rectangle LABEL NEAR
        push ebp
        mov ebp, esp
        sub esp, 14h
        pushad
        mov eax, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 14h]
        mov ecx, dword ptr [ebp + 10h]
        mov edx, dword ptr [ebp + 18h]
        cmp eax, ebx
        jle short clip_rect_order_vertical_bounds
        xchg eax, ebx
        mov dword ptr [ebp + 0Ch], eax
        mov dword ptr [ebp + 14h], ebx
clip_rect_order_vertical_bounds:
        cmp ecx, edx
        jle short clip_rect_clamp_left
        xchg ecx, edx
        mov dword ptr [ebp + 10h], ecx
        mov dword ptr [ebp + 18h], edx
clip_rect_clamp_left:
        cmp eax, dword ptr [vga_state+VGA_STATE_VIEW_LEFT]
        jge short clip_rect_clamp_right
        mov eax, dword ptr [vga_state+VGA_STATE_VIEW_LEFT]
        mov dword ptr [ebp + 0Ch], eax
clip_rect_clamp_right:
        cmp ebx, dword ptr [vga_state+VGA_STATE_VIEW_RIGHT]
        jle short clip_rect_clamp_top
        mov ebx, dword ptr [vga_state+VGA_STATE_VIEW_RIGHT]
        mov dword ptr [ebp + 14h], ebx
clip_rect_clamp_top:
        cmp ecx, dword ptr [vga_state+VGA_STATE_VIEW_TOP]
        jge short clip_rect_clamp_bottom
        mov ecx, dword ptr [vga_state+VGA_STATE_VIEW_TOP]
        mov dword ptr [ebp + 10h], ecx
clip_rect_clamp_bottom:
        cmp edx, dword ptr [vga_state+VGA_STATE_VIEW_BOTTOM]
        jle short clip_rect_measure_source
        mov edx, dword ptr [vga_state+VGA_STATE_VIEW_BOTTOM]
        mov dword ptr [ebp + 18h], edx
clip_rect_measure_source:
        sub ebx, eax
        inc ebx
        mov dword ptr [ebp - 10h], ebx
        sub edx, ecx
        inc edx
        mov dword ptr [ebp - 14h], edx
        mov eax, dword ptr [ebp + 20h]
        mov ebx, dword ptr [ebp + 24h]
        cmp eax, dword ptr [vga_state+VGA_STATE_VIEW_LEFT]
        jge short clip_rect_check_destination_right
        mov eax, dword ptr [vga_state+VGA_STATE_VIEW_LEFT]
        mov dword ptr [ebp + 20h], eax
clip_rect_check_destination_right:
        mov edx, dword ptr [ebp - 10h]
        add edx, eax
        sub edx, dword ptr [vga_state+VGA_STATE_VIEW_RIGHT]
        jle short clip_rect_check_destination_top
        mov ecx, dword ptr [vga_state+VGA_STATE_VIEW_RIGHT]
        sub ecx, eax
        jl near ptr clip_rect_done
        inc ecx
        mov edx, dword ptr [ebp - 10h]
        mov dword ptr [ebp - 10h], ecx
        sub dword ptr [ebp + 14h], edx
        add dword ptr [ebp + 14h], ecx
clip_rect_check_destination_top:
        cmp ebx, dword ptr [vga_state+VGA_STATE_VIEW_TOP]
        jge short clip_rect_check_destination_bottom
        mov ebx, dword ptr [vga_state+VGA_STATE_VIEW_TOP]
        mov dword ptr [ebp + 24h], ebx
clip_rect_check_destination_bottom:
        mov edx, dword ptr [ebp - 14h]
        add edx, ebx
        sub edx, dword ptr [vga_state+VGA_STATE_VIEW_BOTTOM]
        jle short clip_rect_select_vga_mode
        mov ecx, dword ptr [vga_state+VGA_STATE_VIEW_BOTTOM]
        sub ecx, ebx
        jl near ptr clip_rect_done
        inc ecx
        mov edx, dword ptr [ebp - 14h]
        mov dword ptr [ebp - 14h], ecx
        sub dword ptr [ebp + 18h], edx
        add dword ptr [ebp + 18h], ecx
clip_rect_select_vga_mode:
        cmp word ptr [vga_state], 0
        je near ptr clip_rect_chunky_transfer
        cmp byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        je short clip_rect_planar_transfer
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
clip_rect_planar_transfer:
        cmp word ptr [vga_state], 1
        jne short clip_rect_planar_row_setup
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_1
        je short clip_rect_planar_row_setup
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_1
        mov ax, VGA_GC_PLANAR_WRITE_MODE_1
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 1.
clip_rect_planar_row_setup:
        mov ecx, dword ptr [vga_state+VGA_STATE_SCANLINE_BYTES]
        mov eax, dword ptr [ebp + 10h]
        mul ecx
        mov esi, eax
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        mov ebx, dword ptr [ebp + 1Ch]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        mov eax, dword ptr [ebp + 24h]
        mul ecx
        add eax, dword ptr [ebp + 20h]
        add edi, eax
        shr edi, 2
        mov edx, esi
        and edx, 3
        add esi, dword ptr [ebp + 0Ch]
        shr esi, 2
        mov eax, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 14h]
        add eax, edx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        sub ebx, eax
        inc ebx
        shr ecx, 2
        mov eax, ecx
        sub eax, ebx
        mov edx, dword ptr [ebp - 14h]
clip_rect_planar_next_row:
        mov ecx, ebx
        rep movsb
        add esi, eax
        add edi, eax
        dec edx
        jne short clip_rect_planar_next_row
clip_rect_done:
        popad
        mov esp, ebp
        pop ebp
        ret
clip_rect_chunky_transfer:
        cmp byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        je short clip_rect_chunky_row_setup
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
clip_rect_chunky_row_setup:
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        je short clip_rect_chunky_addresses
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        mov ax, VGA_GC_PLANAR_WRITE_MODE_0
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
clip_rect_chunky_addresses:
        mov ecx, dword ptr [vga_state+VGA_STATE_SCANLINE_BYTES]
        mov eax, dword ptr [ebp + 10h]
        mul ecx
        mov esi, dword ptr [ebp + 0Ch]
        add esi, eax
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add esi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        mov ebx, dword ptr [ebp + 1Ch]
        shl ebx, 2
        mov edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_BASES]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_STARTS]
        add edi, dword ptr [ebx + vga_state+VGA_STATE_PAGE_DISPLAYS]
        mov eax, dword ptr [ebp + 24h]
        mul ecx
        add eax, dword ptr [ebp + 20h]
        add edi, eax
        mov edx, dword ptr [ebp - 10h]
        mov ebx, edx
        shr edx, 2
        mov dword ptr [ebp - 8], edx
        and ebx, 3
        mov dword ptr [ebp - 0Ch], ebx
        sub ecx, dword ptr [ebp - 10h]
        mov eax, ecx
        mov ebx, dword ptr [ebp - 14h]
clip_rect_chunky_next_row:
        mov ecx, dword ptr [ebp - 8]
        rep movsd
        mov ecx, dword ptr [ebp - 0Ch]
        rep movsb
        add esi, eax
        add edi, eax
        dec ebx
        jne short clip_rect_chunky_next_row
        popad
        mov esp, ebp
        pop ebp
        ret
        add byte ptr [eax], al
copy_screen_span ENDP
_TEXT ENDS
        END
