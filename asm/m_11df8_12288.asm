.386
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN g_e2ac:BYTE
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC a_11df8
        PUBLIC f_11df8
f_11df8 LABEL NEAR
a_11df8:
        pushad
L_11DF9:
        lea ebp, [esp + 1Ch]
L_11DFD:
        mov eax, dword ptr [ebp + 14h]
L_11E00:
        mov dword ptr [g_8320], eax
L_11E05:
        lea eax, [eax + 1400h]
L_11E0B:
        mov dword ptr [g_8324], eax
L_11E10:
        lea eax, [eax + 2800h]
L_11E16:
        mov dword ptr [g_8328], eax
L_11E1B:
        lea eax, [eax + 2800h]
L_11E21:
        mov word ptr [g_832c], 0Ch
L_11E2A:
        mov ebx, dword ptr [ebp + 0Ch]
L_11E2D:
        mov dword ptr [g_834d], ebx
L_11E33:
        mov esi, dword ptr [ebp + 8]
L_11E36:
        mov word ptr [g_8314], 0
L_11E3F:
        lodsd
L_11E40:
        mov word ptr [g_8355], 901h
L_11E49:
        cmp eax, 38464947h
L_11E4E:
        jne near ptr L_12222
L_11E54:
        lodsw
L_11E56:
        mov word ptr [g_8355], 902h
L_11E5F:
        cmp ax, 6137h
L_11E63:
        jne near ptr L_12222
L_11E69:
        lodsw
L_11E6B:
        mov word ptr [g_8318], ax
L_11E71:
        lodsw
L_11E73:
        mov word ptr [g_831a], ax
L_11E79:
        lodsb
L_11E7A:
        or al, al
L_11E7C:
        sets byte ptr [g_831c]
L_11E83:
        mov bl, al
L_11E85:
        xor bh, bh
L_11E87:
        shr bl, 4
L_11E8A:
        and bl, 7
L_11E8D:
        inc bl
L_11E8F:
        mov byte ptr [g_831d], bl
L_11E95:
        mov bl, al
L_11E97:
        and bl, 7
L_11E9A:
        inc bl
L_11E9C:
        mov word ptr [g_8355], 905h
L_11EA5:
        cmp bx, 8
L_11EA9:
        jne near ptr L_12222
L_11EAF:
        mov word ptr [g_831e], bx
L_11EB6:
        lodsb
L_11EB7:
        lodsb
L_11EB8:
        cmp byte ptr [g_831c], 0
L_11EBF:
        je short L_11EE6
L_11EC1:
        mov bx, 1
L_11EC5:
        mov cx, word ptr [g_831e]
L_11ECC:
        shl bx, cl
L_11ECF:
        movzx ecx, bx
L_11ED2:
        mov eax, ecx
L_11ED4:
        shl ecx, 1
L_11ED6:
        add ecx, eax
L_11ED8:
        mov dword ptr [g_8357], ecx
L_11EDE:
        mov dword ptr [g_835b], esi
L_11EE4:
        add esi, ecx
L_11EE6:
        lodsb
L_11EE7:
        mov word ptr [g_8355], 906h
L_11EF0:
        cmp al, 2Ch
L_11EF2:
        jne near ptr L_12222
L_11EF8:
        lodsw
L_11EFA:
        lodsw
L_11EFC:
        lodsw
L_11EFE:
        cmp word ptr [g_8318], 0
L_11F06:
        jne short L_11F0E
L_11F08:
        mov word ptr [g_8318], ax
L_11F0E:
        mov word ptr [g_8355], 903h
L_11F17:
        cmp ax, 140h
L_11F1B:
        lodsw
L_11F1D:
        cmp word ptr [g_831a], 0
L_11F25:
        jne short L_11F2D
L_11F27:
        mov word ptr [g_831a], ax
L_11F2D:
        mov word ptr [g_8355], 904h
L_11F36:
        cmp ax, 0C8h
L_11F3A:
        lodsw
L_11F3C:
        mov word ptr [g_8355], 907h
