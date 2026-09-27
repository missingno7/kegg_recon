.386
DGROUP GROUP _DATA
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
; Service the DSP IRQ, update DMA/rate state, and hand the frame to the game callback.
        PUBLIC sound_blaster_irq_handler
sound_blaster_irq_handler LABEL NEAR
sound_blaster_irq_entry PROC NEAR
        push eax
L_11259:
        push ecx
L_1125A:
        push edx
L_1125B:
        mov dx, ds
L_1125E:
        rol edx, 10h
L_11261:
        mov ax, SEG DGROUP
L_11265:
        mov ds, eax
L_11267:
        mov dx, word ptr [sound_blaster_base_port]
L_1126E:
        add dx, 0Eh
L_11272:
        in al, dx
L_11273:
        cmp dword ptr [audio_dma_half_bytes], 0
L_1127A:
        je short L_112CF
L_1127C:
        mov eax, dword ptr [active_audio_rate]
L_11281:
        cmp dword ptr [last_audio_sample_rate], eax
L_11287:
        jne short L_112BF
L_11289:
        add dx, -2
L_1128D:
        mov ecx, 3E8h
L_11292:
        in al, dx
L_11293:
        test al, 80h
L_11295:
        loopne L_11292
L_11297:
        mov al, 14h
L_11299:
        out dx, al
L_1129A:
        mov ecx, 3E8h
L_1129F:
        in al, dx
L_112A0:
        test al, 80h
L_112A2:
        loopne L_1129F
L_112A4:
        mov al, 7Fh
L_112A6:
        out dx, al
L_112A7:
        mov ecx, 3E8h
L_112AC:
        in al, dx
L_112AD:
        test al, 80h
L_112AF:
        loopne L_112AC
L_112B1:
        mov al, 2
L_112B3:
        out dx, al
L_112B4:
        mov word ptr [audio_stream_flag], 0FFFFh
L_112BD:
        jmp short L_112D8
L_112BF:
        mov dword ptr [last_audio_sample_rate], eax
L_112C4:
        push eax
L_112C5:
        call set_sound_blaster_sample_rate
L_112CA:
        add esp, 4
L_112CD:
        jmp short L_11289
L_112CF:
        mov word ptr [audio_stream_flag], 0
L_112D8:
        mov dx, es
L_112DB:
        cld
L_112DC:
        mov ax, SEG DGROUP
L_112E0:
        mov es, eax
L_112E2:
        pushad
L_112E3:
        sti
L_112E4:
        call send_pic_end_of_interrupt
L_112E9:
        call transfer_audio_stream_block
L_112EE:
        popad
L_112EF:
        mov es, edx
L_112F1:
        rol edx, 10h
L_112F4:
        mov ds, edx
L_112F6:
        pop edx
L_112F7:
        pop ecx
L_112F8:
        pop eax
L_112F9:
        iretd
sound_blaster_irq_entry ENDP

        PUBLIC sound_blaster_dma_start_entry
; Program and start a Sound Blaster DMA transfer.
        PUBLIC start_sound_blaster_dma_playback
start_sound_blaster_dma_playback LABEL NEAR
sound_blaster_dma_start_entry PROC NEAR
        push eax
L_112FB:
        mov byte ptr [sound_blaster_command_byte], 14h
L_11302:
        call write_sound_blaster_byte
L_11307:
        mov ax, word ptr [sound_dma_block_length]
L_1130D:
        dec ax
L_1130F:
        mov byte ptr [sound_blaster_command_byte], al
L_11314:
        call write_sound_blaster_byte
L_11319:
        mov byte ptr [sound_blaster_command_byte], ah
L_1131F:
        call write_sound_blaster_byte
L_11324:
        pop eax
L_11325:
        ret
L_11326:
        push eax
L_11327:
        mov byte ptr [sound_dma_mode_bits], 48h
L_1132E:
        mov al, byte ptr [sound_blaster_dma_channel]
L_11333:
        mov byte ptr [sound_dma_channel], al
L_11338:
        call program_sound_dma_channel
L_1133D:
        pop eax
L_1133E:
        ret
; Configure the selected DMA channel for the sample stream.
        PUBLIC configure_sound_dma_input
configure_sound_dma_input LABEL NEAR
L_1133F:
        push eax
L_11340:
        mov byte ptr [sound_dma_mode_bits], 58h
L_11347:
        mov al, byte ptr [sound_blaster_dma_channel]
