.386P
DGROUP GROUP _DATA
; ProTracker MOD and Sound Blaster constants used by this module.
MOD_MAGIC_MK                         EQU 2E4B2E4Dh ; 'M.K.'
MOD_MAGIC_FLT4                       EQU 34544C46h ; 'FLT4'
MOD_MAGIC_8CHN                       EQU 4E484338h ; '8CHN'
MOD_TAG_OFFSET                       EQU 438h
MOD_PATTERN_DATA_OFFSET              EQU 43Ch
MOD_SONG_LENGTH_OFFSET               EQU 3B6h
MOD_ORDER_TABLE_OFFSET                EQU 3B8h
MOD_PATTERN_COUNT                     EQU 80h
MOD_PATTERN_ROWS                      EQU 40h
MOD_SAMPLE_COUNT                      EQU 1Fh
MOD_SAMPLE_HEADER_BYTES               EQU 1Eh
MOD_TITLE_BYTES                       EQU 14h
MOD_SAMPLE_SLOTS                      EQU 20h
TRACKER_CHANNEL_STATE_BYTES           EQU 16h
MOD_ORDER_POSITION_UNINITIALIZED      EQU 0FFh
MOD_NIBBLE_SHIFT                      EQU 4
MOD_SAMPLE_NIBBLE_MASK                EQU 0F0h
TRACKER_FIXED_POINT_SHIFT             EQU 10h
SEGMENT_PAIR_ROTATE_BITS              EQU 10h
TRACKER_SAMPLE_AREA_OFFSET            EQU 4200h
TRACKER_MEMORY_ALIGN_BIAS             EQU 0FFh
TRACKER_MEMORY_ALIGN_MASK             EQU 0FFFFFF00h
TRACKER_RENDER_CHUNK_MASK             EQU 3Fh
TRACKER_RENDER_CHUNK_BOUNDARY         EQU -40h
TRACKER_MIX_GROUP_MASK                EQU 7
TRACKER_MIX_GROUP_SHIFT               EQU 3
TRACKER_CHANNEL_MIXER_VOLUME_MASK     EQU 0FF00h
MOD_FOUR_CHANNELS                     EQU 4
MOD_EIGHT_CHANNELS                    EQU 8
MOD_DEFAULT_TICKS_PER_ROW              EQU 6
MOD_DEFAULT_TEMPO_BPM                  EQU 7Dh
MOD_MAX_VOLUME                        EQU 40h
MOD_TEMPO_THRESHOLD_BPM                EQU 20h
MOD_PERIOD_MASK                       EQU 0FFFh
MOD_SAMPLE_CLOCK_HZ                   EQU 372C00h
MOD_TICK_TIME_NUMERATOR               EQU 280h
MILLISECONDS_PER_SECOND               EQU 3E8h
MOD_SAMPLE_RATE_ARG                   EQU 0Ch
MOD_FILE_IMAGE_ARG                    EQU 8
SOUND_BLASTER_BASE_ARG                EQU 10h
SOUND_IRQ_NUMBER_ARG                  EQU 14h
SOUND_DMA_CHANNEL_ARG                 EQU 18h
MOD_MIN_LOOP_WORDS                    EQU 2
MOD_EFFECT_SET_VOLUME                 EQU 0Ch
MOD_EFFECT_SET_SPEED_OR_TEMPO          EQU 0Fh
MOD_EFFECT_POSITION_JUMP              EQU 0Bh
MOD_EFFECT_PATTERN_BREAK              EQU 0Dh
MOD_EFFECT_SAMPLE_OFFSET              EQU 9
MOD_EFFECT_VOLUME_SLIDE               EQU 0Ah

PLAYER_ERROR_BAD_MOD                   EQU 602h
PLAYER_ERROR_DPMI_ALLOC                EQU 603h
PLAYER_ERROR_DSP_START                 EQU 604h
DPMI_ALLOCATE_DOS_MEMORY               EQU 100h
DPMI_FREE_DOS_MEMORY                   EQU 101h
DOS_SET_INTERRUPT_VECTOR                EQU 25h
DOS_GET_INTERRUPT_VECTOR                EQU 35h
DOS_SERVICES_INTERRUPT                  EQU 21h
DPMI_SERVICES_INTERRUPT                 EQU 31h
DOS_IRQ_MASTER_VECTOR_BASE              EQU 8
DOS_IRQ_SLAVE_VECTOR_BASE               EQU 60h
TRACKER_DOS_MEMORY_PARAGRAPHS           EQU 434h
TRACKER_DMA_BUFFER_BYTES                EQU 120h
TRACKER_MIX_TABLE_CLEAR_BYTES           EQU 140h
TRACKER_MAX_POLL_COUNT                  EQU 10000h
PCM_UNSIGNED_SILENCE                    EQU 80h
SB_DSP_BUSY_MASK                        EQU 80h
SAMPLE_LOOP_END_PATCH_VALUE             EQU 12345678h
SAMPLE_LOOP_LENGTH_PATCH_VALUE          EQU 12345678h

