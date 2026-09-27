.386
DGROUP GROUP _DATA
; Sound Blaster DSP register offsets, status and command values.
sb_dsp_reset_port_offset EQU 6
sb_dsp_write_status_offset EQU 0Ch
sb_dsp_read_status_irq_ack_offset EQU 0Eh
sb_dsp_read_data_offset EQU 0Ah
sb_dsp_status_bit_7 EQU 80h
sb_dsp_io_poll_count EQU 3E8h
sb_dsp_reset_delay_count EQU 0FFh
sb_dsp_reset_enable EQU 1
sb_dsp_reset_disable EQU 0
sb_dsp_reset_ack_byte EQU 0AAh
sb_dsp_sample_rate_divisor EQU 0F42h
sb_dsp_rate_rounding_bias EQU 7Fh
sb_dsp_time_constant_base EQU 100h
sb_dsp_set_time_constant_command EQU 40h
sb_dsp_start_dma_command EQU 14h
sb_dsp_dma_length_low EQU 7Fh
sb_dsp_dma_length_high EQU 2
sb_dsp_halt_8bit_dma_command EQU 0D0h
sb_dsp_speaker_on_command EQU 0D1h
sb_dsp_speaker_off_command EQU 0D3h
pic_master_command_port EQU 20h
pic_slave_command_port EQU 0A0h
pic_end_of_interrupt_command EQU 20h
pic_slave_irq_base EQU 8
sound_dma_mode_single_transfer EQU 48h
sound_dma_mode_single_auto_init_transfer EQU 58h

_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN audio_stream_flag:WORD
EXTRN active_audio_rate:DWORD
EXTRN audio_dma_half_bytes:DWORD
EXTRN last_audio_sample_rate:DWORD
EXTRN sound_blaster_irq:BYTE
EXTRN sound_blaster_dma_channel:BYTE
EXTRN sound_dma_mode_bits:BYTE
EXTRN sound_dma_channel:BYTE
EXTRN sound_blaster_base_port:WORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
EXTRN mask_sound_dma_channel:NEAR
EXTRN program_sound_dma_channel:NEAR
EXTRN transfer_audio_stream_block:NEAR
        ASSUME CS:_TEXT, DS:DGROUP
; Acknowledge the DSP IRQ, start a 640-byte block when audio is active, then call the stream handler.
; The DSP status loops are bounded; the IRQ is enabled before the game callback.
        PUBLIC sound_blaster_irq_handler
sound_blaster_irq_handler LABEL NEAR
sound_blaster_irq_entry PROC NEAR
        push eax
        push ecx
        push edx
        mov dx, ds
        rol edx, 10h
        mov ax, SEG DGROUP
        mov ds, eax
        mov dx, word ptr [sound_blaster_base_port]
        add dx, sb_dsp_read_status_irq_ack_offset
        in al, dx
        cmp dword ptr [audio_dma_half_bytes], 0
        je short no_audio_dma_block_pending
        mov eax, dword ptr [active_audio_rate]
        cmp dword ptr [last_audio_sample_rate], eax
        jne short sample_rate_changed
wait_for_dsp_write_before_block_command:
        add dx, sb_dsp_write_status_offset-sb_dsp_read_status_irq_ack_offset
        mov ecx, sb_dsp_io_poll_count
wait_for_dsp_write_before_block_length_low:
        in al, dx
        test al, sb_dsp_status_bit_7
        loopne wait_for_dsp_write_before_block_length_low
        mov al, sb_dsp_start_dma_command
        out dx, al
        mov ecx, sb_dsp_io_poll_count
wait_for_dsp_write_before_block_length_high:
        in al, dx
        test al, sb_dsp_status_bit_7
        loopne wait_for_dsp_write_before_block_length_high
        mov al,sb_dsp_dma_length_low
        out dx, al
        mov ecx, sb_dsp_io_poll_count
wait_for_dsp_write_before_block_length_upper:
        in al, dx
        test al, sb_dsp_status_bit_7
        loopne wait_for_dsp_write_before_block_length_upper
        mov al,sb_dsp_dma_length_high
        out dx, al
        mov word ptr [audio_stream_flag], 0FFFFh
        jmp short restore_irq_data_segments
sample_rate_changed:
        mov dword ptr [last_audio_sample_rate], eax
        push eax
        call set_sound_blaster_sample_rate
        add esp, 4
        jmp short wait_for_dsp_write_before_block_command
