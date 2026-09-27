.386
; PIT channel 0, both 8259 masks, and the RTC/NMI index port.
RTC_INDEX_PORT              EQU 070h
RTC_NMI_DISABLE_BIT         EQU 080h
RTC_INDEX_MASK              EQU 07Fh
PIC_MASTER_COMMAND_PORT     EQU 020h
PIC_MASTER_MASK_PORT        EQU 021h
PIC_SLAVE_MASK_PORT         EQU 0A1h
PIC_EOI_COMMAND             EQU 020h
PIC_MASK_ALL                EQU 0FFh
PIT_CHANNEL0_PORT           EQU 040h
PIT_COMMAND_PORT            EQU 043h
PIT_CHANNEL0_RATEGEN_LH     EQU 034h
PIT_RELOAD_ALL_ONES         EQU 0FFFFh
PIT_COUNTER_MODULUS         EQU 10000h
PIT_SETTLE_LOOP_SEED        EQU 0FFFFFC18h
EXTRN wait_for_vsync:NEAR ; T06 helper that waits for vertical retrace.
EXTRN process_timer_events:NEAR ; T06 helper that advances the timer-event callbacks.
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN pit_rollover_value:DWORD ; Current PIT reload value supplied by T06.
EXTRN timer_enabled09:DWORD ; T06 global incremented once per PIT IRQ; higher-level meaning is unclear.
        PUBLIC g_73d4
        PUBLIC g_pit_elapsed_ticks
; This separate initialized dword precedes the named sample result in the original data block.
g_73d4  DD 0
g_pit_elapsed_ticks DD 0
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ASSUME CS:_TEXT, DS:DGROUP
; Legacy entry name is called by the frozen T06 timing code; the descriptive alias is used here.
        PUBLIC f_9f64
        PUBLIC measure_pit_channel0
; Mask both PICs while sampling channel 0, then restore the RTC/NMI index and PIC masks.
f_9f64 LABEL NEAR
measure_pit_channel0 PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        pushfd
        cli
        in      al,RTC_INDEX_PORT
        mov     ah,al
        and     ah,RTC_NMI_DISABLE_BIT
        or      al,RTC_NMI_DISABLE_BIT
        jmp     short rtc_index_delay_1
rtc_index_delay_1:
        jmp     short rtc_index_delay_2
rtc_index_delay_2:
        jmp     short rtc_index_write
rtc_index_write:
        out     RTC_INDEX_PORT,al
        shl     eax,8
        in      al,PIC_MASTER_MASK_PORT
        mov     ah,al
        in      al,PIC_SLAVE_MASK_PORT
        push    eax
        mov     al,PIC_MASK_ALL
        out     PIC_MASTER_MASK_PORT,al
        out     PIC_SLAVE_MASK_PORT,al
        call    wait_for_vsync
        call    wait_for_vsync
        mov     al,PIT_CHANNEL0_RATEGEN_LH
        out     PIT_COMMAND_PORT,al
        jmp     short pit_mode_delay_1
pit_mode_delay_1:
        jmp     short pit_mode_delay_2
pit_mode_delay_2:
        jmp     short pit_mode_delay_3
pit_mode_delay_3:
        mov     al,0
        out     PIT_CHANNEL0_PORT,al
        jmp     short pit_counter_load_low_delay_1
pit_counter_load_low_delay_1:
        jmp     short pit_counter_load_low_delay_2
pit_counter_load_low_delay_2:
        jmp     short pit_counter_load_low_write
pit_counter_load_low_write:
        out     PIT_CHANNEL0_PORT,al
        push    ecx
        push    eax
        mov     ecx,PIT_SETTLE_LOOP_SEED
pit_settle_spin:
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        btc     eax,1
        inc     ecx
        jne     short pit_settle_spin
        pop     eax
        pop     ecx
        call    wait_for_vsync
        out     PIT_COMMAND_PORT,al
        jmp     short pit_latch_delay_1
pit_latch_delay_1:
        jmp     short pit_latch_delay_2
pit_latch_delay_2:
        jmp     short pit_latch_command
pit_latch_command:
        in      al,PIT_CHANNEL0_PORT
        jmp     short pit_read_low_delay_1
pit_read_low_delay_1:
        jmp     short pit_read_low_delay_2
pit_read_low_delay_2:
        jmp     short pit_read_low
