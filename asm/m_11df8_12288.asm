.386
DGROUP GROUP _DATA
; GIF87a and its 256-entry global table feed 8-bit pixels and six-bit VGA DAC values.
gif87a_signature_dword EQU 38464947h
gif87a_version_word EQU 6137h
gif_image_separator_byte EQU 2Ch
gif_error_signature_dword EQU 901h
gif_error_version_word EQU 902h
gif_error_color_table_size EQU 905h
gif_error_image_separator EQU 906h
gif_error_image_width EQU 903h
gif_error_image_height EQU 904h
gif_error_local_color_table EQU 907h
gif_error_image_data EQU 908h
gif_max_image_width EQU 140h
gif_max_image_height EQU 0C8h
gif_required_color_index_bits EQU 8
gif_descriptor_color_resolution_shift EQU 4
gif_descriptor_low_three_bits_mask EQU 7
gif_image_interlace_flag_bit EQU 6
gif_odd_output_length_mask EQU 1
gif_rgb_components_per_entry EQU 3
gif_bits_per_input_byte EQU 8
gif_lzw_max_code_width EQU 0Ch
gif_lzw_code_width_after_clear EQU 9
gif_lzw_code_limit_after_clear EQU 200h
gif_lzw_first_code_after_clear EQU 102h
gif_lzw_mask_after_clear EQU 1FFh
gif_palette_entry_count EQU 100h
gif_rgb_to_vga_dac_shift EQU 2
gif_image_state_decoded_bytes_offset EQU 10h
gif_lzw_stack_to_prefix_table_offset EQU 1400h
gif_prefix_to_suffix_table_offset EQU 2800h

_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN gif_decoded_image_state:BYTE
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
; Validate GIF87a, compact size-prefixed image-data blocks, expand LZW pixels, and append a VGA DAC palette.
        PUBLIC decode_gif_image_entry
        PUBLIC decode_gif_image
decode_gif_image LABEL NEAR
decode_gif_image_entry:
        pushad
        lea ebp, [esp + 1Ch]
        mov eax, dword ptr [ebp + 14h]
        mov dword ptr [gif_lzw_expansion_stack], eax
        lea eax, [eax + gif_lzw_stack_to_prefix_table_offset]
        mov dword ptr [gif_lzw_prefix_table], eax
        lea eax, [eax + gif_prefix_to_suffix_table_offset]
        mov dword ptr [gif_lzw_suffix_table], eax
        lea eax, [eax + gif_prefix_to_suffix_table_offset]
        mov word ptr [gif_lzw_code_width], gif_lzw_max_code_width
        mov ebx, dword ptr [ebp + 0Ch]
        mov dword ptr [gif_pixel_output_start], ebx
        mov esi, dword ptr [ebp + 8]
        mov word ptr [gif_reserved_state], 0
        lodsd
        mov word ptr [gif_decode_error_code], gif_error_signature_dword
        cmp eax, gif87a_signature_dword
        jne near ptr finish_gif_decode
        lodsw
        mov word ptr [gif_decode_error_code], gif_error_version_word
        cmp ax, gif87a_version_word
        jne near ptr finish_gif_decode
        lodsw
        mov word ptr [gif_image_width], ax
        lodsw
        mov word ptr [gif_image_height], ax
        lodsb
        or al, al
        sets byte ptr [gif_has_global_color_table]
        mov bl, al
        xor bh, bh
; Packed-screen fields carry color resolution and global palette index width.
        shr bl,gif_descriptor_color_resolution_shift
        and bl,gif_descriptor_low_three_bits_mask
        inc bl
        mov byte ptr [gif_color_resolution_bits], bl
        mov bl, al
        and bl,gif_descriptor_low_three_bits_mask
        inc bl
        mov word ptr [gif_decode_error_code], gif_error_color_table_size
        cmp bx, gif_required_color_index_bits
        jne near ptr finish_gif_decode
        mov word ptr [gif_color_index_bits], bx
        lodsb
        lodsb
        cmp byte ptr [gif_has_global_color_table], 0
        je short after_global_color_table
        mov bx, 1
        mov cx, word ptr [gif_color_index_bits]
        shl bx, cl
        movzx ecx, bx
        mov eax, ecx
        shl ecx, 1
        add ecx, eax
        mov dword ptr [gif_global_color_table_bytes], ecx
        mov dword ptr [gif_global_color_table_source], esi
        add esi, ecx