L_11F45:
        or al, al
L_11F47:
        js near ptr L_12222
L_11F4D:
        mov word ptr [g_8355], 908h
L_11F56:
        bt ax, 6
L_11F5B:
        push edi
L_11F5C:
        push esi
L_11F5D:
        mov edi, esi
L_11F5F:
        xor eax, eax
L_11F61:
        mov ecx, eax
L_11F63:
        lodsb
L_11F64:
        or al, al
L_11F66:
        je short L_11F7C
L_11F68:
        mov cl, al
L_11F6A:
        shr cl, 1
L_11F6C:
        jae short L_11F6F
L_11F6E:
        movsb
L_11F6F:
        shr cl, 1
L_11F71:
        jae short L_11F75
L_11F73:
        movsw
L_11F75:
        rep movsd
L_11F77:
        lodsb
L_11F78:
        or al, al
L_11F7A:
        jne short L_11F68
L_11F7C:
        xor eax, eax
L_11F7E:
        mov ebx, eax
L_11F80:
        pop esi
L_11F81:
        pop edi
L_11F82:
        mov al, byte ptr [esi - 1]
L_11F85:
        mov ecx, eax
L_11F87:
        inc eax
L_11F88:
        mov word ptr [g_832c], ax
L_11F8E:
        mov word ptr [g_832e], ax
L_11F94:
        mov eax, 1
L_11F99:
        shl ax, cl
L_11F9C:
        mov word ptr [g_8349], ax
L_11FA2:
        inc ax
L_11FA4:
        mov word ptr [g_834b], ax
L_11FAA:
        inc ax
L_11FAC:
        mov word ptr [g_8330], ax
L_11FB2:
        mov word ptr [g_8332], ax
L_11FB8:
        mov eax, 1
L_11FBD:
        mov cx, word ptr [g_832c]
L_11FC4:
        shl ax, cl
L_11FC7:
        mov word ptr [g_8334], ax
L_11FCD:
        mov word ptr [g_8336], ax
L_11FD3:
        dec ax
L_11FD5:
        mov word ptr [g_8338], ax
L_11FDB:
        mov eax, 1
L_11FE0:
        mov cx, word ptr [g_831e]
L_11FE7:
        shl ax, cl
L_11FEA:
        dec ax
L_11FEC:
        mov word ptr [g_833a], ax
L_11FF2:
        mov dword ptr [g_833d], 0
L_11FFC:
        mov word ptr [g_8341], 0
L_12005:
        mov dword ptr [g_8351], esi
L_1200B:
        mov ebx, dword ptr [g_8320]
L_12011:
        mov edi, dword ptr [g_834d]
L_12017:
        mov word ptr [g_8355], 0
L_12020:
        push ebp
L_12021:
        mov edx, dword ptr [g_833d]
L_12027:
        mov cx, word ptr [g_8341]
L_1202E:
        mov esi, dword ptr [g_8351]
L_12034:
        cmp cx, word ptr [g_832c]
L_1203B:
        jge short L_12062
L_1203D:
        xor eax, eax
L_1203F:
        lodsb
L_12040:
        shl eax, cl
L_12042:
        add edx, eax
L_12044:
        add cx, 8
L_12048:
        cmp cx, word ptr [g_832c]
L_1204F:
        jge short L_1205C
L_12051:
        xor eax, eax
L_12053:
        lodsb
L_12054:
        shl eax, cl
L_12056:
        add edx, eax
L_12058:
        add cx, 8
L_1205C:
        mov dword ptr [g_8351], esi
L_12062:
        mov eax, edx
L_12064:
        and dx, word ptr [g_8338]
L_1206B:
        sub cx, word ptr [g_832c]
L_12072:
        mov word ptr [g_8341], cx
L_12079:
        mov cx, word ptr [g_832c]
L_12080:
        shr eax, cl
L_12082:
        mov dword ptr [g_833d], eax
L_12087:
        mov eax, edx
L_12089:
        cmp ax, word ptr [g_834b]
L_12090:
        je near ptr L_12221