; Sound Blaster DSP register offsets are relative to module_sound_io_base.
SB_DSP_RESET_OFFSET                     EQU 6
SB_DSP_READ_DATA_OFFSET                 EQU 0Ah
SB_DSP_WRITE_DATA_OFFSET                EQU 0Ch
SB_DSP_READ_STATUS_OFFSET               EQU 0Eh
SB_DSP_STATUS_TO_WRITE_DELTA            EQU -2
SB_DSP_STATUS_TO_READ_DATA_DELTA        EQU 4
SB_DSP_RESET_TO_STATUS_DELTA            EQU 8
SB_DSP_CMD_SPEAKER_ON                   EQU 0D1h
SB_DSP_CMD_SET_TIME_CONSTANT            EQU 40h
SB_DSP_CMD_SINGLE_CYCLE_DMA             EQU 14h
SB_DSP_CMD_SPEAKER_OFF                  EQU 0D3h
SB_DSP_DMA_SINGLE_CYCLE_MODE            EQU 58h
SB_DSP_RESET_ACK                        EQU 0AAh

; 8237 DMA controller and cascaded 8259 PIC ports.
DMA_CHANNEL_MASK_PORT                   EQU 0Ah
DMA_MODE_PORT                           EQU 0Bh
DMA_CLEAR_FLIP_FLOP_PORT                EQU 0Ch
DMA_CHANNEL_MASK_BIT                    EQU 4
DMA_PAGE_PORTS_PACKED                   EQU 82818387h
DMA_COUNT_STABILITY_TOLERANCE            EQU 10h
DMA_COUNT_STABILITY_TOLERANCE_NEGATIVE   EQU -10h
PIC_MASTER_MASK_PORT                    EQU 21h
PIC_SLAVE_MASK_PORT                     EQU 0A1h
PIC_MASTER_EOI_PORT                     EQU 20h
PIC_SLAVE_EOI_PORT                      EQU 0A0h
PIC_END_OF_INTERRUPT_COMMAND            EQU 20h
PIC_IRQ_BIT_BASE                        EQU 1

; Packed on-disk and in-memory records. Structure fields are offsets only.
MODSampleHeader STRUC
    sample_name                         DB 22 DUP (?)
    sample_length_words                 DW ?
    sample_finetune                     DB ?
    sample_volume                       DB ?
    sample_repeat_start_words           DW ?
    sample_repeat_length_words          DW ?
MODSampleHeader ENDS

MODPatternEvent STRUC
    note_and_instrument_word            DW ?
    instrument_effect_word              DW ?
MODPatternEvent ENDS

ChannelRowEvent STRUC
    note_period                         DW ?
    sample_number                       DB ?
    volume                              DB ?
    effect_and_parameter                DW ?
ChannelRowEvent ENDS

TrackerChannelState STRUC
    sample_position                     DD ?
    sample_fraction                     DD ?
    sample_loop_start                   DD ?
    sample_loop_end                     DD ?
    sample_period                       DD ?
    channel_volume                      DB ?
    reserved                            DB ?
TrackerChannelState ENDS

_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
; Sound Blaster configuration and DPMI/IRQ bookkeeping.
        PUBLIC module_sound_io_base
module_sound_io_base	DW 0
        PUBLIC module_sound_irq_number
module_sound_irq_number	DB 0
        PUBLIC module_sound_dma_channel
module_sound_dma_channel	DB 0
        PUBLIC module_sample_rate
module_sample_rate	DW 0
        ; Saved DOS IRQ vector and DPMI selector for the allocated tracker buffers.
        PUBLIC module_saved_irq_vector_offset
module_saved_irq_vector_offset	DD 0
        PUBLIC module_saved_irq_vector_segment
module_saved_irq_vector_segment LABEL DWORD
        DB 0h, 0h
        PUBLIC tracker_dos_memory_selector
tracker_dos_memory_selector	DW 0
        ; Pointers to the generated volume table and the DMA output buffer.
        PUBLIC module_volume_mix_table
module_volume_mix_table	DD 0
        PUBLIC module_audio_buffer
module_audio_buffer	DD 0
        PUBLIC module_audio_buffer_bytes
module_audio_buffer_bytes	DW 0
        PUBLIC module_audio_buffer_half_offset
module_audio_buffer_half_offset	DW 0
        PUBLIC module_audio_buffer_remaining_bytes
module_audio_buffer_remaining_bytes	DW 0
        PUBLIC samples_until_next_tracker_tick
samples_until_next_tracker_tick	DW 0
        PUBLIC samples_per_tracker_tick
samples_per_tracker_tick	DW 0
        PUBLIC tracker_sample_period_scale
tracker_sample_period_scale	DD 0
        PUBLIC module_channel_count
module_channel_count	DW 0
        ; Eight packed mixer states: sample position, fraction, loop bounds, period, volume.
        PUBLIC module_channel_0_state
module_channel_0_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        PUBLIC module_channel_1_state
module_channel_1_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        PUBLIC module_channel_2_state
module_channel_2_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        PUBLIC module_channel_3_state
module_channel_3_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        PUBLIC module_channel_4_state
module_channel_4_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        PUBLIC module_channel_5_state
module_channel_5_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        PUBLIC module_channel_6_state
module_channel_6_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        PUBLIC module_channel_7_state
module_channel_7_state	DB TRACKER_CHANNEL_STATE_BYTES DUP (0)
        ; Pointer table used by the per-channel row parser and sample mixer.
        PUBLIC module_channel_state_table
module_channel_state_table	DD module_channel_0_state
        DD module_channel_1_state
        DD module_channel_2_state
        DD module_channel_3_state
        DD module_channel_4_state
        DD module_channel_5_state
        DD module_channel_6_state
        DD module_channel_7_state
        PUBLIC module_order_position
module_order_position	DB 0
        PUBLIC module_song_length
module_song_length	DB 0
        PUBLIC module_tick_counter
