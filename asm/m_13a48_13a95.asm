.386
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
        PUBLIC saved_ds
saved_ds  DB 0
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
L_13A49:
        lea ebp, [esp + 1Ch]
L_13A4D:
        mov esi, dword ptr [ebp + 8]
L_13A50:
        mov eax, dword ptr [ebp + 0Ch]
L_13A53:
        mov ebx, dword ptr [ebp + 10h]
L_13A56:
        mov ecx, dword ptr [ebp + 14h]
L_13A59:
        mov edx, 3C8h
L_13A5E:
        out dx, al
L_13A5F:
        inc edx
L_13A60:
        mov edi, ebx
L_13A62:
        add ebx, ebx
L_13A64:
        add edi, ebx
L_13A66:
        xchg edi, ecx
L_13A68:
        mov ebp, 3Fh
L_13A6D:
        lodsb
L_13A6E:
        sub eax, edi
L_13A70:
        or eax, eax
L_13A72:
        jge short L_13A7B
L_13A74:
        sub eax, eax
L_13A76:
        out dx, al
L_13A77:
        loop L_13A6D
L_13A79:
        jmp short L_13A84
L_13A7B:
        cmp eax, ebp
L_13A7D:
        jle short L_13A81
L_13A7F:
        mov eax, ebp
L_13A81:
        out dx, al
L_13A82:
        loop L_13A6D
L_13A84:
        popad
L_13A85:
        ret
L_13A86:
        add byte ptr [eax], al
        PUBLIC copy_ds_to_es
copy_ds_to_es LABEL NEAR
; The game uses the data selector as the destination selector for string operations.
L_13A88:
        mov [saved_ds], ds
L_13A8E:
        mov es, [saved_ds]
L_13A94:
        ret
write_dac_palette_entry ENDP
_TEXT ENDS
        END