L_12096:
        cmp ax, word ptr [g_8349]
L_1209D:
        jne near ptr L_1213C
L_120A3:
        mov word ptr [g_832c], 9
L_120AC:
        mov word ptr [g_8334], 200h
L_120B5:
        mov word ptr [g_8332], 102h
L_120BE:
        mov word ptr [g_8338], 1FFh
L_120C7:
        mov edx, dword ptr [g_833d]
L_120CD:
        mov cx, word ptr [g_8341]
L_120D4:
        cmp cx, word ptr [g_832c]
L_120DB:
        jge short L_12102
L_120DD:
        xor eax, eax
L_120DF:
        lodsb
L_120E0:
        shl eax, cl
L_120E2:
        add edx, eax
L_120E4:
        add cx, 8
L_120E8:
        cmp cx, word ptr [g_832c]
L_120EF:
        jge short L_120FC
L_120F1:
        xor eax, eax
L_120F3:
        lodsb
L_120F4:
        shl eax, cl
L_120F6:
        add edx, eax
L_120F8:
        add cx, 8
L_120FC:
        mov dword ptr [g_8351], esi
L_12102:
        mov eax, edx
L_12104:
        and dx, word ptr [g_8338]
L_1210B:
        sub cx, word ptr [g_832c]
L_12112:
        mov word ptr [g_8341], cx
L_12119:
        mov cx, word ptr [g_832c]
L_12120:
        shr eax, cl
L_12122:
        mov dword ptr [g_833d], eax
L_12127:
        mov word ptr [g_8343], dx
L_1212E:
        mov byte ptr [g_833c], dl
L_12134:
        mov al, dl
L_12136:
        stosb
L_12137:
        jmp near ptr L_12021
L_1213C:
        mov word ptr [g_8316], ax
L_12142:
        cmp ax, word ptr [g_8332]
L_12149:
        jl short L_1215A
L_1214B:
        mov ax, word ptr [g_8343]
L_12151:
        mov dl, byte ptr [g_833c]
L_12157:
        mov byte ptr [ebx], dl
L_12159:
        inc ebx
L_1215A:
        movzx ebp, word ptr [g_833a]
L_12161:
        cmp ax, bp
L_12164:
        jle short L_12186
L_12166:
        movzx eax, ax
L_12169:
        push ecx
L_1216A:
        mov ecx, dword ptr [g_8328]
L_12170:
        mov dl, byte ptr [eax + ecx]
L_12173:
        mov byte ptr [ebx], dl
L_12175:
        inc ebx
L_12176:
        mov ecx, dword ptr [g_8324]
L_1217C:
        mov ax, word ptr [ecx + eax*2]
L_12180:
        cmp ax, bp
L_12183:
        jg short L_1216A
L_12185:
        pop ecx
L_12186:
        and ax, bp
L_12189:
        mov byte ptr [g_833c], al
L_1218E:
        stosb
L_1218F:
        mov ebp, dword ptr [g_8320]
L_12195:
        cmp ebx, ebp
L_12197:
        je short L_121A6
L_12199:
        dec ebx
L_1219A:
        mov al, byte ptr [ebx]
L_1219C:
        stosb
L_1219D:
        cmp ebx, ebp
L_1219F:
        jne short L_12199
L_121A1:
        mov al, byte ptr [g_833c]
L_121A6:
        movzx edx, word ptr [g_8332]
L_121AD:
        push ecx
L_121AE:
        mov ecx, dword ptr [g_8328]
L_121B4:
        mov byte ptr [edx + ecx], al
L_121B7:
        mov ax, word ptr [g_8343]
L_121BD:
        mov ecx, dword ptr [g_8324]
L_121C3:
        mov word ptr [ecx + edx*2], ax
L_121C7:
        pop ecx
L_121C8:
        add edx, edx
L_121CA:
        mov ax, word ptr [g_8316]
L_121D0:
        mov word ptr [g_8343], ax
L_121D6:
        inc word ptr [g_8332]
L_121DD:
        mov ax, word ptr [g_8332]