after_global_color_table:
        lodsb
        mov word ptr [gif_decode_error_code], gif_error_image_separator
        cmp al, gif_image_separator_byte
        jne near ptr finish_gif_decode
        lodsw
        lodsw
        lodsw
        cmp word ptr [gif_image_width], 0
        jne short validate_gif_image_width
        mov word ptr [gif_image_width], ax
validate_gif_image_width:
        mov word ptr [gif_decode_error_code], gif_error_image_width
        cmp ax, gif_max_image_width
        lodsw
        cmp word ptr [gif_image_height], 0
        jne short validate_gif_image_height
        mov word ptr [gif_image_height], ax
validate_gif_image_height:
        mov word ptr [gif_decode_error_code], gif_error_image_height
        cmp ax, gif_max_image_height
        lodsw
        mov word ptr [gif_decode_error_code], gif_error_local_color_table
        or al, al
        js near ptr finish_gif_decode
        mov word ptr [gif_decode_error_code], gif_error_image_data
        bt ax,gif_image_interlace_flag_bit
        push edi
        push esi
        mov edi, esi
        xor eax, eax
        mov ecx, eax
        lodsb
        or al, al
        je short gif_data_subblocks_compacted
; Remove each data-sub-block length in place to form contiguous LZW input.
compact_next_gif_data_subblock:
        mov cl, al
        shr cl, 1
        jae short compact_subblock_word_tail
        movsb
compact_subblock_word_tail:
        shr cl, 1
        jae short compact_subblock_dword_tail
        movsw
compact_subblock_dword_tail:
        rep movsd
        lodsb
        or al, al
        jne short compact_next_gif_data_subblock
gif_data_subblocks_compacted:
        xor eax, eax
        mov ebx, eax
        pop esi
        pop edi
        mov al, byte ptr [esi - 1]
        mov ecx, eax
        inc eax
        mov word ptr [gif_lzw_code_width], ax
        mov word ptr [gif_lzw_next_code_width], ax
        mov eax, 1
        shl ax, cl
        mov word ptr [gif_lzw_clear_code], ax
        inc ax
        mov word ptr [gif_lzw_end_code], ax
        inc ax
        mov word ptr [gif_lzw_first_available_code], ax
        mov word ptr [gif_lzw_next_code], ax
        mov eax, 1
        mov cx, word ptr [gif_lzw_code_width]
        shl ax, cl
        mov word ptr [gif_lzw_code_limit], ax
        mov word ptr [gif_lzw_code_limit_shadow], ax
        dec ax
        mov word ptr [gif_lzw_code_mask], ax
        mov eax, 1
        mov cx, word ptr [gif_color_index_bits]
        shl ax, cl
        dec ax
        mov word ptr [gif_color_index_mask], ax
        mov dword ptr [gif_lzw_bit_buffer], 0
        mov word ptr [gif_lzw_bits_buffered], 0
        mov dword ptr [gif_compressed_data_cursor], esi
        mov ebx, dword ptr [gif_lzw_expansion_stack]
        mov edi, dword ptr [gif_pixel_output_start]
        mov word ptr [gif_decode_error_code], 0
        push ebp
decode_next_lzw_code:
        mov edx, dword ptr [gif_lzw_bit_buffer]
        mov cx, word ptr [gif_lzw_bits_buffered]
        mov esi, dword ptr [gif_compressed_data_cursor]
        cmp cx, word ptr [gif_lzw_code_width]
        jge short extract_lzw_code_from_bit_buffer
        xor eax, eax
        lodsb
        shl eax, cl
        add edx, eax
        add cx, gif_bits_per_input_byte
        cmp cx, word ptr [gif_lzw_code_width]
        jge short save_refilled_lzw_input_cursor
        xor eax, eax
        lodsb
        shl eax, cl
        add edx, eax
        add cx, gif_bits_per_input_byte
save_refilled_lzw_input_cursor:
        mov dword ptr [gif_compressed_data_cursor], esi