module_tick_counter	DB 0
        ; MOD order list, followed by the current row and its tempo/tick state.
        PUBLIC module_pattern_order_table
module_pattern_order_table	DB MOD_PATTERN_COUNT DUP (0)
        PUBLIC module_current_pattern_row
module_current_pattern_row	DD 0
        PUBLIC module_ticks_per_row
module_ticks_per_row	DB 0
        PUBLIC module_row_tick_countdown
module_row_tick_countdown	DB 0
        PUBLIC module_tempo_bpm
module_tempo_bpm	DB 0
        ; One cached six-byte event per possible output channel.
        PUBLIC module_channel_row_events
module_channel_row_events	DB 48 DUP (0)
        ; Parsed pattern addresses and per-sample start/loop/volume tables.
        PUBLIC module_pattern_addresses
module_pattern_addresses	DD MOD_PATTERN_COUNT DUP (0)
        PUBLIC module_sample_addresses
module_sample_addresses	DD MOD_SAMPLE_SLOTS DUP (0)
        PUBLIC module_sample_loop_starts
module_sample_loop_starts	DD MOD_SAMPLE_SLOTS DUP (0)
        PUBLIC module_sample_loop_ends
module_sample_loop_ends	DD MOD_SAMPLE_SLOTS DUP (0)
        PUBLIC module_sample_volumes
module_sample_volumes	DB MOD_SAMPLE_SLOTS DUP (0)
        PUBLIC module_player_error_code
module_player_error_code	DD 0
        PUBLIC module_saved_es_segment
module_saved_es_segment	DD 0
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
; Entry: edi points to a loaded MOD image; the remaining arguments are rate, DSP base, IRQ, and DMA.
; Accepts the four-channel M.K./FLT4 tags and the eight-channel 8CHN tag.
        PUBLIC load_protracker_module
load_protracker_module LABEL NEAR
parse_protracker_module PROC NEAR
        enter 0, 0
        pushad
        mov edi, dword ptr [ebp + MOD_FILE_IMAGE_ARG]
        mov ax, word ptr [ebp + MOD_SAMPLE_RATE_ARG]
        mov dx, word ptr [ebp + SOUND_BLASTER_BASE_ARG]
        mov cl, byte ptr [ebp + SOUND_IRQ_NUMBER_ARG]
        mov ch, byte ptr [ebp + SOUND_DMA_CHANNEL_ARG]
        mov word ptr [module_sound_io_base], dx
        mov byte ptr [module_sound_irq_number], cl
        mov byte ptr [module_sound_dma_channel], ch
        mov word ptr [module_sample_rate], ax
        mov eax, MOD_SAMPLE_CLOCK_HZ
        xor edx, edx
        shld edx, eax, TRACKER_FIXED_POINT_SHIFT
        shl eax, TRACKER_FIXED_POINT_SHIFT
        movzx ebx, word ptr [module_sample_rate]
        div ebx
        mov dword ptr [tracker_sample_period_scale], eax
        mov dword ptr [module_player_error_code], PLAYER_ERROR_BAD_MOD
        call parse_mod_header
        jb short load_error_exit
        mov dword ptr [module_player_error_code], PLAYER_ERROR_DPMI_ALLOC
        call allocate_tracker_memory
        jb short load_error_exit
        mov dl, byte ptr [module_tempo_bpm]
        call update_tick_period_for_tempo
        call program_dma_audio_buffer
        call uninstall_protracker_irq
        mov dword ptr [module_player_error_code], 0
        call start_sound_blaster_playback
        jae short load_error_exit
        mov dword ptr [module_player_error_code], PLAYER_ERROR_DSP_START
        call stop_playback_entry
load_error_exit:
        popad
        mov eax, dword ptr [module_player_error_code]
        leave
        ret
; Stop playback, mask the DMA channel, stop the DSP, and release the DPMI buffer.
        PUBLIC stop_protracker_module
stop_protracker_module LABEL NEAR
stop_playback_entry:
        pushad
        call stop_sound_blaster_playback
        call install_protracker_irq
        call mask_dma_channel
        call free_tracker_memory
        popad
        ret
; Decode the MOD order table and 31 sample headers, then derive sample pointers.
parse_mod_header:
        pushad
        mov word ptr [module_channel_count], MOD_FOUR_CHANNELS
        cmp dword ptr [edi + MOD_TAG_OFFSET], MOD_MAGIC_MK
        je short recognized_mod_signature
        cmp dword ptr [edi + MOD_TAG_OFFSET], MOD_MAGIC_FLT4
        je short recognized_mod_signature
        mov word ptr [module_channel_count], MOD_EIGHT_CHANNELS
        cmp dword ptr [edi + MOD_TAG_OFFSET], MOD_MAGIC_8CHN
        je short recognized_mod_signature
        stc
        jmp near ptr parse_mod_header_return
recognized_mod_signature:
        mov byte ptr [module_order_position], MOD_ORDER_POSITION_UNINITIALIZED
        mov byte ptr [module_tick_counter], MOD_PATTERN_ROWS
        mov byte ptr [module_ticks_per_row], MOD_DEFAULT_TICKS_PER_ROW
        mov byte ptr [module_row_tick_countdown], 0
        mov byte ptr [module_tempo_bpm], MOD_DEFAULT_TEMPO_BPM
        mov al, byte ptr [edi + MOD_SONG_LENGTH_OFFSET]
        mov byte ptr [module_song_length], al
        mov ecx, MOD_PATTERN_COUNT
        xor ebx, ebx
        xor ah, ah
