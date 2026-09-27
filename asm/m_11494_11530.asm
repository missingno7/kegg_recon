.386P
DGROUP GROUP _DATA
; 8237 DMA controller registers and channel programming values.
dma_single_mask_register_port EQU 0Ah
dma_mode_register_port EQU 0Bh
dma_clear_byte_pointer_port EQU 0Ch
dma_channel_mask_bit EQU 4
; Packed lookup bytes for channels 0..3: 87h, 83h, 81h, 82h.
dma_page_port_lookup_word EQU 82818387h
dma_count_sample_tolerance EQU 10h
dma_page_port_lookup_shift EQU 3
dma_page_address_word_shift EQU 10h

_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
        PUBLIC sound_dma_buffer_address
sound_dma_buffer_address	DD 0
        PUBLIC sound_dma_transfer_count
sound_dma_transfer_count	DW 0
        PUBLIC sound_dma_mode_bits
sound_dma_mode_bits	DB 0
        PUBLIC sound_dma_channel
sound_dma_channel	DB 0
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC sound_dma_mask_entry
; Mask or program the selected 8237 DMA channel.
        PUBLIC mask_sound_dma_channel
mask_sound_dma_channel LABEL NEAR
sound_dma_mask_entry PROC NEAR
        push eax
        mov al, byte ptr [sound_dma_channel]
        or al, dma_channel_mask_bit
        out dma_single_mask_register_port, al
        pop eax
        ret
        PUBLIC program_sound_dma_channel
program_sound_dma_channel LABEL NEAR
        push eax
        push ecx
        push edx
        mov cl, byte ptr [sound_dma_channel]
        mov al, cl
        or al, dma_channel_mask_bit
        out dma_single_mask_register_port, al
        out dma_clear_byte_pointer_port, al
        mov al, cl
        or al, byte ptr [sound_dma_mode_bits]
        out dma_mode_register_port, al
        movzx dx, cl
        add dx, dx
        mov eax, dword ptr [sound_dma_buffer_address]
        out dx, al
        mov al, ah
        out dx, al
        inc dx
        mov ax, word ptr [sound_dma_transfer_count]
        dec ax
        out dx, al
        mov al, ah
        out dx, al
        mov edx, dma_page_port_lookup_word
        shl cl, 3
        shr edx, cl
        xor dh, dh
        shr cl, 3
        shr eax, 10h
        out dx, al
        mov al, cl
        out dma_single_mask_register_port, al
        pop edx
        pop ecx
        pop eax
        ret
        push ecx
        push edx
        movzx dx, byte ptr [sound_dma_channel]
        add dx, dx
        inc dx
        in al, dx
        mov ah, al
        in al, dx
        xchg al, ah
; Reject torn 8237 count reads until two samples differ by at most 16.
retry_unstable_dma_count_sample:
        mov cx, ax
        in al, dx
        mov ah, al
        in al, dx
        xchg al, ah
        sub cx, ax
        cmp cx, dma_count_sample_tolerance
        jg short retry_unstable_dma_count_sample
        cmp cx, -dma_count_sample_tolerance
        jl short retry_unstable_dma_count_sample
        neg ax
        add ax, word ptr [sound_dma_transfer_count]
        dec ax
        pop edx
        pop ecx
        ret
sound_dma_mask_entry ENDP
_TEXT ENDS
        END
