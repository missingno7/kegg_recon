.386
Vector SEGMENT USE16 AT 1122h
        ORG 3344h
HandlerVector LABEL FAR
Vector ENDS
; Reconstructed real-mode ISR templates from LE object 2.
; Each independently paragraph-aligned USE16 CODE segment gives ORG 0
; offsets that the protected-mode installer uses when copying the template.

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
        mov     ax,1234h           ; patched to allocated real-mode segment
        mov     ds,ax
        jmp     short SBIRQ0_Ack
SBIRQ0_Count DW 0
SBIRQ0_Port DW 0
SBIRQ0_Length DW 0
SBIRQ0_EoiPort DW 0
SBIRQ0_Service:
        push    eax
        push    ecx
        push    edx
        mov     dx,word ptr ds:[000fh]
        add     dx,0ch
        mov     ecx,03e8h
        mov     ah,al
SBIRQ0_Wait:
        in      al,dx
        and     al,80h
        loopne  SBIRQ0_Wait
        mov     al,ah
        out     dx,al
        pop     edx
        pop     ecx
        pop     eax
        ret
SBIRQ0_Ack:
        mov     dx,word ptr ds:[0013h]
        mov     al,20h
        out     dx,al
        mov     dx,word ptr ds:[000fh]
        add     dx,0eh
        in      al,dx
        inc     word ptr ds:[000dh]
        mov     al,14h
        call    SBIRQ0_Service
        mov     ax,word ptr ds:[0011h]
        dec     ax
        call    SBIRQ0_Service
        mov     al,ah
        call    SBIRQ0_Service
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr HandlerVector         ; patched to previous real-mode vector
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
        mov     ax,1234h
        mov     ds,ax
        jmp     short SBIRQ1_Ack
SBIRQ1_Count DW 0
SBIRQ1_Port DW 0
SBIRQ1_Length DW 0
SBIRQ1_EoiPort DW 0
SBIRQ1_Service:
        push    eax
        push    ecx
        push    edx
        mov     dx,word ptr ds:[000fh]
        add     dx,0ch
        mov     ecx,03e8h
        mov     ah,al
SBIRQ1_Wait:
        in      al,dx
        and     al,80h
        loopne  SBIRQ1_Wait
        mov     al,ah
        out     dx,al
        pop     edx
        pop     ecx
        pop     eax
        ret
SBIRQ1_Ack:
        mov     dx,word ptr ds:[0013h]
        mov     al,20h
        out     dx,al
        mov     dx,word ptr ds:[000fh]
        add     dx,0eh
        in      al,dx
        inc     word ptr ds:[000dh]
        mov     al,14h
        call    SBIRQ1_Service
        mov     ax,word ptr ds:[0011h]
        dec     ax
        call    SBIRQ1_Service
        mov     al,ah
        call    SBIRQ1_Service
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr HandlerVector
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
        mov     ax,1234h
        mov     ds,ax
        jmp     short TIMER_Ack
TIMER_Count DW 0
TIMER_Ack:
        inc     word ptr ds:[000dh]
        mov     al,20h
        out     20h,al
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr HandlerVector
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
        mov     ax,1234h
        mov     ds,ax
        jmp     short KEYBOARD_Ack
KEYBOARD_LastScan DB 0
KEYBOARD_CurrentScan DB 0
KEYBOARD_Count DW 0
KEYBOARD_Reserved0 DW 0
KEYBOARD_Reserved1 DW 0
KEYBOARD_Ack:
        inc     word ptr ds:[000fh]
        in      al,60h
        mov     ah,al
        mov     al,byte ptr ds:[000dh]
        or      al,80h
        mov     byte ptr ds:[000eh],al
        mov     byte ptr ds:[000dh],ah
        mov     al,20h
        out     20h,al
        pop     eax
        mov     ds,ax
        popad
        jmp     far ptr HandlerVector
        nop
KEYBOARD ENDS

        END
