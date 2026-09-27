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
        PUBLIC copy_chunky_scanline_to_vga
        ; Split chunky scanline pixels across the four VGA planes.
        ; Args at [EBP+8..10h]: chunky source, VGA destination, scanline byte count.
        PUBLIC copy_chunky_scanline_to_vga_entry
copy_chunky_scanline_to_vga_entry LABEL NEAR
copy_chunky_scanline_to_vga PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        cmp byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        je short copy_scanline_select_map_mask
        mov byte ptr [vga_state+VGA_STATE_CACHED_GC_MODE], VGA_CACHED_GC_PLANAR_WRITE_MODE_0
        mov ax, VGA_GC_PLANAR_WRITE_MODE_0
        mov dx, VGA_GC_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
copy_scanline_select_map_mask:
        mov ah, VGA_MAP_MASK_PLANE_0
copy_scanline_next_plane:
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], ah
        mov al, VGA_SEQ_MAP_MASK_REGISTER
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; VGA Graphics Controller write mode 0.
        mov esi, dword ptr [ebp + 8]
        mov edi, dword ptr [ebp + 0Ch]
        mov ecx, dword ptr [ebp + 10h]
        shr ecx, 2
        mov edx, 3
copy_scanline_copy_plane_byte:
        movsb
        add esi, edx
        loop copy_scanline_copy_plane_byte
        inc dword ptr [ebp + 8]
        add ah, ah
        cmp ah, 10h
        jne short copy_scanline_next_plane
        cmp byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        je short copy_scanline_restore_all_planes
        mov byte ptr [vga_state+VGA_STATE_CACHED_SEQ_MASK], VGA_CACHED_SEQ_MAP_MASK_ALL_PLANES
        mov ax, VGA_SEQ_MAP_MASK_ALL_PLANES
        mov dx, VGA_SEQ_INDEX_DATA_PORT
        out dx, ax ; Enable all four VGA sequencer planes.
copy_scanline_restore_all_planes:
        popad
        ret
        ORG $+1 ; original zero fill to the next even code address
copy_chunky_scanline_to_vga ENDP
_TEXT ENDS
        END