scan_pattern_order_table:
        mov al, byte ptr [edi + ebx + MOD_ORDER_TABLE_OFFSET]
        mov byte ptr [ebx + module_pattern_order_table], al
        cmp al, ah
        jb short next_order_table_entry
        mov ah, al
next_order_table_entry:
        inc ebx
        loop scan_pattern_order_table
        movzx ecx, ah
        inc ecx
        xor ebx, ebx
        movzx eax, word ptr [module_channel_count]
        shl eax, 8
        mov esi, edi
        add esi, MOD_PATTERN_DATA_OFFSET
store_pattern_address:
        mov dword ptr [ebx*4 + module_pattern_addresses], esi
        add esi, eax
        inc ebx
        loop store_pattern_address
        mov ecx, MOD_SAMPLE_COUNT
        lea edi, [edi + MOD_TITLE_BYTES]
        xor ebx, ebx
        inc ebx
next_sample_header:
        mov al, byte ptr [edi + sample_volume]
        mov byte ptr [ebx + module_sample_volumes], al
        movzx eax, word ptr [edi + sample_length_words]
        movzx edx, word ptr [edi + sample_repeat_length_words]
        movzx ebp, word ptr [edi + sample_repeat_start_words]
        xchg al, ah
        xchg dl, dh
        xchg ax, bp
        xchg al, ah
        xchg ax, bp
        add eax, eax
        add edx, edx
        add ebp, ebp
        cmp edx, MOD_MIN_LOOP_WORDS
        ja short normalize_sample_loop
        xor edx, edx
        mov ebp, eax
normalize_sample_loop:
        add edx, ebp
        add eax, esi
        add edx, esi
        add ebp, esi
        mov dword ptr [ebx*4 + module_sample_addresses], esi
        mov dword ptr [ebx*4 + module_sample_loop_ends], edx
        mov dword ptr [ebx*4 + module_sample_loop_starts], ebp
        mov esi, eax
        add edi, MOD_SAMPLE_HEADER_BYTES
        inc ebx
        loop next_sample_header
        clc
parse_mod_header_return:
        popad
        ret
; Called by the IRQ-side renderer; advance rows and apply per-channel effects.
advance_protracker_tick:
        pushad
        dec byte ptr [module_row_tick_countdown]
        jle short begin_pattern_row
        mov esi, OFFSET module_channel_row_events
        xor ebx, ebx
process_channel_tick_effects:
        call apply_channel_tick_effect
        add esi, 6
        inc ebx
        cmp bx, word ptr [module_channel_count]
        jb short process_channel_tick_effects
        popad
        ret
begin_pattern_row:
        mov al, byte ptr [module_ticks_per_row]
        mov byte ptr [module_row_tick_countdown], al
        inc byte ptr [module_tick_counter]
        cmp byte ptr [module_tick_counter], MOD_PATTERN_ROWS
        jb short process_pattern_row
        xor ebx, ebx
        mov byte ptr [module_tick_counter], bl
        mov bl, byte ptr [module_order_position]
        inc bl
        cmp bl, byte ptr [module_song_length]
        jb short advance_order_position
        xor bl, bl
advance_order_position:
        mov byte ptr [module_order_position], bl
        mov bl, byte ptr [ebx + module_pattern_order_table]
        mov edi, dword ptr [ebx*4 + module_pattern_addresses]
        mov dword ptr [module_current_pattern_row], edi
process_pattern_row:
        mov edi, dword ptr [module_current_pattern_row]
        mov esi, OFFSET module_channel_row_events
        xor ebx, ebx
decode_next_channel_event:
        call decode_channel_pattern_event
        add esi, 6
        add edi, 4
        inc ebx
        cmp bx, word ptr [module_channel_count]
        jb short decode_next_channel_event
        mov dword ptr [module_current_pattern_row], edi
        popad
        ret
; Decode one four-byte MOD event into the six-byte cached channel-row record.
decode_channel_pattern_event:
        mov al, byte ptr [edi + instrument_effect_word]
        shr al, MOD_NIBBLE_SHIFT
        mov ah, byte ptr [edi]
        and ah, MOD_SAMPLE_NIBBLE_MASK
        or al, ah
        test al, al
        je short finish_note_event
        mov byte ptr [esi + sample_number], al
        movzx eax, al
        mov al, byte ptr [eax + module_sample_volumes]
        mov byte ptr [esi + volume], al
        call set_channel_volume
finish_note_event:
        mov ax, word ptr [edi + note_and_instrument_word]
        xchg al, ah
        and ax, MOD_PERIOD_MASK
        test ax, ax
        je short decode_pattern_effect
        mov word ptr [esi], ax
        movzx ecx, ax
        call calculate_channel_sample_period
        push esi
        push edi
        movzx eax, byte ptr [esi + sample_number]
        mov edx, dword ptr [eax*4 + module_sample_addresses]
        mov esi, dword ptr [eax*4 + module_sample_loop_starts]
        mov edi, dword ptr [eax*4 + module_sample_loop_ends]
        call set_channel_sample_bounds
        pop edi
        pop esi
decode_pattern_effect:
        mov ax, word ptr [edi + instrument_effect_word]
        xchg al, ah
        and ax, MOD_PERIOD_MASK
        mov word ptr [esi + effect_and_parameter], ax
        cmp ah, MOD_EFFECT_SET_VOLUME
        je short effect_set_volume
        cmp ah, MOD_EFFECT_SET_SPEED_OR_TEMPO
        je short effect_set_speed_or_tempo
        cmp ah, MOD_EFFECT_POSITION_JUMP
        je short effect_pattern_position_jump
        cmp ah, MOD_EFFECT_PATTERN_BREAK
        je short effect_pattern_break
        cmp ah, MOD_EFFECT_SAMPLE_OFFSET
        je short effect_sample_offset
        ret
