.386
EXTRN g_746c:WORD
EXTRN g_746e:WORD
EXTRN g_7b16:WORD
EXTRN g_7b18:WORD
EXTRN g_8360:DWORD
EXTRN g_8368:DWORD
EXTRN g_8374:WORD
EXTRN g_8378:DWORD
EXTRN g_837c:DWORD
EXTRN g_8380:DWORD
EXTRN g_8384:DWORD
EXTRN g_8388:DWORD
EXTRN g_83aa:BYTE
EXTRN g_840e:DWORD
EXTRN g_e2e0:DWORD
EXTRN g_e2e4:DWORD
EXTRN g_e2e8:DWORD
EXTRN g_e324:WORD
EXTRN g_e326:DWORD
EXTRN g_e336:DWORD
EXTRN g_e346:DWORD
EXTRN g_e35e:DWORD
EXTRN g_e362:WORD
EXTRN g_e36e:DWORD
EXTRN g_e372:DWORD
EXTRN g_e376:DWORD
EXTRN g_e37a:DWORD
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_12cbd
a_12cbd:
        pushad
L_12CBE:
        lea ebp, [esp + 1Ch]
L_12CC2:
        cmp byte ptr [g_e384], 40h
L_12CC9:
        je short L_12CDC
L_12CCB:
        mov byte ptr [g_e384], 40h
L_12CD2:
        mov ax, 4005h
L_12CD6:
        mov dx, 3CEh
L_12CDA:
        out dx, ax
L_12CDC:
        movzx ebx, word ptr [g_7b16]
L_12CE3:
        shl ebx, 2
L_12CE6:
        mov esi, dword ptr [ebx + g_e326]
L_12CEC:
        add esi, dword ptr [ebx + g_e336]
L_12CF2:
        add esi, dword ptr [ebx + g_e346]
L_12CF8:
        movzx ebx, word ptr [g_7b18]
L_12CFF:
        shl ebx, 2
L_12D02:
        mov edi, dword ptr [ebx + g_e326]
L_12D08:
        add edi, dword ptr [ebx + g_e336]
L_12D0E:
        add edi, dword ptr [ebx + g_e346]
L_12D14:
        mov dword ptr [g_e2e4], esi
L_12D1A:
        mov dword ptr [g_e2e0], edi
L_12D20:
        mov dword ptr [g_8368], esi
L_12D26:
        mov ebx, dword ptr [g_e2e8]
L_12D2C:
        mov dword ptr [g_8360], ebx
L_12D32:
        mov ebx, dword ptr [ebp + 0Ch]
L_12D35:
        mov edx, dword ptr [ebp + 10h]
L_12D38:
        mov esi, dword ptr [ebp + 8]
L_12D3B:
        call L_12D5F
L_12D40:
        mov ebx, dword ptr [g_8360]
L_12D46:
        mov dword ptr [g_e2e8], ebx
L_12D4C:
        mov byte ptr [g_e385], 0Fh
L_12D53:
        mov ax, 0F02h
L_12D57:
        mov dx, 3C4h
L_12D5B:
        out dx, ax
L_12D5D:
        popad
L_12D5E:
        ret
L_12D5F:
        mov cx, word ptr [esi + 2]
L_12D63:
        mov bp, word ptr [g_e35e]
L_12D6A:
        add bp, bp
L_12D6D:
        cmp cx, bp
L_12D70:
        ja near ptr L_12E0E
L_12D76:
        rol ecx, 10h
L_12D79:
        mov cx, word ptr [esi + 4]
L_12D7D:
        mov bp, word ptr [g_e362]
L_12D84:
        add bp, bp
L_12D87:
        cmp cx, bp
L_12D8A:
        ja near ptr L_12E0E
L_12D90:
        movzx ebp, word ptr [esi + 8]
L_12D94:
        test word ptr [g_8374], 1
L_12D9D:
        je short L_12DB4
L_12D9F:
        test bp, 2
L_12DA4:
        je short L_12DC7
L_12DA6:
        movsx eax, word ptr [esi + 0Eh]
L_12DAA:
        add ebx, eax
L_12DAC:
        movsx eax, word ptr [esi + 10h]
L_12DB0:
        add edx, eax
L_12DB2:
        jmp short L_12DC7
L_12DB4:
        test bp, 1
L_12DB9:
        je short L_12DC7
L_12DBB:
        movsx eax, word ptr [esi + 0Ah]
L_12DBF:
        add ebx, eax
L_12DC1:
        movsx eax, word ptr [esi + 0Ch]
L_12DC5:
        add edx, eax
L_12DC7:
        mov eax, ebx
L_12DC9:
        or eax, edx
L_12DCB:
        cmp eax, 7D00h
L_12DD0:
        jg short L_12E19
L_12DD2:
        cmp eax, 0FFFF8300h