no_audio_dma_block_pending:
        mov word ptr [audio_stream_flag], 0
restore_irq_data_segments:
        mov dx, es
        cld
        mov ax, SEG DGROUP
        mov es, eax
        pushad
        sti
        call send_pic_end_of_interrupt
        call transfer_audio_stream_block
        popad
        mov es, edx
        rol edx, 10h
        mov ds, edx
        pop edx
        pop ecx
        pop eax
        iretd
sound_blaster_irq_entry ENDP

        PUBLIC sound_blaster_dma_start_entry
; Program and start a Sound Blaster DMA transfer.
        PUBLIC start_sound_blaster_dma_playback
start_sound_blaster_dma_playback LABEL NEAR
sound_blaster_dma_start_entry PROC NEAR
        push eax
        mov byte ptr [sound_blaster_command_byte], sb_dsp_start_dma_command
        call write_sound_blaster_byte
        mov ax, word ptr [sound_dma_block_length]
        dec ax
        mov byte ptr [sound_blaster_command_byte], al
        call write_sound_blaster_byte
        mov byte ptr [sound_blaster_command_byte], ah
        call write_sound_blaster_byte
        pop eax
        ret
        push eax
        mov byte ptr [sound_dma_mode_bits],sound_dma_mode_single_transfer
        mov al, byte ptr [sound_blaster_dma_channel]
        mov byte ptr [sound_dma_channel], al
        call program_sound_dma_channel
        pop eax
        ret
; Configure the selected DMA channel for the sample stream.
        PUBLIC configure_sound_dma_input
configure_sound_dma_input LABEL NEAR
        push eax
        mov byte ptr [sound_dma_mode_bits],sound_dma_mode_single_auto_init_transfer
        mov al, byte ptr [sound_blaster_dma_channel]
        mov byte ptr [sound_dma_channel], al
        call program_sound_dma_channel
        pop eax
        ret
        PUBLIC stop_sound_blaster_dma
stop_sound_blaster_dma LABEL NEAR
        mov byte ptr [sound_blaster_command_byte], sb_dsp_halt_8bit_dma_command
        call write_sound_blaster_byte
        ret
        PUBLIC mask_active_sound_dma_channel
mask_active_sound_dma_channel LABEL NEAR
        push eax
        mov al, byte ptr [sound_blaster_dma_channel]
        mov byte ptr [sound_dma_channel], al
        call mask_sound_dma_channel
        pop eax
        ret
sound_blaster_dma_start_entry ENDP
        PUBLIC sound_blaster_rate_entry
; Convert the requested rate to the DSP time constant (the 0F42h scaled divisor).
        PUBLIC set_sound_blaster_sample_rate
set_sound_blaster_sample_rate LABEL NEAR
sound_blaster_rate_entry PROC NEAR
        push    ebp
        lea     ebp,[esp]
        push    eax
        push    ebx
        push    edx
        sub     dx,dx
        mov ax,sb_dsp_sample_rate_divisor
        mov     ebx,[ebp+8]
        add bx,sb_dsp_rate_rounding_bias
        shr     bx,8
        or      bx,bx
        jz      short sample_rate_done
        div     bx
        mov bx,sb_dsp_time_constant_base
        sub     bx,ax
        mov byte ptr sound_blaster_command_byte,sb_dsp_set_time_constant_command
        call    write_sound_blaster_byte
        jc      short sample_rate_done
        mov     byte ptr sound_blaster_command_byte,bl
        call    write_sound_blaster_byte
sample_rate_done:
        pop     edx
        pop     ebx
        pop     eax
        pop     ebp
        ret
sound_blaster_rate_entry ENDP
        PUBLIC pic_eoi_entry
        PUBLIC send_pic_end_of_interrupt
send_pic_end_of_interrupt LABEL NEAR
pic_eoi_entry PROC NEAR
        push    eax
        mov al,pic_end_of_interrupt_command
        cmp byte ptr sound_blaster_irq,pic_slave_irq_base
        jl      short send_master_pic_eoi
        out     pic_slave_command_port,al
send_master_pic_eoi:
        out     pic_master_command_port,al
        pop     eax
        ret
pic_eoi_entry ENDP
        PUBLIC sound_blaster_speaker_on_entry
        PUBLIC enable_sound_blaster_speaker
