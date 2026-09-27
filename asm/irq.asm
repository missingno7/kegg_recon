.386
; Real-mode interrupt-controller and Sound Blaster DSP values.
PIC_MASTER_COMMAND_PORT EQU 020h
PIC_EOI_COMMAND         EQU 020h
SB_DSP_WRITE_OFFSET     EQU 0Ch
SB_DSP_ACK_OFFSET       EQU 0Eh
SB_IRQ_TEMPLATE_OFFSET  EQU 0Dh
TIMER_IRQ_TEMPLATE_OFFSET EQU 0Dh
KEYBOARD_IRQ_TEMPLATE_OFFSET EQU 0Dh
SB_DSP_BUSY_BIT         EQU 080h
SB_DSP_WAIT_LIMIT       EQU 03E8h
SB_DSP_DMA_COMMAND      EQU 014h
IRQ_TEMPLATE_RELOAD     EQU 1234h
KEYBOARD_DATA_PORT      EQU 060h
KEYBOARD_RELEASE_FLAG   EQU 080h

; Context words embedded after the short entry jump in each copied Sound Blaster ISR.
SB_IRQ_TEMPLATE_FIELDS STRUC
SB_IRQ_INTERRUPT_COUNT  DW ?
SB_IRQ_DSP_BASE_PORT     DW ?
SB_IRQ_TRANSFER_LENGTH  DW ?
SB_IRQ_PIC_EOI_PORT      DW ?
SB_IRQ_TEMPLATE_FIELDS ENDS
TIMER_IRQ_TEMPLATE_FIELDS STRUC
TIMER_IRQ_INTERRUPT_COUNT DW ?
TIMER_IRQ_TEMPLATE_FIELDS ENDS
KEYBOARD_IRQ_TEMPLATE_FIELDS STRUC
KEYBOARD_IRQ_LAST_SCAN_CODE DB ?
KEYBOARD_IRQ_PREVIOUS_SCAN_FLAGS DB ?
KEYBOARD_IRQ_INTERRUPT_COUNT DW ?
KEYBOARD_IRQ_RESERVED_WORD_11 DW ?
KEYBOARD_IRQ_RESERVED_WORD_13 DW ?
KEYBOARD_IRQ_TEMPLATE_FIELDS ENDS
Vector SEGMENT USE16 AT 1122h
        ORG 3344h
PreviousIRQVector LABEL FAR
Vector ENDS
; Reconstructed real-mode ISR templates from LE object 2.
; Each independently paragraph-aligned USE16 CODE segment gives ORG 0
; offsets that the protected-mode installer uses when copying the template.
; The DSP templates acknowledge the PIC/DSP and issue command 14h with the saved block length.

SBIRQ0 SEGMENT PARA PUBLIC USE16 'CODE'
        ASSUME  CS:SBIRQ0
        ASSUME  DS:SBIRQ0
        ORG     0
PUBLIC a_0
a_0 LABEL BYTE
SBIRQ0_Start LABEL BYTE
        pushad
        mov     ax,ds
        push    eax
        mov     ax,IRQ_TEMPLATE_RELOAD ; patched to allocated real-mode segment
        mov     ds,ax
        jmp     short SBIRQ0_AcknowledgeInterrupt
SBIRQ0_InterruptCount DW 0
SBIRQ0_DSPBasePort DW 0
SBIRQ0_TransferLength DW 0
SBIRQ0_PICCommandPort DW 0
; Send one byte after the DSP write-status port clears its busy bit.
SBIRQ0_WriteDSPByte:
        push    eax
        push    ecx
        push    edx
        mov     dx,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_DSP_BASE_PORT]
        add     dx,SB_DSP_WRITE_OFFSET
        mov     ecx,SB_DSP_WAIT_LIMIT
        mov     ah,al
SBIRQ0_WaitForDSPReady:
        in      al,dx
        and     al,SB_DSP_BUSY_BIT
        loopne  SBIRQ0_WaitForDSPReady
        mov     al,ah
        out     dx,al
        pop     edx
        pop     ecx
        pop     eax
        ret
SBIRQ0_AcknowledgeInterrupt:
        mov     dx,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_PIC_EOI_PORT]
        mov     al,PIC_EOI_COMMAND
        out     dx,al
        mov     dx,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_DSP_BASE_PORT]
        add     dx,SB_DSP_ACK_OFFSET
        in      al,dx
        inc     word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_INTERRUPT_COUNT]
        mov     al,SB_DSP_DMA_COMMAND
        call    SBIRQ0_WriteDSPByte
        mov     ax,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_TRANSFER_LENGTH]
        dec     ax
        call    SBIRQ0_WriteDSPByte
        mov     al,ah
        call    SBIRQ0_WriteDSPByte
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr PreviousIRQVector         ; patched to previous real-mode vector
        nop
SBIRQ0 ENDS

SBIRQ1 SEGMENT PARA PUBLIC USE16 'CODE'
        ASSUME  CS:SBIRQ1
        ASSUME  DS:SBIRQ1
        ORG     0