extract_lzw_code_from_bit_buffer:
        mov eax, edx
        and dx, word ptr [gif_lzw_code_mask]
        sub cx, word ptr [gif_lzw_code_width]
        mov word ptr [gif_lzw_bits_buffered], cx
        mov cx, word ptr [gif_lzw_code_width]
        shr eax, cl
        mov dword ptr [gif_lzw_bit_buffer], eax
        mov eax, edx
        cmp ax, word ptr [gif_lzw_end_code]
        je near ptr lzw_end_code_reached
        cmp ax, word ptr [gif_lzw_clear_code]
        jne near ptr decode_non_clear_lzw_code
        mov word ptr [gif_lzw_code_width], gif_lzw_code_width_after_clear
        mov word ptr [gif_lzw_code_limit], gif_lzw_code_limit_after_clear
        mov word ptr [gif_lzw_next_code], gif_lzw_first_code_after_clear
        mov word ptr [gif_lzw_code_mask], gif_lzw_mask_after_clear
        mov edx, dword ptr [gif_lzw_bit_buffer]
        mov cx, word ptr [gif_lzw_bits_buffered]
        cmp cx, word ptr [gif_lzw_code_width]
        jge short extract_code_after_lzw_clear
        xor eax, eax
        lodsb
        shl eax, cl
        add edx, eax
        add cx, gif_bits_per_input_byte
        cmp cx, word ptr [gif_lzw_code_width]
        jge short save_clear_code_refill_cursor
        xor eax, eax
        lodsb
        shl eax, cl
        add edx, eax
        add cx, gif_bits_per_input_byte
save_clear_code_refill_cursor:
        mov dword ptr [gif_compressed_data_cursor], esi
extract_code_after_lzw_clear:
        mov eax, edx
        and dx, word ptr [gif_lzw_code_mask]
        sub cx, word ptr [gif_lzw_code_width]
        mov word ptr [gif_lzw_bits_buffered], cx
        mov cx, word ptr [gif_lzw_code_width]
        shr eax, cl
        mov dword ptr [gif_lzw_bit_buffer], eax
        mov word ptr [gif_lzw_saved_codes], dx
        mov byte ptr [gif_previous_literal], dl
        mov al, dl
        stosb
        jmp near ptr decode_next_lzw_code
decode_non_clear_lzw_code:
        mov word ptr [gif_lzw_current_code], ax
        cmp ax, word ptr [gif_lzw_next_code]
        jl short expand_lzw_code_string
        mov ax, word ptr [gif_lzw_saved_codes]
        mov dl, byte ptr [gif_previous_literal]
        mov byte ptr [ebx], dl
        inc ebx
expand_lzw_code_string:
        movzx ebp, word ptr [gif_color_index_mask]
        cmp ax, bp
        jle short lzw_prefix_chain_done
        movzx eax, ax
        push ecx
; Prefix entries are words; suffix entries are bytes.
follow_lzw_prefix_chain:
        mov ecx, dword ptr [gif_lzw_suffix_table]
        mov dl, byte ptr [eax + ecx]
        mov byte ptr [ebx], dl
        inc ebx
        mov ecx, dword ptr [gif_lzw_prefix_table]
        mov ax, word ptr [ecx + eax*2]
        cmp ax, bp
        jg short follow_lzw_prefix_chain
        pop ecx
lzw_prefix_chain_done:
        and ax, bp
        mov byte ptr [gif_previous_literal], al
        stosb
        mov ebp, dword ptr [gif_lzw_expansion_stack]
        cmp ebx, ebp
        je short add_lzw_dictionary_entry
copy_lzw_string_in_reverse:
        dec ebx
        mov al, byte ptr [ebx]
        stosb
        cmp ebx, ebp
        jne short copy_lzw_string_in_reverse
        mov al, byte ptr [gif_previous_literal]
add_lzw_dictionary_entry:
        movzx edx, word ptr [gif_lzw_next_code]
        push ecx
        mov ecx, dword ptr [gif_lzw_suffix_table]
        mov byte ptr [edx + ecx], al
        mov ax, word ptr [gif_lzw_saved_codes]
        mov ecx, dword ptr [gif_lzw_prefix_table]
        mov word ptr [ecx + edx*2], ax
        pop ecx
        add edx, edx
        mov ax, word ptr [gif_lzw_current_code]
        mov word ptr [gif_lzw_saved_codes], ax
        inc word ptr [gif_lzw_next_code]
        mov ax, word ptr [gif_lzw_next_code]
        cmp ax, word ptr [gif_lzw_code_limit]
        jne near ptr decode_next_lzw_code
        cmp word ptr [gif_lzw_code_width], 0Ch
        je near ptr decode_next_lzw_code
        inc word ptr [gif_lzw_code_width]
        mov ax, word ptr [gif_lzw_code_limit]
        add ax, ax
        mov word ptr [gif_lzw_code_limit], ax
        dec ax
        mov word ptr [gif_lzw_code_mask], ax
        jmp near ptr decode_next_lzw_code
lzw_end_code_reached:
        pop ebp
