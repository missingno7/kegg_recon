.386
; VGA indexed register ports and their data aliases (the helpers increment DX).
VGA_ATTR_INDEX_PORT       EQU 03C0h
VGA_ATTR_DATA_PORT        EQU 03C1h
VGA_CRTC_INDEX_PORT       EQU 03D4h
VGA_CRTC_DATA_PORT        EQU 03D5h
VGA_SEQ_INDEX_PORT        EQU 03C4h
VGA_SEQ_DATA_PORT         EQU 03C5h
VGA_GC_INDEX_PORT         EQU 03CEh
VGA_GC_DATA_PORT          EQU 03CFh
VGA_SEQ_PLANE_MASK_INDEX  EQU 02h
VGA_GC_READ_PLANE_INDEX   EQU 04h
VGA_GC_MODE_INDEX         EQU 05h
VGA_ALL_PLANES_MASK       EQU 0Fh
VGA_READ_PLANE_MASK       EQU 03h
VGA_PLANE_ROTATION_SEED   EQU 011h
VGA_PLANE_ROTATION_COMMAND EQU 1102h
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
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN vga_state:WORD
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC update_attr_register_entry
        PUBLIC update_attr_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write an attribute-controller register.
update_attr_register LABEL NEAR
update_attr_register_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push eax
        push edx
        mov dx, VGA_ATTR_INDEX_PORT
        mov al, byte ptr [ebp + 8]
        or al, 20h
        out dx, al
        jmp short attr_index_latch_delay
attr_index_latch_delay:
        jmp short attr_register_read
attr_register_read:
        in al, dx
        and al, byte ptr [ebp + 0Ch]
        or al, byte ptr [ebp + 10h]
        out dx, al
        pop edx
        pop eax
        pop ebp
        ret
update_attr_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC update_crtc_register_entry
        PUBLIC update_crtc_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write a CRT-controller register.
update_crtc_register LABEL NEAR
update_crtc_register_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push eax
        push edx
        mov dx, VGA_CRTC_INDEX_PORT
        mov al, byte ptr [ebp + 8]
        out dx, al
        jmp short crtc_index_latch_delay
crtc_index_latch_delay:
        jmp short crtc_data_port_select
crtc_data_port_select:
        inc dx
        in al, dx
        and al, byte ptr [ebp + 0Ch]
        or al, byte ptr [ebp + 10h]
        out dx, al
        pop edx
        pop eax
        pop ebp
        ret
update_crtc_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC update_seq_register_entry
        PUBLIC update_seq_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write a VGA sequencer register.
update_seq_register LABEL NEAR
update_seq_register_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push eax
        push edx
        mov dx, VGA_SEQ_INDEX_PORT
        mov al, byte ptr [ebp + 8]
        out dx, al
        jmp short seq_index_latch_delay
seq_index_latch_delay:
        jmp short seq_data_port_select
seq_data_port_select:
        inc dx
        in al, dx
        and al, byte ptr [ebp + 0Ch]
        or al, byte ptr [ebp + 10h]
        out dx, al
        pop edx
        pop eax
        pop ebp
        ret
update_seq_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC update_gc_register_entry
        PUBLIC update_gc_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write a VGA graphics-controller register.
update_gc_register LABEL NEAR
update_gc_register_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push eax
        push edx
        mov dx, VGA_GC_INDEX_PORT
        mov al, byte ptr [ebp + 8]
        out dx, al
        jmp short gc_index_latch_delay
gc_index_latch_delay:
        jmp short gc_data_port_select
gc_data_port_select:
        inc dx
        in al, dx
        and al, byte ptr [ebp + 0Ch]
        or al, byte ptr [ebp + 10h]
        out dx, al
        pop edx
        pop eax
        pop ebp
        ret
update_gc_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC set_seq_plane_mask_entry
        PUBLIC set_seq_plane_mask
; Set the sequencer's four-bit plane write mask and update its cached value.
set_seq_plane_mask LABEL NEAR
set_seq_plane_mask_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push edx
        mov al, VGA_SEQ_PLANE_MASK_INDEX
        mov ah, byte ptr [ebp + 8]
        and ah, VGA_ALL_PLANES_MASK
        mov byte ptr [vga_state+vga_seq_plane_mask], ah
        mov dx, VGA_SEQ_INDEX_PORT
        out dx, ax
        pop edx
        pop ebp
        shr eax, 8
        ret
set_seq_plane_mask_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC rotate_seq_plane_mask_entry
        PUBLIC rotate_seq_plane_mask
; Rotate 0x11 by the requested amount and use the low nibble as the plane mask.
rotate_seq_plane_mask LABEL NEAR
rotate_seq_plane_mask_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push edx
        mov ax, VGA_PLANE_ROTATION_COMMAND
        mov edx, ecx
        mov cl, byte ptr [ebp + 8]
        rol ah, cl
        mov ecx, edx
        and ah, VGA_ALL_PLANES_MASK
        mov byte ptr [vga_state+vga_seq_plane_mask], ah
        mov dx, VGA_SEQ_INDEX_PORT
        out dx, ax
        pop edx
        pop ebp
        shr eax, 8
        ret
rotate_seq_plane_mask_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC set_gc_read_map_entry
        PUBLIC set_gc_read_map
; Select the VGA plane used by graphics-controller reads.
set_gc_read_map LABEL NEAR
set_gc_read_map_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push edx
        mov al, VGA_GC_READ_PLANE_INDEX
        mov ah, byte ptr [ebp + 8]
        and ah, VGA_READ_PLANE_MASK
        mov byte ptr [vga_state+vga_gc_read_plane], ah
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
        pop edx
        pop ebp
        shr eax, 8
        ret
set_gc_read_map_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC set_gc_mode_entry
        PUBLIC set_gc_mode
; Set the cached VGA graphics-controller mode register.
set_gc_mode LABEL NEAR
set_gc_mode_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push edx
        mov al, VGA_GC_MODE_INDEX
        mov ah, byte ptr [ebp + 8]
        mov byte ptr [vga_state+vga_gc_mode], ah
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
        pop edx
        pop ebp
        shr eax, 8
        ret
        ORG $+3 ; original zero fill to the next aligned entry at 13A48h
set_gc_mode_entry ENDP
_TEXT ENDS
        END