; Apply the supported tick effects: volume, speed/tempo, order jump, break, and offset.
apply_channel_tick_effect:
        mov ax, word ptr [esi + effect_and_parameter]
        cmp ah, MOD_EFFECT_VOLUME_SLIDE
        je short effect_volume_slide
        ret
effect_set_volume:
        call set_channel_volume
effect_ignore_zero_speed:
        ret
effect_set_speed_or_tempo:
        test al, al
        je short effect_ignore_zero_speed
        cmp al, MOD_TEMPO_THRESHOLD_BPM
        jae short effect_set_tempo_bpm
        mov byte ptr [module_ticks_per_row], al
        mov byte ptr [module_row_tick_countdown], al
        ret
effect_set_tempo_bpm:
        mov byte ptr [module_tempo_bpm], al
        mov dl, al
        call update_tick_period_for_tempo
        ret
effect_pattern_position_jump:
        dec al
        mov byte ptr [module_order_position], al
        mov byte ptr [module_tick_counter], MOD_PATTERN_ROWS
        ret
effect_pattern_break:
        mov byte ptr [module_tick_counter], MOD_PATTERN_ROWS
        ret
effect_volume_slide:
        mov ah, al
        mov al, byte ptr [esi + volume]
        test ah, MOD_SAMPLE_NIBBLE_MASK
        je short slide_volume_down
        shr ah, MOD_NIBBLE_SHIFT
        add al, ah
        cmp al, MOD_MAX_VOLUME
        jbe short store_clamped_channel_volume
        mov al, SB_DSP_CMD_SET_TIME_CONSTANT
store_clamped_channel_volume:
        mov byte ptr [esi + volume], al
        call set_channel_volume
        ret
slide_volume_down:
        sub al, ah
        jge short store_clamped_channel_volume
        xor al, al
        jmp short store_clamped_channel_volume
effect_sample_offset:
        xor edx, edx
        mov dh, al
        push esi
        push edi
        movzx eax, byte ptr [esi + sample_number]
        add edx, dword ptr [eax*4 + module_sample_addresses]
        mov esi, dword ptr [eax*4 + module_sample_loop_starts]
        mov edi, dword ptr [eax*4 + module_sample_loop_ends]
        call set_channel_sample_bounds
        pop edi
        pop esi
        ret
; Allocate DOS memory through DPMI and build the unsigned-sample volume lookup table.
allocate_tracker_memory:
        pushad
        xor ax, ax
        mov word ptr [module_audio_buffer_half_offset], ax
        mov word ptr [samples_until_next_tracker_tick], ax
        mov word ptr [samples_per_tracker_tick], ax
        mov word ptr [tracker_dos_memory_selector], ax
        mov ax, DPMI_ALLOCATE_DOS_MEMORY
        mov bx, TRACKER_DOS_MEMORY_PARAGRAPHS
        int DPMI_SERVICES_INTERRUPT
        jb near ptr allocate_tracker_memory_return
        mov word ptr [tracker_dos_memory_selector], dx
        mov ecx, TRACKER_MIX_TABLE_CLEAR_BYTES
        movzx edi, ax
        shl edi, 4
        mov esi, edi
        add esi, ecx
        shl ax, 4
        neg ax
        cmp ax, cx
        jae short align_tracker_buffer_pointers
        mov esi, edi
        add edi, TRACKER_SAMPLE_AREA_OFFSET
align_tracker_buffer_pointers:
        add esi, TRACKER_MEMORY_ALIGN_BIAS
        and esi, TRACKER_MEMORY_ALIGN_MASK
        mov dword ptr [module_volume_mix_table], esi
        mov dword ptr [module_audio_buffer], edi
        mov ecx, TRACKER_DMA_BUFFER_BYTES
        mov word ptr [module_audio_buffer_bytes], cx
        mov [module_saved_es_segment], es
        push dword ptr [module_saved_es_segment]
        mov ax, ds
        mov es, eax
        cld
        mov al, PCM_UNSIGNED_SILENCE
        rep stosb
        pop dword ptr [module_saved_es_segment]
        mov es, [module_saved_es_segment]
        mov edi, dword ptr [module_volume_mix_table]
        mov cx, word ptr [module_channel_count]
        shr cx, 3
        xor bx, bx
build_volume_mix_table:
        mov al, bl
        imul bh
        sar ax, cl
        mov byte ptr [edi], ah
        inc edi
        inc bl
        jne short build_volume_mix_table
        inc bh
        cmp bh, MOD_MAX_VOLUME
        jbe short build_volume_mix_table
        clc
allocate_tracker_memory_return:
        popad
        ret
; Release the real-mode DOS memory block only when allocation succeeded.
free_tracker_memory:
        pushad
        mov ax, DPMI_FREE_DOS_MEMORY
        mov dx, word ptr [tracker_dos_memory_selector]
        test dx, dx
        je short free_tracker_memory_return
        int DPMI_SERVICES_INTERRUPT
        mov word ptr [tracker_dos_memory_selector], 0
free_tracker_memory_return:
        popad
        ret