finish_gif_decode:
        mov eax, edi
        sub eax, dword ptr [ebp + 0Ch]
        mov dword ptr [gif_decoded_image_state+gif_image_state_decoded_bytes_offset], eax
        and eax,gif_odd_output_length_mask
        sub edi, eax
        sub dword ptr [gif_decoded_image_state+gif_image_state_decoded_bytes_offset], eax
        mov ecx, dword ptr [gif_global_color_table_bytes]
        add dword ptr [gif_decoded_image_state+gif_image_state_decoded_bytes_offset], ecx
        mov esi, dword ptr [gif_global_color_table_source]
        mov ebx, dword ptr [ebp + 10h]
        mov eax, dword ptr [ebp + 0Ch]
        mov dword ptr [ebx], eax
        mov eax, edi
        mov dword ptr [ebx + 4], eax
        movzx eax, word ptr [gif_image_width]
        mov dword ptr [ebx + 8], eax
        movzx eax, word ptr [gif_image_height]
        mov dword ptr [ebx + 0Ch], eax
        mov dword ptr [ebx + 14h], gif_palette_entry_count
        mov dword ptr [ebx + 10h], gif_rgb_components_per_entry
; Convert each global RGB component from 8-bit to VGA's six-bit DAC range.
convert_global_palette_to_vga_dac:
        lodsb
        shr al, gif_rgb_to_vga_dac_shift
        stosb
        loop convert_global_palette_to_vga_dac
        popad
        movzx eax, word ptr [gif_decode_error_code]
        ret
_TEXT ENDS
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
; This word is reset by the decoder; other uses are not established.
        PUBLIC gif_reserved_state
gif_reserved_state	DW 0
        PUBLIC gif_lzw_current_code
gif_lzw_current_code	DW 0
        PUBLIC gif_image_width
gif_image_width	DW 0
        PUBLIC gif_image_height
gif_image_height	DW 0
        PUBLIC gif_has_global_color_table
gif_has_global_color_table	DB 0
        PUBLIC gif_color_resolution_bits
gif_color_resolution_bits	DB 0
        PUBLIC gif_color_index_bits
gif_color_index_bits	DW 0
        PUBLIC gif_lzw_expansion_stack
gif_lzw_expansion_stack LABEL DWORD
        DB 1h, 0h, 0h, 0h
        PUBLIC gif_lzw_prefix_table
gif_lzw_prefix_table LABEL DWORD
        DB 1h, 0h, 0h, 0h
        PUBLIC gif_lzw_suffix_table
gif_lzw_suffix_table LABEL DWORD
        DB 1h, 0h, 0h, 0h
        PUBLIC gif_lzw_code_width
gif_lzw_code_width	DW 0
        PUBLIC gif_lzw_next_code_width
gif_lzw_next_code_width	DW 0
        PUBLIC gif_lzw_first_available_code
gif_lzw_first_available_code	DW 0
        PUBLIC gif_lzw_next_code
gif_lzw_next_code	DW 0
        PUBLIC gif_lzw_code_limit
gif_lzw_code_limit	DW 0
        PUBLIC gif_lzw_code_limit_shadow
gif_lzw_code_limit_shadow	DW 0
        PUBLIC gif_lzw_code_mask
gif_lzw_code_mask	DW 0
        PUBLIC gif_color_index_mask
gif_color_index_mask	DW 0
        PUBLIC gif_previous_literal
gif_previous_literal	DB 0
        PUBLIC gif_lzw_bit_buffer
gif_lzw_bit_buffer	DD 0
        PUBLIC gif_lzw_bits_buffered
gif_lzw_bits_buffered	DW 0
        PUBLIC gif_lzw_saved_codes
gif_lzw_saved_codes	DW 3 DUP (0)
        PUBLIC gif_lzw_clear_code
gif_lzw_clear_code	DW 0
        PUBLIC gif_lzw_end_code
gif_lzw_end_code	DW 0
        PUBLIC gif_pixel_output_start
gif_pixel_output_start	DD 0
        PUBLIC gif_compressed_data_cursor
gif_compressed_data_cursor	DD 0
        PUBLIC gif_decode_error_code
gif_decode_error_code	DW 0
        PUBLIC gif_global_color_table_bytes
gif_global_color_table_bytes	DD 0
        PUBLIC gif_global_color_table_source
gif_global_color_table_source LABEL DWORD
        DB 0h, 0h, 0h, 0h, 0h
_DATA ENDS
        END
