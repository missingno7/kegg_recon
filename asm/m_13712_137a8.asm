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
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN disp_idx:WORD
EXTRN vga_state:WORD
EXTRN draw_idx:WORD
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT
        PUBLIC fill_planar_video_rows
; Fill successive VGA scan-line spans with one repeated pixel value.
fill_planar_video_rows:
        cmp byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        je short planar_rows_plane_mask_ready
        mov byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        mov ax, VGA_SEQ_ALL_PLANES_COMMAND
        mov dx, VGA_SEQ_INDEX_PORT
        out dx, ax
planar_rows_plane_mask_ready:
        cmp byte ptr [vga_state+vga_gc_mode], 40h
        je short planar_rows_graphics_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], 40h
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
planar_rows_graphics_mode_ready:
        mov ecx, dword ptr [vga_state+vga_screen_stride]
        mov eax, dword ptr [ebp + 10h]
        mul ecx
        mov edi, eax
        add edi, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 8]
        shl ebx, 2
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group0]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group1]
        add edi, dword ptr [ebx + vga_state+vga_page_origin_group2]
        mov edx, dword ptr [ebp - 10h]
        mov ebx, edx
        shr edx, 2
        mov dword ptr [ebp - 8], edx
        and ebx, 3
        mov dword ptr [ebp - 0Ch], ebx
        sub ecx, dword ptr [ebp - 10h]
        mov esi, ecx
        mov ebx, dword ptr [ebp - 14h]
        mov edx, dword ptr [ebp + 1Ch]
        mov dh, dl
        mov eax, edx
        rol eax, 10h
        mov ax, dx
planar_rows_next_scanline:
        mov ecx, dword ptr [ebp - 8]
        rep stosd
        mov ecx, dword ptr [ebp - 0Ch]
        rep stosb
        add edi, esi
        dec ebx
        jne short planar_rows_next_scanline
        popad
        mov esp, ebp
        pop ebp
        ret
_TEXT ENDS
END