; Select the channel's sample start and loop bounds from the parsed MOD tables.
set_channel_sample_bounds:
        push ebx
        mov ebx, dword ptr [ebx*4 + module_channel_state_table]
        mov dword ptr [ebx], edx
        mov dword ptr [ebx + sample_loop_start], esi
        mov dword ptr [ebx + sample_loop_end], edi
        pop ebx
        ret
; Convert the MOD note period to the fixed-point sample-advance value.
calculate_channel_sample_period:
        push eax
        push ebx
        push edx
        jecxz sample_period_ready
        mov ebx, dword ptr [ebx*4 + module_channel_state_table]
        mov eax, dword ptr [tracker_sample_period_scale]
        xor edx, edx
        div ecx
        mov dword ptr [ebx + sample_period], eax
sample_period_ready:
        pop edx
        pop ebx
        pop eax
        ret
; Store the four-bit MOD sample volume in this channel's mixer state.
set_channel_volume:
        push ebx
        mov ebx, dword ptr [ebx*4 + module_channel_state_table]
        mov byte ptr [ebx + channel_volume], al
        pop ebx
        ret
; Recompute the number of output samples generated per tracker tick.
update_tick_period_for_tempo:
        push eax
        push ecx
        push edx
        mov ch, dl
        xor cl, cl
        mov ax, word ptr [module_sample_rate]
        mov dx, MOD_TICK_TIME_NUMERATOR
        mul dx
        div cx
        mov word ptr [samples_per_tracker_tick], ax
        pop edx
        pop ecx
        pop eax
        ret
; Refill the DMA buffer and advance the track when a tracker row is due.
render_sample_buffer:
        movzx edi, word ptr [module_audio_buffer_half_offset]
        add edi, dword ptr [module_audio_buffer]
        movzx ecx, word ptr [module_audio_buffer_bytes]
        shr ecx, 1
        mov word ptr [module_audio_buffer_remaining_bytes], cx
        xor word ptr [module_audio_buffer_half_offset], cx
        push edi
        mov [module_saved_es_segment], es
        push dword ptr [module_saved_es_segment]
        mov ax, ds
        mov es, eax
        mov al, PCM_UNSIGNED_SILENCE
        cld
        rep stosb
        pop dword ptr [module_saved_es_segment]
        mov es, [module_saved_es_segment]
        pop edi
render_row_when_due:
        cmp word ptr [samples_until_next_tracker_tick], 0
        jg short limit_render_chunk_to_row
        call advance_protracker_tick
        mov ax, word ptr [samples_per_tracker_tick]
        add word ptr [samples_until_next_tracker_tick], ax
limit_render_chunk_to_row:
        mov ax, word ptr [module_audio_buffer_remaining_bytes]
        mov cx, word ptr [samples_until_next_tracker_tick]
        add cx, TRACKER_RENDER_CHUNK_MASK
        and cx, TRACKER_RENDER_CHUNK_BOUNDARY
        cmp ax, cx
        jle short mix_render_chunk_channels
        mov ax, cx
mix_render_chunk_channels:
        sub word ptr [module_audio_buffer_remaining_bytes], ax
        sub word ptr [samples_until_next_tracker_tick], ax
        movzx ecx, ax
        mov ebx, OFFSET module_channel_0_state
        mov dx, word ptr [module_channel_count]
mix_next_channel:
        push ebx
        push ecx
        push edx
        push edi
        call mix_channel_samples
        pop edi
        pop edx
        pop ecx
        pop ebx
        add ebx, TRACKER_CHANNEL_STATE_BYTES
        dec dx
        jg short mix_next_channel
        add edi, ecx
        cmp word ptr [module_audio_buffer_remaining_bytes], 0
        jg short render_row_when_due
        ret
; Mix one channel through self-modified loop bounds; eight samples are unrolled below.
mix_channel_samples:
        push ebx
        mov eax, dword ptr [ebx + sample_loop_end]
        ASSUME DS:_TEXT ; keep: self-modifying stores into the code
        mov dword ptr ds:[mix_eight_sample_group+2], eax
        mov dword ptr ds:[check_wrapped_sample_position+2], eax
        sub eax, dword ptr [ebx + sample_loop_start]
        mov dword ptr ds:[wrap_sample_loop_position+2], eax
        ASSUME DS:DGROUP ; keep
        mov esi, dword ptr [ebx]
        mov ebp, dword ptr [ebx + sample_fraction]
        mov eax, dword ptr [ebx + sample_period]
        xor edx, edx
        shld edx, eax, TRACKER_FIXED_POINT_SHIFT
        shl eax, TRACKER_FIXED_POINT_SHIFT
        mov bh, byte ptr [ebx + channel_volume]
        and ebx, TRACKER_CHANNEL_MIXER_VOLUME_MASK
        add ebx, dword ptr [module_volume_mix_table]
lookup_volume_scaled_sample:
        test ecx, TRACKER_MIX_GROUP_MASK
        jne short lookup_volume_scaled_sample
        shr ecx, TRACKER_MIX_GROUP_SHIFT
        jecxz finish_channel_sample_mix
        neg ecx
        jmp short mix_eight_sample_group
        ALIGN 4
        nop
        nop
mix_eight_sample_group:
        cmp esi, SAMPLE_LOOP_END_PATCH_VALUE
        jae short wrap_sample_loop_position
mix_sample_if_before_loop_end:
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        mov bl, byte ptr [esi]
        add ebp, eax
        mov bl, byte ptr [ebx]
        adc esi, edx
        add byte ptr [edi], bl
        inc edi
        inc ecx
        jne short mix_eight_sample_group