L_1134C:
        mov byte ptr [sound_dma_channel], al
L_11351:
        call program_sound_dma_channel
L_11356:
        pop eax
L_11357:
        ret
        PUBLIC stop_sound_blaster_dma
stop_sound_blaster_dma LABEL NEAR
L_11358:
        mov byte ptr [sound_blaster_command_byte], 0D0h
L_1135F:
        call write_sound_blaster_byte
L_11364:
        ret
        PUBLIC mask_active_sound_dma_channel
mask_active_sound_dma_channel LABEL NEAR
L_11365:
        push eax
L_11366:
        mov al, byte ptr [sound_blaster_dma_channel]
L_1136B:
        mov byte ptr [sound_dma_channel], al
L_11370:
        call mask_sound_dma_channel
L_11375:
        pop eax
L_11376:
        ret
sound_blaster_dma_start_entry ENDP
        PUBLIC sound_blaster_rate_entry
; Send the requested sample rate to the DSP.
        PUBLIC set_sound_blaster_sample_rate
set_sound_blaster_sample_rate LABEL NEAR
sound_blaster_rate_entry PROC NEAR
        push    ebp
        lea     ebp,[esp]
        push    eax
        push    ebx
        push    edx
        sub     dx,dx
        mov     ax,0F42h
        mov     ebx,[ebp+8]
        add     bx,7Fh
        shr     bx,8
        or      bx,bx
        jz      short L_113B8
        div     bx
        mov     bx,100h
        sub     bx,ax
        mov     byte ptr sound_blaster_command_byte,40h
        call    write_sound_blaster_byte
        jc      short L_113B8
        mov     byte ptr sound_blaster_command_byte,bl
        call    write_sound_blaster_byte
L_113B8:
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
        mov     al,20h
        cmp     byte ptr sound_blaster_irq,8
        jl      short L_113CB
        out     0A0h,al
L_113CB:
        out     20h,al
        pop     eax
        ret
pic_eoi_entry ENDP
        PUBLIC sound_blaster_speaker_on_entry
        PUBLIC enable_sound_blaster_speaker
enable_sound_blaster_speaker LABEL NEAR
sound_blaster_speaker_on_entry PROC NEAR
        call    reset_sound_blaster_dsp
        mov     byte ptr sound_blaster_command_byte,0D1h
        call    write_sound_blaster_byte
        ret
sound_blaster_speaker_on_entry ENDP
        PUBLIC sound_blaster_stop_entry
        PUBLIC stop_sound_blaster_playback
stop_sound_blaster_playback LABEL NEAR
sound_blaster_stop_entry PROC NEAR
        call    stop_sound_blaster_dma
        call    reset_sound_blaster_dsp
        mov     byte ptr sound_blaster_command_byte,0D3h
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
        add     dx,0Eh
        mov     ecx,3E8h
L_1140A:
        in      al,dx
        test    al,80h
        loope   short L_1140A
        stc
        jecxz   short L_1141D
        add     dx,-4
        in      al,dx
        mov     byte ptr sound_blaster_response_byte,al
        clc
L_1141D:
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
        add     dx,0Ch
        in      al,dx
        test    al,80h
        je      short L_11443
        push    ecx
        mov     ecx,3E8h
L_11438:
        in      al,dx
        test    al,80h
        loopne  short L_11438
        stc
        or      ecx,ecx
        pop     ecx
        je      short L_1144A
L_11443:
        mov     al,byte ptr sound_blaster_command_byte
        out     dx,al
        clc
L_1144A:
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
        add     dx,6
        mov     al,1
        out     dx,al
        push    eax
        mov     ax,0FFh
L_11461:
        dec     ax
        jne     short L_11461
        pop     eax
        mov     al,0
        out     dx,al
        pop     edx
        call    sound_blaster_read_entry
        mov     eax,0
        jb      short L_11484
        cmp     byte ptr sound_blaster_response_byte,0AAh
        je      short L_11484
        mov     eax,0FFFFFFFFh
L_11484:
        ret
sound_blaster_reset_entry ENDP
        PUBLIC sound_blaster_ack_entry
        PUBLIC acknowledge_sound_blaster_irq
acknowledge_sound_blaster_irq LABEL NEAR
sound_blaster_ack_entry PROC NEAR
        push    edx
        mov     dx,word ptr sound_blaster_base_port
        add     dx,0Eh
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