L_12DD7:
        jl short L_12E19
L_12DD9:
        movzx eax, word ptr [esi + 6]
L_12DDD:
        cmp eax, 1000h
L_12DE2:
        jg short L_12E3A
L_12DE4:
        add esi, eax
L_12DE6:
        and ebp, 7
L_12DE9:
        cmp word ptr [g_e324], 0
L_12DF1:
        jne short L_12DF8
L_12DF3:
        cmp ebp, 5
L_12DF6:
        je short L_12E45
L_12DF8:
        shl ebp, 4
L_12DFB:
        lea edi, [ebp + g_83aa]
L_12E01:
        movsx ebp, word ptr [g_746e]
L_12E08:
        mov ebp, dword ptr [ebp + edi]
L_12E0C:
        jmp dword ptr [edi]
L_12E0E:
        mov word ptr [g_746c], 401h
L_12E17:
        jmp short L_12E50
L_12E19:
        mov word ptr [g_746c], 402h
L_12E22:
        jmp short L_12E50
L_12E24:
        mov word ptr [g_746c], 403h
L_12E2D:
        jmp short L_12E50
L_12E2F:
        mov word ptr [g_746c], 404h
L_12E38:
        jmp short L_12E50
L_12E3A:
        mov word ptr [g_746c], 405h
L_12E43:
        jmp short L_12E50
L_12E45:
        mov word ptr [g_746c], 406h
L_12E4E:
        jmp short L_12E50
L_12E50:
        mov edi, dword ptr [g_840e]
L_12E56:
        mov dword ptr [edi], 0
L_12E5C:
        ret
L_12E5D:
        ret
L_12E5E:
        sub eax, eax
L_12E60:
        mov dword ptr [g_8378], eax
L_12E65:
        mov dword ptr [g_837c], eax
L_12E6A:
        mov dword ptr [g_8380], eax
L_12E6F:
        mov dword ptr [g_8384], eax
L_12E74:
        mov dword ptr [g_8388], eax
L_12E79:
        cmp edx, dword ptr [g_e372]
L_12E7F:
        jge short L_12E9C
L_12E81:
        mov eax, dword ptr [g_e372]
L_12E86:
        sub eax, edx
L_12E88:
        sub cx, ax
L_12E8B:
        jle near ptr L_12F2E
L_12E91:
        mov edx, dword ptr [g_e372]
L_12E97:
        mov dword ptr [g_8378], eax
L_12E9C:
        movsx eax, cx
L_12E9F:
        add eax, edx
L_12EA1:
        dec eax
L_12EA2:
        cmp eax, dword ptr [g_e37a]
L_12EA8:
        jle short L_12EBA
L_12EAA:
        sub eax, dword ptr [g_e37a]
L_12EB0:
        sub cx, ax
L_12EB3:
        jle short L_12F2E
L_12EB5:
        mov dword ptr [g_837c], eax
L_12EBA:
        cmp ebx, dword ptr [g_e36e]
L_12EC0:
        jge short L_12EE1
L_12EC2:
        mov eax, dword ptr [g_e36e]
L_12EC7:
        sub eax, ebx
L_12EC9:
        rol ecx, 10h
L_12ECC:
        sub cx, ax
L_12ECF:
        jle short L_12F2E
L_12ED1:
        rol ecx, 10h
L_12ED4:
        mov dword ptr [g_8380], eax
L_12ED9:
        mov ebx, dword ptr [g_e36e]
L_12EDF:
        jmp short L_12F0F
L_12EE1:
        mov eax, ecx
L_12EE3:
        shr eax, 10h
L_12EE6:
        add eax, ebx
L_12EE8:
        dec eax
L_12EE9:
        cmp eax, dword ptr [g_e376]
L_12EEF:
        jle short L_12F0F
L_12EF1:
        sub eax, dword ptr [g_e376]
L_12EF7:
        rol ecx, 10h
L_12EFA:
        sub cx, ax
L_12EFD:
        jle short L_12F2E
L_12EFF:
        mov dword ptr [g_8384], eax
L_12F04:
        movsx eax, cx
L_12F07:
        mov dword ptr [g_8388], eax
L_12F0C:
        rol ecx, 10h
L_12F0F:
        jmp short L_12F11
L_12F11:
        mov eax, dword ptr [g_e35e]
L_12F16:
        mul edx
L_12F18:
        add eax, ebx
L_12F1A:
        mov edi, dword ptr [g_8368]
L_12F20:
        add edi, eax
L_12F22:
        mov ebx, ecx
L_12F24:
        xchg ecx, ebp
L_12F26:
        shr ebx, 10h
L_12F29:
        movzx ebp, bp
L_12F2C:
        jmp ecx
L_12F2E:
        ret
        ORG $+1 ; original zero fill to the next even code address
ASM_TEXT ENDS
        END