finish_channel_sample_mix:
        pop ebx
        mov dword ptr [ebx], esi
        mov dword ptr [ebx + sample_fraction], ebp
        ret
wrap_sample_loop_position:
        sub esi, SAMPLE_LOOP_LENGTH_PATCH_VALUE
check_wrapped_sample_position:
        cmp esi, SAMPLE_LOOP_END_PATCH_VALUE
        jb short mix_sample_if_before_loop_end
        jmp short finish_channel_sample_mix
; Wait for DSP write-ready at base+0Ch before sending a command or parameter.
write_sound_blaster_dsp_byte:
        push eax
        push ecx
        push edx
        mov dx, word ptr [module_sound_io_base]
        add dx, SB_DSP_WRITE_DATA_OFFSET
        mov ecx, TRACKER_MAX_POLL_COUNT
        mov ah, al
wait_for_dsp_write_ready:
        in al, dx
        and al, SB_DSP_BUSY_MASK
        loopne wait_for_dsp_write_ready
        mov al, ah
        out dx, al
        pop edx
        pop ecx
        pop eax
        ret
; Pulse the DSP reset register and wait for its 0AAh acknowledgement.
reset_sound_blaster_dsp:
        pushad
        mov dx, word ptr [module_sound_io_base]
        add dx, SB_DSP_RESET_OFFSET
        mov al, 1
        out dx, al
        in al, dx
        in al, dx
        in al, dx
        in al, dx
        mov al, 0
        out dx, al
        add dx, SB_DSP_RESET_TO_STATUS_DELTA
        mov ecx, TRACKER_MAX_POLL_COUNT
wait_for_dsp_read_ready:
        in al, dx
        and al, SB_DSP_BUSY_MASK
        loope wait_for_dsp_read_ready
        sub dx, SB_DSP_STATUS_TO_READ_DATA_DELTA
        in al, dx
        cmp al, SB_DSP_RESET_ACK
        clc
        je short finish_dsp_reset
        stc
finish_dsp_reset:
        popad
        ret
; Program the DSP block length after the DMA channel is configured.
start_sound_blaster_playback:
        pushad
        call reset_sound_blaster_dsp
        jb short start_playback_return
        mov al, SB_DSP_CMD_SPEAKER_ON
        call write_sound_blaster_dsp_byte
        mov al, SB_DSP_CMD_SET_TIME_CONSTANT
        call write_sound_blaster_dsp_byte
        mov ax, MILLISECONDS_PER_SECOND
        mul ax
        div word ptr [module_sample_rate]
        neg ax
        call write_sound_blaster_dsp_byte
        mov al, SB_DSP_CMD_SINGLE_CYCLE_DMA
        call write_sound_blaster_dsp_byte
        mov ax, word ptr [module_audio_buffer_bytes]
        shr ax, 1
        dec ax
        call write_sound_blaster_dsp_byte
        mov al, ah
        call write_sound_blaster_dsp_byte
        clc
start_playback_return:
        popad
        ret
; Send the DSP speaker-off command before unhooking the IRQ.
stop_sound_blaster_playback:
        pushad
        call reset_sound_blaster_dsp
        mov al, SB_DSP_CMD_SPEAKER_OFF
        call write_sound_blaster_dsp_byte
        popad
        ret
; Mask the configured 8237 channel while playback is stopped.
mask_dma_channel:
        pushad
        mov al, byte ptr [module_sound_dma_channel]
        or al, DMA_CHANNEL_MASK_BIT
        out DMA_CHANNEL_MASK_PORT, al
        popad
        ret
; Program the 8237 channel address/count registers for the unsigned PCM buffer.
program_dma_audio_buffer:
        pushad
        mov cl, byte ptr [module_sound_dma_channel]
        mov al, cl
        or al, DMA_CHANNEL_MASK_BIT
        out DMA_CHANNEL_MASK_PORT, al
        out DMA_CLEAR_FLIP_FLOP_PORT, al
        mov al, cl
        or al, SB_DSP_DMA_SINGLE_CYCLE_MODE
        out DMA_MODE_PORT, al
        movzx dx, cl
        add dx, dx
        mov eax, dword ptr [module_audio_buffer]
        out dx, al
        mov al, ah
        out dx, al
        inc dx
        mov ax, word ptr [module_audio_buffer_bytes]
        dec ax
        out dx, al
        mov al, ah
        out dx, al
        mov edx, DMA_PAGE_PORTS_PACKED
        shl cl, TRACKER_MIX_GROUP_SHIFT
        shr edx, cl
        xor dh, dh
        shr cl, TRACKER_MIX_GROUP_SHIFT
        shr eax, TRACKER_FIXED_POINT_SHIFT
        out dx, al
        mov al, cl
        out DMA_CHANNEL_MASK_PORT, al
        popad
        ret
; Read the active 8237 byte count until two consecutive samples agree.
read_current_dma_byte_count:
        push ecx
        push edx
        movzx dx, byte ptr [module_sound_dma_channel]
        add dx, dx
        inc dx
        in al, dx
        mov ah, al
        in al, dx
        xchg al, ah
read_dma_counter_again:
        mov cx, ax
        in al, dx
        mov ah, al
        in al, dx
        xchg al, ah
        sub cx, ax
        cmp cx, DMA_COUNT_STABILITY_TOLERANCE
        jg short read_dma_counter_again
        cmp cx, DMA_COUNT_STABILITY_TOLERANCE_NEGATIVE
        jl short read_dma_counter_again
        neg ax
        add ax, word ptr [module_audio_buffer_bytes]
        dec ax
        pop edx
        pop ecx
        ret