PUBLIC a_70
a_70 LABEL BYTE
SBIRQ1_Start LABEL BYTE
        pushad
        mov     ax,ds
        push    eax
        mov     ax,IRQ_TEMPLATE_RELOAD
        mov     ds,ax
        jmp     short SBIRQ1_AcknowledgeInterrupt
SBIRQ1_InterruptCount DW 0
SBIRQ1_DSPBasePort DW 0
SBIRQ1_TransferLength DW 0
SBIRQ1_PICCommandPort DW 0
SBIRQ1_WriteDSPByte:
        push    eax
        push    ecx
        push    edx
        mov     dx,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_DSP_BASE_PORT]
        add     dx,SB_DSP_WRITE_OFFSET
        mov     ecx,SB_DSP_WAIT_LIMIT
        mov     ah,al
SBIRQ1_WaitForDSPReady:
        in      al,dx
        and     al,SB_DSP_BUSY_BIT
        loopne  SBIRQ1_WaitForDSPReady
        mov     al,ah
        out     dx,al
        pop     edx
        pop     ecx
        pop     eax
        ret
SBIRQ1_AcknowledgeInterrupt:
        mov     dx,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_PIC_EOI_PORT]
        mov     al,PIC_EOI_COMMAND
        out     dx,al
        mov     dx,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_DSP_BASE_PORT]
        add     dx,SB_DSP_ACK_OFFSET
        in      al,dx
        inc     word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_INTERRUPT_COUNT]
        mov     al,SB_DSP_DMA_COMMAND
        call    SBIRQ1_WriteDSPByte
        mov     ax,word ptr ds:[SB_IRQ_TEMPLATE_OFFSET+SB_IRQ_TRANSFER_LENGTH]
        dec     ax
        call    SBIRQ1_WriteDSPByte
        mov     al,ah
        call    SBIRQ1_WriteDSPByte
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr PreviousIRQVector
        nop
SBIRQ1 ENDS

TIMER SEGMENT PARA PUBLIC USE16 'CODE'
        ASSUME  CS:TIMER
        ASSUME  DS:TIMER
        ORG     0
PUBLIC a_e0
a_e0 LABEL BYTE
PUBLIC o2_e0
o2_e0 LABEL BYTE
TIMER_Start LABEL BYTE
        pushad
        mov     ax,ds
        push    eax
        mov     ax,IRQ_TEMPLATE_RELOAD
        mov     ds,ax
        jmp     short TIMER_AcknowledgeInterrupt
TIMER_InterruptCount DW 0
; Tick the copied counter, send the master PIC EOI, then chain to the saved handler.
TIMER_AcknowledgeInterrupt:
        inc     word ptr ds:[TIMER_IRQ_TEMPLATE_OFFSET+TIMER_IRQ_INTERRUPT_COUNT]
        mov     al,PIC_EOI_COMMAND
        out     PIC_MASTER_COMMAND_PORT,al
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr PreviousIRQVector
        nop
PUBLIC o2_103
o2_103 LABEL BYTE
TIMER ENDS

KEYBOARD SEGMENT PARA PUBLIC USE16 'CODE'
        ASSUME  CS:KEYBOARD
        ASSUME  DS:KEYBOARD
        ORG     0
PUBLIC a_110
a_110 LABEL BYTE
PUBLIC irq_110
irq_110 LABEL BYTE
KEYBOARD_Start LABEL BYTE
        pushad
        mov     ax,ds
        push    eax
        mov     ax,IRQ_TEMPLATE_RELOAD
        mov     ds,ax
        jmp     short KEYBOARD_AcknowledgeInterrupt
KEYBOARD_LastScanCode DB 0
KEYBOARD_PreviousScanFlags DB 0
KEYBOARD_InterruptCount DW 0
KEYBOARD_ReservedWord11 DW 0
KEYBOARD_ReservedWord13 DW 0
; Save the scan byte, retain the previous byte with its high-bit state, and chain.
KEYBOARD_AcknowledgeInterrupt:
        inc     word ptr ds:[KEYBOARD_IRQ_TEMPLATE_OFFSET+KEYBOARD_IRQ_INTERRUPT_COUNT]
        in      al,KEYBOARD_DATA_PORT
        mov     ah,al
        mov     al,byte ptr ds:[KEYBOARD_IRQ_TEMPLATE_OFFSET+KEYBOARD_IRQ_LAST_SCAN_CODE]
        or      al,KEYBOARD_RELEASE_FLAG
        mov     byte ptr ds:[KEYBOARD_IRQ_TEMPLATE_OFFSET+KEYBOARD_IRQ_PREVIOUS_SCAN_FLAGS],al
        mov     byte ptr ds:[KEYBOARD_IRQ_TEMPLATE_OFFSET+KEYBOARD_IRQ_LAST_SCAN_CODE],ah
        mov     al,PIC_EOI_COMMAND
        out     PIC_MASTER_COMMAND_PORT,al
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr PreviousIRQVector
        nop
KEYBOARD ENDS

        END
