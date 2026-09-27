.386
; VGA DAC write-index and component-data ports.
VGA_DAC_WRITE_INDEX_PORT  EQU 03C8h
VGA_DAC_COMPONENT_PORT    EQU 03C9h
VGA_DAC_COMPONENT_MAX     EQU 03Fh
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
        PUBLIC saved_ds
saved_ds  DD 0
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ASSUME CS:_TEXT
        PUBLIC write_dac_palette_entry
        PUBLIC write_dac_palette
; Parameters: RGB source, starting DAC index, color count, and brightness adjustment.
; Apply the adjustment to each component, clamp to 0..63, and write the VGA DAC.
write_dac_palette LABEL NEAR
write_dac_palette_entry PROC NEAR
        pushad
        lea ebp, [esp + 1Ch]
        mov esi, dword ptr [ebp + 8]
        mov eax, dword ptr [ebp + 0Ch]
        mov ebx, dword ptr [ebp + 10h]
        mov ecx, dword ptr [ebp + 14h]
        mov edx, VGA_DAC_WRITE_INDEX_PORT
        out dx, al
        inc edx
        mov edi, ebx
        add ebx, ebx
        add edi, ebx
        xchg edi, ecx
        mov ebp, VGA_DAC_COMPONENT_MAX
palette_next_component:
        lodsb
        sub eax, edi
        or eax, eax
        jge short palette_component_nonnegative
        sub eax, eax
        out dx, al
        loop palette_next_component
        jmp short palette_next_color_or_return
palette_component_nonnegative:
        cmp eax, ebp
        jle short palette_component_within_range
        mov eax, ebp
palette_component_within_range:
        out dx, al
        loop palette_next_component
palette_next_color_or_return:
        popad
        ret
        add byte ptr [eax], al
        PUBLIC copy_ds_to_es
copy_ds_to_es LABEL NEAR
; The game uses the data selector as the destination selector for string operations.
        mov [saved_ds], ds
        mov es, [saved_ds]
        ret
write_dac_palette_entry ENDP
_TEXT ENDS
        END
