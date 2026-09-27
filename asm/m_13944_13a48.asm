.386
EXTRN g_e324:WORD
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC update_attr_register_entry
        PUBLIC update_attr_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write an attribute-controller register.
update_attr_register LABEL NEAR
update_attr_register_entry PROC NEAR
        push ebp
L_13945:
        lea ebp, [esp]
L_13948:
        push eax
L_13949:
        push edx
L_1394A:
        mov dx, 3C0h
L_1394E:
        mov al, byte ptr [ebp + 8]
L_13951:
        or al, 20h
L_13953:
        out dx, al
L_13954:
        jmp short L_13956
L_13956:
        jmp short L_13958
L_13958:
        in al, dx
L_13959:
        and al, byte ptr [ebp + 0Ch]
L_1395C:
        or al, byte ptr [ebp + 10h]
L_1395F:
        out dx, al
L_13960:
        pop edx
L_13961:
        pop eax
L_13962:
        pop ebp
L_13963:
        ret
update_attr_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC update_crtc_register_entry
        PUBLIC update_crtc_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write a CRT-controller register.
update_crtc_register LABEL NEAR
update_crtc_register_entry PROC NEAR
        push ebp
L_13965:
        lea ebp, [esp]
L_13968:
        push eax
L_13969:
        push edx
L_1396A:
        mov dx, 3D4h
L_1396E:
        mov al, byte ptr [ebp + 8]
L_13971:
        out dx, al
L_13972:
        jmp short L_13974
L_13974:
        jmp short L_13976
L_13976:
        inc dx
L_13978:
        in al, dx
L_13979:
        and al, byte ptr [ebp + 0Ch]
L_1397C:
        or al, byte ptr [ebp + 10h]
L_1397F:
        out dx, al
L_13980:
        pop edx
L_13981:
        pop eax
L_13982:
        pop ebp
L_13983:
        ret
update_crtc_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC update_seq_register_entry
        PUBLIC update_seq_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write a VGA sequencer register.
update_seq_register LABEL NEAR
update_seq_register_entry PROC NEAR
        push ebp
L_13985:
        lea ebp, [esp]
L_13988:
        push eax
L_13989:
        push edx
L_1398A:
        mov dx, 3C4h
L_1398E:
        mov al, byte ptr [ebp + 8]
L_13991:
        out dx, al
L_13992:
        jmp short L_13994
L_13994:
        jmp short L_13996
L_13996:
        inc dx
L_13998:
        in al, dx
L_13999:
        and al, byte ptr [ebp + 0Ch]
L_1399C:
        or al, byte ptr [ebp + 10h]
L_1399F:
        out dx, al
L_139A0:
        pop edx
L_139A1:
        pop eax
L_139A2:
        pop ebp
L_139A3:
        ret
update_seq_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC update_gc_register_entry
        PUBLIC update_gc_register
; Parameters: register index, bits to retain, and bits to set.
; Read-modify-write a VGA graphics-controller register.
update_gc_register LABEL NEAR
update_gc_register_entry PROC NEAR
        push ebp
L_139A5:
        lea ebp, [esp]
L_139A8:
        push eax
L_139A9:
        push edx
L_139AA:
        mov dx, 3CEh
L_139AE:
        mov al, byte ptr [ebp + 8]
L_139B1:
        out dx, al
L_139B2:
        jmp short L_139B4
L_139B4:
        jmp short L_139B6
L_139B6:
        inc dx
L_139B8:
        in al, dx
L_139B9:
        and al, byte ptr [ebp + 0Ch]
L_139BC:
        or al, byte ptr [ebp + 10h]
L_139BF:
        out dx, al
L_139C0:
        pop edx
L_139C1:
        pop eax
L_139C2:
        pop ebp
L_139C3:
        ret
update_gc_register_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC set_seq_plane_mask_entry
        PUBLIC set_seq_plane_mask
; Set the sequencer's four-bit plane write mask and update its cached value.
set_seq_plane_mask LABEL NEAR
set_seq_plane_mask_entry PROC NEAR
        push ebp
L_139C5:
        lea ebp, [esp]
L_139C8:
        push edx
L_139C9:
        mov al, 2
L_139CB:
        mov ah, byte ptr [ebp + 8]
L_139CE:
        and ah, 0Fh
L_139D1:
        mov byte ptr [g_e324+61h], ah
L_139D7:
        mov dx, 3C4h
L_139DB:
        out dx, ax
L_139DD:
        pop edx
L_139DE:
        pop ebp
L_139DF:
        shr eax, 8
L_139E2:
        ret
set_seq_plane_mask_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC rotate_seq_plane_mask_entry
        PUBLIC rotate_seq_plane_mask
; Rotate 0x11 by the requested amount and use the low nibble as the plane mask.
rotate_seq_plane_mask LABEL NEAR
rotate_seq_plane_mask_entry PROC NEAR
        push ebp
L_139E4:
        lea ebp, [esp]
L_139E7:
        push edx
L_139E8:
        mov ax, 1102h
L_139EC:
        mov edx, ecx
L_139EE:
        mov cl, byte ptr [ebp + 8]
L_139F1:
        rol ah, cl
L_139F3:
        mov ecx, edx
L_139F5:
        and ah, 0Fh
L_139F8:
        mov byte ptr [g_e324+61h], ah
L_139FE:
        mov dx, 3C4h
L_13A02:
        out dx, ax
L_13A04:
        pop edx
L_13A05:
        pop ebp
L_13A06:
        shr eax, 8
L_13A09:
        ret
rotate_seq_plane_mask_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC set_gc_read_map_entry
        PUBLIC set_gc_read_map
; Select the VGA plane used by graphics-controller reads.
set_gc_read_map LABEL NEAR
set_gc_read_map_entry PROC NEAR
        push ebp
L_13A0B:
        lea ebp, [esp]
L_13A0E:
        push edx
L_13A0F:
        mov al, 4
L_13A11:
        mov ah, byte ptr [ebp + 8]
L_13A14:
        and ah, 3
L_13A17:
        mov byte ptr [g_e324+62h], ah
L_13A1D:
        mov dx, 3CEh
L_13A21:
        out dx, ax
L_13A23:
        pop edx
L_13A24:
        pop ebp
L_13A25:
        shr eax, 8
L_13A28:
        ret
set_gc_read_map_entry ENDP
        ASSUME CS:_TEXT
        PUBLIC set_gc_mode_entry
        PUBLIC set_gc_mode
; Set the cached VGA graphics-controller mode register.
set_gc_mode LABEL NEAR
set_gc_mode_entry PROC NEAR
        push ebp
L_13A2A:
        lea ebp, [esp]
L_13A2D:
        push edx
L_13A2E:
        mov al, 5
L_13A30:
        mov ah, byte ptr [ebp + 8]
L_13A33:
        mov byte ptr [g_e324+60h], ah
L_13A39:
        mov dx, 3CEh
L_13A3D:
        out dx, ax
L_13A3F:
        pop edx
L_13A40:
        pop ebp
L_13A41:
        shr eax, 8
L_13A44:
        ret
        ORG $+3 ; original zero fill to the next aligned entry at 13A48h
set_gc_mode_entry ENDP
_TEXT ENDS
        END