; Save and replace the DOS IRQ vector, then unmask this IRQ on the PIC.
install_protracker_irq:
        pushad
        mov ax, ds
        push eax
        in al, PIC_SLAVE_MASK_PORT
        mov ah, al
        in al, PIC_MASTER_MASK_PORT
        mov dx, PIC_IRQ_BIT_BASE
        mov cl, byte ptr [module_sound_irq_number]
        shl dx, cl
        or ax, dx
        out PIC_MASTER_MASK_PORT, al
        mov al, ah
        out PIC_SLAVE_MASK_PORT, al
        mov ah, DOS_SET_INTERRUPT_VECTOR
        mov al, cl
        cmp al, 8
        jb short select_dos_irq_vector
        add al, DOS_IRQ_SLAVE_VECTOR_BASE
select_dos_irq_vector:
        add al, DOS_IRQ_MASTER_VECTOR_BASE
        lds edx, fword ptr [module_saved_irq_vector_offset]
        xor ebx, ebx
        mov bx, ds
        or ebx, edx
        test ebx, ebx
        je short restore_module_data_segment
        int DOS_SERVICES_INTERRUPT
restore_module_data_segment:
        pop eax
        mov ds, eax
        xor ebx, ebx
        mov dword ptr [module_saved_irq_vector_offset], ebx
        mov word ptr [module_saved_irq_vector_segment], bx
        popad
        ret
; Restore the original DOS IRQ vector and mask state.
uninstall_protracker_irq:
        pushad
        mov ax, ds
        rol eax, SEGMENT_PAIR_ROTATE_BITS
        mov ax, es
        push eax
        mov ah, DOS_GET_INTERRUPT_VECTOR
        mov al, byte ptr [module_sound_irq_number]
        cmp al, 8
        jb short restore_irq_vector_number
        add al, DOS_IRQ_SLAVE_VECTOR_BASE
restore_irq_vector_number:
        add al, DOS_IRQ_MASTER_VECTOR_BASE
        int DOS_SERVICES_INTERRUPT
        mov dword ptr [module_saved_irq_vector_offset], ebx
        mov [module_saved_irq_vector_segment], es
        mov ah, DOS_SET_INTERRUPT_VECTOR
        mov al, byte ptr [module_sound_irq_number]
        cmp al, 8
        jb short set_irq_vector_number
        add al, DOS_IRQ_SLAVE_VECTOR_BASE
set_irq_vector_number:
        add al, DOS_IRQ_MASTER_VECTOR_BASE
        mov dx, cs
        mov ds, edx
        mov edx, OFFSET protracker_irq_handler
        int DOS_SERVICES_INTERRUPT
        pop eax
        mov es, eax
        rol eax, SEGMENT_PAIR_ROTATE_BITS
        mov ds, eax
        in al, PIC_SLAVE_MASK_PORT
        mov al, ah
        in al, PIC_MASTER_MASK_PORT
        mov dx, PIC_IRQ_BIT_BASE
        mov cl, byte ptr [module_sound_irq_number]
        shl dx, cl
        not dx
        and ax, dx
        out PIC_MASTER_MASK_PORT, al
        mov al, ah
        out PIC_SLAVE_MASK_PORT, al
        popad
        ret
        PUBLIC protracker_irq_handler
; SB IRQ: acknowledge the DSP, restart its block, mix the next buffer, then EOI.
protracker_irq_handler LABEL DWORD
        push eax
        mov ax, ds
        push eax
        mov ax, SEG DGROUP
        mov ds, eax
        push eax
        push ecx
        push edx
        mov dx, word ptr [module_sound_io_base]
        add dx, SB_DSP_READ_STATUS_OFFSET
        in al, dx
        add dx, SB_DSP_STATUS_TO_WRITE_DELTA
        mov ecx, TRACKER_MAX_POLL_COUNT
        mov ah, al
wait_for_irq_dsp_command_ready:
        in al, dx
        and al, SB_DSP_BUSY_MASK
        loopne wait_for_irq_dsp_command_ready
        mov al, SB_DSP_CMD_SINGLE_CYCLE_DMA
        out dx, al
        mov ecx, TRACKER_MAX_POLL_COUNT
wait_for_irq_dsp_length_ready:
        in al, dx
        and al, SB_DSP_BUSY_MASK
        loopne wait_for_irq_dsp_length_ready
        mov ax, word ptr [module_audio_buffer_bytes]
        shr ax, 1
        dec ax
        out dx, al
        mov ecx, TRACKER_MAX_POLL_COUNT
wait_for_irq_dsp_high_byte_ready:
        in al, dx
        and al, SB_DSP_BUSY_MASK
        loopne wait_for_irq_dsp_high_byte_ready
        mov ax, word ptr [module_audio_buffer_bytes]
        shr ax, 1
        dec ax
        mov al, ah
        out dx, al
        pop edx
        pop ecx
        pop eax
        mov al, PIC_END_OF_INTERRUPT_COMMAND
        cmp byte ptr [module_sound_irq_number], 8
        jl short send_master_pic_end_of_interrupt
        out PIC_SLAVE_EOI_PORT, al
send_master_pic_end_of_interrupt:
        out PIC_MASTER_EOI_PORT, al
        sti
        pushad
        call render_sample_buffer
        popad
        pop eax
        mov ds, eax
        pop eax
        iretd

parse_protracker_module ENDP
_TEXT ENDS
        END