enable_sound_blaster_speaker LABEL NEAR
sound_blaster_speaker_on_entry PROC NEAR
        call    reset_sound_blaster_dsp
        mov byte ptr sound_blaster_command_byte,sb_dsp_speaker_on_command
        call    write_sound_blaster_byte
        ret
sound_blaster_speaker_on_entry ENDP
        PUBLIC sound_blaster_stop_entry
        PUBLIC stop_sound_blaster_playback
stop_sound_blaster_playback LABEL NEAR
sound_blaster_stop_entry PROC NEAR
        call    stop_sound_blaster_dma
        call    reset_sound_blaster_dsp
        mov byte ptr sound_blaster_command_byte,sb_dsp_speaker_off_command
        call    write_sound_blaster_byte
        ret
sound_blaster_stop_entry ENDP
        PUBLIC sound_blaster_read_entry
        PUBLIC read_sound_blaster_byte
read_sound_blaster_byte LABEL NEAR
sound_blaster_read_entry PROC NEAR
        push    ecx
        push    edx
        mov     dx,word ptr sound_blaster_base_port
        add dx, sb_dsp_read_status_irq_ack_offset
        mov ecx,sb_dsp_io_poll_count
wait_for_dsp_response:
        in      al,dx
        test al,sb_dsp_status_bit_7
        loope   short wait_for_dsp_response
        stc
        jecxz   short dsp_response_timeout
        add dx, sb_dsp_read_data_offset-sb_dsp_read_status_irq_ack_offset
        in      al,dx
        mov     byte ptr sound_blaster_response_byte,al
        clc
dsp_response_timeout:
        pop     edx
        pop     ecx
        ret
sound_blaster_read_entry ENDP
        PUBLIC sound_blaster_write_entry
        PUBLIC write_sound_blaster_byte
write_sound_blaster_byte LABEL NEAR
sound_blaster_write_entry PROC NEAR
        push    eax
        push    edx
        mov     dx,word ptr sound_blaster_base_port
        add dx, sb_dsp_write_status_offset
        in      al,dx
        test al,sb_dsp_status_bit_7
        je      short write_dsp_command_byte
        push    ecx
        mov ecx,sb_dsp_io_poll_count
wait_for_dsp_write_ready:
        in      al,dx
        test al,sb_dsp_status_bit_7
        loopne  short wait_for_dsp_write_ready
        stc
        or      ecx,ecx
        pop     ecx
        je      short dsp_write_done
write_dsp_command_byte:
        mov     al,byte ptr sound_blaster_command_byte
        out     dx,al
        clc
dsp_write_done:
        pop     edx
        pop     eax
        ret
sound_blaster_write_entry ENDP
        PUBLIC sound_blaster_reset_entry
        PUBLIC reset_sound_blaster_dsp
reset_sound_blaster_dsp LABEL NEAR
sound_blaster_reset_entry PROC NEAR
        push    edx
        mov     dx,word ptr sound_blaster_base_port
        add dx, sb_dsp_reset_port_offset
        mov al,sb_dsp_reset_enable
        out     dx,al
        push    eax
        mov ax,sb_dsp_reset_delay_count
dsp_reset_delay:
        dec     ax
        jne     short dsp_reset_delay
        pop     eax
        mov al,sb_dsp_reset_disable
        out     dx,al
        pop     edx
        call    sound_blaster_read_entry
        mov     eax,0
        jb      short dsp_reset_result
        cmp byte ptr sound_blaster_response_byte,sb_dsp_reset_ack_byte
        je      short dsp_reset_result
        mov     eax,0FFFFFFFFh
dsp_reset_result:
        ret
sound_blaster_reset_entry ENDP
        PUBLIC sound_blaster_ack_entry
        PUBLIC acknowledge_sound_blaster_irq
acknowledge_sound_blaster_irq LABEL NEAR
sound_blaster_ack_entry PROC NEAR
        push    edx
        mov     dx,word ptr sound_blaster_base_port
        add dx, sb_dsp_read_status_irq_ack_offset
        in      al,dx
        pop     edx
        ret
sound_blaster_ack_entry ENDP
_TEXT ENDS
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
        PUBLIC sound_dma_block_length
sound_dma_block_length	DW 0
        PUBLIC sound_blaster_response_byte
sound_blaster_response_byte	DB 0
        PUBLIC sound_blaster_command_byte
sound_blaster_command_byte	DB 0
_DATA ENDS
        END
