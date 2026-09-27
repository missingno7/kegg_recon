.386P
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
VGA_GC_LATCH_COPY_MODE     EQU 041h
VGA_GC_LATCH_COPY_COMMAND  EQU 4105h
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
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN vga_state:WORD
        PUBLIC cpu_type
        PUBLIC cpu_mode
        PUBLIC cpu_iopl
cpu_type  DD 0FFFFFFFFh
cpu_mode  DD 0FFFFFFFFh
cpu_iopl  DD 0FFFFFFFFh
_DATA ENDS
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
DGROUP GROUP _DATA
        ASSUME CS:_TEXT
        ASSUME DS:DGROUP
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC probe_cpu_environment_entry
        PUBLIC probe_cpu_environment
CPU_TYPE_80386             EQU 386h
CPU_TYPE_80486             EQU 486h
EFLAGS_AC_FLAG             EQU 40000h
EFLAGS_VM_FLAG             EQU 20000h
CR0_PROTECTED_MODE_BIT     EQU 1
CPU_MODE_REAL              EQU 0
CPU_MODE_PROTECTED         EQU 1
CPU_MODE_VIRTUAL_8086       EQU 2
EFLAGS_IOPL_SHIFT          EQU 0Ch
EFLAGS_IOPL_MASK           EQU 3
; Record the detected CPU generation, execution mode, and I/O privilege level.
probe_cpu_environment LABEL NEAR
probe_cpu_environment_entry PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        cli
        mov dword ptr [cpu_type], CPU_TYPE_80386
        pushfd
        mov ebp, esp
        and sp, -4
        pushfd
        cli
        pop eax
        mov ebx, eax
        xor eax, EFLAGS_AC_FLAG
        push eax
        popfd
        pushfd
        pop eax
        xor eax, ebx
        mov esp, ebp
        popfd
        test eax, EFLAGS_AC_FLAG
        je short cpu_is_386_or_earlier
        mov dword ptr [cpu_type], CPU_TYPE_80486
cpu_is_386_or_earlier:
        mov dword ptr [cpu_mode], CPU_MODE_REAL
        smsw ax
        test al, CR0_PROTECTED_MODE_BIT
        je short cpu_mode_detected
        mov dword ptr [cpu_mode], CPU_MODE_PROTECTED
        pushfd
        pop eax
        test eax, EFLAGS_VM_FLAG
        je short cpu_mode_detected
        mov dword ptr [cpu_mode], CPU_MODE_VIRTUAL_8086
cpu_mode_detected:
        pushfd
        pop eax
        shr eax, EFLAGS_IOPL_SHIFT
        and eax, EFLAGS_IOPL_MASK
        mov dword ptr [cpu_iopl], eax
        ; This historical probe enables interrupts on exit rather than restoring the entry IF bit.
        sti
        popad
        ret
        ORG $+3 ; original zero fill to the next aligned entry at 13824h
probe_cpu_environment_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC clear_video_bytes_entry
        PUBLIC clear_video_bytes
; Parameters: destination at [ebp+8], byte count at [ebp+0Ch].
; Clear the range, switching VGA write planes when its start is in video memory.
clear_video_bytes LABEL NEAR
clear_video_bytes_entry PROC NEAR
        push ebp
        lea ebp, [esp]
        push eax
        push ecx
        push edi
        mov edi, dword ptr [ebp + 8]
        sub eax, eax
        cmp edi, VGA_B000_MEMORY_START
        jge short clear_memory_range
        cmp edi, VGA_A000_MEMORY_START
        jl short clear_memory_range
        cmp byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        je short clear_video_plane_mask_ready
        mov byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        mov ax, VGA_SEQ_ALL_PLANES_COMMAND
        mov dx, VGA_SEQ_INDEX_PORT
        out dx, ax
clear_video_plane_mask_ready:
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        je short clear_memory_range
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
clear_memory_range:
        mov ecx, dword ptr [ebp + 0Ch]
        shr ecx, 2
        rep stosd
        mov ecx, dword ptr [ebp + 0Ch]
        and ecx, 3
        rep stosb
        pop edi
        pop ecx
        pop eax
        pop ebp
        ret
clear_video_bytes_entry ENDP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC mov_mem
        PUBLIC move_memory_bytes
; C callers use the layout-fitted mov_mem entry.
; Parameters: source at [ebp+8], destination at [ebp+0Ch], byte count at [ebp+10h].
; Move overlapping ranges safely; use VGA latch-copy mode for eligible video-to-video copies.
mov_mem LABEL NEAR
move_memory_bytes PROC NEAR
        push ebp
        lea ebp, [esp]
        push ecx
        push edx
        push esi
        push edi
        mov esi, dword ptr [ebp + 8]
        mov edi, dword ptr [ebp + 0Ch]
        mov ecx, dword ptr [ebp + 10h]
        cmp esi, edi
        jge short copy_backward_endpoints_ready
        std
        add esi, ecx
        add edi, ecx
        dec esi
        dec edi
copy_backward_endpoints_ready:
        cmp esi, VGA_B000_MEMORY_START
        jge short copy_source_outside_video_memory
        cmp esi, VGA_A000_MEMORY_START
        jl short copy_source_outside_video_memory
        cmp edi, VGA_B000_MEMORY_START
        jge short copy_destination_in_video_memory
        cmp edi, VGA_A000_MEMORY_START
        jl short copy_destination_in_video_memory
        cmp word ptr [vga_state], 1
        jne short video_to_video_copy_mode_ready
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_LATCH_COPY_MODE
        je short video_to_video_copy_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_LATCH_COPY_MODE
        mov ax, VGA_GC_LATCH_COPY_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
video_to_video_copy_mode_ready:
        jmp short copy_forward_tail
copy_source_outside_video_memory:
        cmp edi, VGA_B000_MEMORY_START
        jge short clear_video_copy_graphics_mode_ready
        cmp edi, VGA_A000_MEMORY_START
        jl short clear_video_copy_graphics_mode_ready
copy_destination_in_video_memory:
        cmp byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        je short clear_video_copy_plane_mask_ready
        mov byte ptr [vga_state+vga_seq_plane_mask], VGA_ALL_PLANES_MASK
        mov ax, VGA_SEQ_ALL_PLANES_COMMAND
        mov dx, VGA_SEQ_INDEX_PORT
        out dx, ax
clear_video_copy_plane_mask_ready:
        cmp byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        je short clear_video_copy_graphics_mode_ready
        mov byte ptr [vga_state+vga_gc_mode], VGA_GC_PLANAR_MODE_VALUE
        mov ax, VGA_GC_PLANAR_MODE_COMMAND
        mov dx, VGA_GC_INDEX_PORT
        out dx, ax
clear_video_copy_graphics_mode_ready:
        shr ecx, 2
        rep movsd
        mov ecx, dword ptr [ebp + 10h]
        and ecx, 3
copy_forward_tail:
        rep movsb
        cld
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebp
        ret
        ORG $+1 ; original zero fill to the next even code address
move_memory_bytes ENDP
_TEXT ENDS
END