L_121E3:
        cmp ax, word ptr [g_8334]
L_121EA:
        jne near ptr L_12021
L_121F0:
        cmp word ptr [g_832c], 0Ch
L_121F8:
        je near ptr L_12021
L_121FE:
        inc word ptr [g_832c]
L_12205:
        mov ax, word ptr [g_8334]
L_1220B:
        add ax, ax
L_1220E:
        mov word ptr [g_8334], ax
L_12214:
        dec ax
L_12216:
        mov word ptr [g_8338], ax
L_1221C:
        jmp near ptr L_12021
L_12221:
        pop ebp
L_12222:
        mov eax, edi
L_12224:
        sub eax, dword ptr [ebp + 0Ch]
L_12227:
        mov dword ptr [g_e2ac+10h], eax
L_1222C:
        and eax, 1
L_1222F:
        sub edi, eax
L_12231:
        sub dword ptr [g_e2ac+10h], eax
L_12237:
        mov ecx, dword ptr [g_8357]
L_1223D:
        add dword ptr [g_e2ac+10h], ecx
L_12243:
        mov esi, dword ptr [g_835b]
L_12249:
        mov ebx, dword ptr [ebp + 10h]
L_1224C:
        mov eax, dword ptr [ebp + 0Ch]
L_1224F:
        mov dword ptr [ebx], eax
L_12251:
        mov eax, edi
L_12253:
        mov dword ptr [ebx + 4], eax
L_12256:
        movzx eax, word ptr [g_8318]
L_1225D:
        mov dword ptr [ebx + 8], eax
L_12260:
        movzx eax, word ptr [g_831a]
L_12267:
        mov dword ptr [ebx + 0Ch], eax
L_1226A:
        mov dword ptr [ebx + 14h], 100h
L_12271:
        mov dword ptr [ebx + 10h], 3
L_12278:
        lodsb
L_12279:
        shr al, 2
L_1227C:
        stosb
L_1227D:
        loop L_12278
L_1227F:
        popad
L_12280:
        movzx eax, word ptr [g_8355]
L_12287:
        ret
_TEXT ENDS
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
        PUBLIC g_8314
g_8314	DW 0
        PUBLIC g_8316
g_8316	DW 0
        PUBLIC g_8318
g_8318	DW 0
        PUBLIC g_831a
g_831a	DW 0
        PUBLIC g_831c
g_831c	DB 0
        PUBLIC g_831d
g_831d	DB 0
        PUBLIC g_831e
g_831e	DW 0
        PUBLIC g_8320
g_8320 LABEL DWORD
        DB 1h, 0h, 0h, 0h
        PUBLIC g_8324
g_8324 LABEL DWORD
        DB 1h, 0h, 0h, 0h
        PUBLIC g_8328
g_8328 LABEL DWORD
        DB 1h, 0h, 0h, 0h
        PUBLIC g_832c
g_832c	DW 0
        PUBLIC g_832e
g_832e	DW 0
        PUBLIC g_8330
g_8330	DW 0
        PUBLIC g_8332
g_8332	DW 0
        PUBLIC g_8334
g_8334	DW 0
        PUBLIC g_8336
g_8336	DW 0
        PUBLIC g_8338
g_8338	DW 0
        PUBLIC g_833a
g_833a	DW 0
        PUBLIC g_833c
g_833c	DB 0
        PUBLIC g_833d
g_833d	DD 0
        PUBLIC g_8341
g_8341	DW 0
        PUBLIC g_8343
g_8343	DW 3 DUP (0)
        PUBLIC g_8349
g_8349	DW 0
        PUBLIC g_834b
g_834b	DW 0
        PUBLIC g_834d
g_834d	DD 0
        PUBLIC g_8351
g_8351	DD 0
        PUBLIC g_8355
g_8355	DW 0
        PUBLIC g_8357
g_8357	DD 0
        PUBLIC g_835b
g_835b LABEL DWORD
        DB 0h, 0h, 0h, 0h, 0h
_DATA ENDS
        END