pit_read_low:
        mov     ah,al
        in      al,PIT_CHANNEL0_PORT
        xchg    ah,al
        movzx   eax,ax
        mov     ebx,PIT_COUNTER_MODULUS
        sub     ebx,eax
        mov     dword ptr g_pit_elapsed_ticks,ebx
        pop     eax
        out     PIC_SLAVE_MASK_PORT,al
        mov     al,ah
        out     PIC_MASTER_MASK_PORT,al
        shr     eax,8
        in      al,RTC_INDEX_PORT
        and     al,RTC_INDEX_MASK
        or      al,ah
        jmp     short rtc_restore_delay_1
rtc_restore_delay_1:
        jmp     short rtc_restore_delay_2
rtc_restore_delay_2:
        jmp     short rtc_restore_index
rtc_restore_index:
        out     RTC_INDEX_PORT,al
        popfd
        popad
        mov     eax,dword ptr g_pit_elapsed_ticks
        ret
measure_pit_channel0 ENDP
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC set_pit_channel0_reload
set_pit_channel0_reload PROC NEAR
        push    ebp
        lea     ebp,[esp]
        push    eax
        push    ebx
        pushfd
        cli
        mov     ebx,[ebp+8]
        mov     al,PIT_CHANNEL0_RATEGEN_LH
        out     PIT_COMMAND_PORT,al
        jmp     short pit_reload_low_delay_1
pit_reload_low_delay_1:
        jmp     short pit_reload_low_delay_2
pit_reload_low_delay_2:
        jmp     short pit_reload_low_write
pit_reload_low_write:
        mov     al,bl
        out     PIT_CHANNEL0_PORT,al
        jmp     short pit_reload_high_delay_1
pit_reload_high_delay_1:
        jmp     short pit_reload_high_delay_2
pit_reload_high_delay_2:
        jmp     short pit_reload_high_write
pit_reload_high_write:
        mov     al,bh
        out     PIT_CHANNEL0_PORT,al
        popfd
        pop     ebx
        pop     eax
        pop     ebp
        ret
set_pit_channel0_reload ENDP
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC pit_channel0_interrupt
pit_channel0_interrupt PROC NEAR
        push    eax
        push    edx
        mov     al,PIT_CHANNEL0_RATEGEN_LH
        out     PIT_COMMAND_PORT,al
        mov     ax,PIT_RELOAD_ALL_ONES
        jmp     short irq_pit_low_delay_1
irq_pit_low_delay_1:
        jmp     short irq_pit_low_delay_2
irq_pit_low_delay_2:
        jmp     short irq_pit_low_write
irq_pit_low_write:
        out     PIT_CHANNEL0_PORT,al
        jmp     short irq_pit_high_delay_1
irq_pit_high_delay_1:
        jmp     short irq_pit_high_delay_2
irq_pit_high_delay_2:
        jmp     short irq_pit_high_write
irq_pit_high_write:
        mov     al,ah
        out     PIT_CHANNEL0_PORT,al
        mov     dx,ds
        rol     edx,10h
        mov     dx,es
        push    edx
        cld
        mov     ax,SEG DGROUP
        mov     ds,eax
        mov     es,eax
        call    wait_for_vsync
        mov     al,PIT_CHANNEL0_RATEGEN_LH
        out     PIT_COMMAND_PORT,al
        mov     eax,dword ptr pit_rollover_value
        jmp     short irq_pit_reload_low_delay_1
irq_pit_reload_low_delay_1:
        jmp     short irq_pit_reload_low_delay_2
irq_pit_reload_low_delay_2:
        jmp     short irq_pit_reload_low_write
irq_pit_reload_low_write:
        out     PIT_CHANNEL0_PORT,al
        jmp     short irq_pit_reload_high_delay_1
irq_pit_reload_high_delay_1:
        jmp     short irq_pit_reload_high_delay_2
irq_pit_reload_high_delay_2:
        jmp     short irq_pit_reload_high_write
irq_pit_reload_high_write:
        mov     al,ah
        out     PIT_CHANNEL0_PORT,al
        pushad
        inc     dword ptr timer_enabled09
        call    process_timer_events
        popad
        mov     al,PIC_EOI_COMMAND
        out     PIC_MASTER_COMMAND_PORT,al
        pop     edx
        mov     es,edx
        rol     edx,10h
        mov     ds,edx
        pop     edx
        pop     eax
        iretd
pit_channel0_interrupt ENDP
_TEXT ENDS
        END
