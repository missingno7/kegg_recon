.386
EXTRN g_e324:WORD
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC copy_screen_span
        ; Copy one linear framebuffer span, using planar or packed VGA access.
        PUBLIC copy_screen_span_entry
copy_screen_span_entry LABEL NEAR
copy_screen_span PROC NEAR
        pushad
L_12F9D:
        lea ebp, [esp + 1Ch]
L_12FA1:
        cmp dword ptr [ebp + 18h], 0
L_12FA5:
        jg short L_12FA9
L_12FA7:
        popad
L_12FA8:
        ret
L_12FA9:
        cmp word ptr [g_e324], 0
L_12FB1:
        je near ptr L_1303B
L_12FB7:
        cmp byte ptr [g_e324+61h], 0Fh
L_12FBE:
        je short L_12FD1
L_12FC0:
        mov byte ptr [g_e324+61h], 0Fh
L_12FC7:
        mov ax, 0F02h
L_12FCB:
        mov dx, 3C4h
L_12FCF:
        out dx, ax
L_12FD1:
        cmp word ptr [g_e324], 1
L_12FD9:
        jne short L_12FF5
L_12FDB:
        cmp byte ptr [g_e324+60h], 41h
L_12FE2:
        je short L_12FF5
L_12FE4:
        mov byte ptr [g_e324+60h], 41h
L_12FEB:
        mov ax, 4105h
L_12FEF:
        mov dx, 3CEh
L_12FF3:
        out dx, ax
L_12FF5:
        mov ecx, dword ptr [ebp + 18h]
L_12FF8:
        shr ecx, 2
L_12FFB:
        mov ebx, dword ptr [ebp + 8]
L_12FFE:
        shl ebx, 2
L_13001:
        mov esi, dword ptr [ebx + g_e324+2h]
L_13007:
        add esi, dword ptr [ebx + g_e324+12h]
L_1300D:
        add esi, dword ptr [ebx + g_e324+22h]
L_13013:
        add esi, dword ptr [ebp + 0Ch]
L_13016:
        mov ebx, dword ptr [ebp + 10h]
L_13019:
        shl ebx, 2
L_1301C:
        mov edi, dword ptr [ebx + g_e324+2h]
L_13022:
        add edi, dword ptr [ebx + g_e324+12h]
L_13028:
        add edi, dword ptr [ebx + g_e324+22h]
L_1302E:
        add edi, dword ptr [ebp + 14h]
L_13031:
        shr esi, 2
L_13034:
        shr edi, 2
L_13037:
        rep movsb byte ptr es:[edi], byte ptr [esi]
L_13039:
        popad
L_1303A:
        ret
L_1303B:
        cmp byte ptr [g_e324+61h], 0Fh
L_13042:
        je short L_13055
L_13044:
        mov byte ptr [g_e324+61h], 0Fh
L_1304B:
        mov ax, 0F02h
L_1304F:
        mov dx, 3C4h
L_13053:
        out dx, ax
L_13055:
        cmp byte ptr [g_e324+60h], 40h
L_1305C:
        je short L_1306F
L_1305E:
        mov byte ptr [g_e324+60h], 40h
L_13065:
        mov ax, 4005h
L_13069:
        mov dx, 3CEh
L_1306D:
        out dx, ax
L_1306F:
        mov ebx, dword ptr [ebp + 8]
L_13072:
        shl ebx, 2
L_13075:
        mov esi, dword ptr [ebx + g_e324+2h]
L_1307B:
        add esi, dword ptr [ebx + g_e324+12h]
L_13081:
        add esi, dword ptr [ebx + g_e324+22h]
L_13087:
        add esi, dword ptr [ebp + 0Ch]
L_1308A:
        mov ebx, dword ptr [ebp + 10h]
L_1308D:
        shl ebx, 2
L_13090:
        mov edi, dword ptr [ebx + g_e324+2h]
L_13096:
        add edi, dword ptr [ebx + g_e324+12h]
L_1309C:
        add edi, dword ptr [ebx + g_e324+22h]
L_130A2:
        add edi, dword ptr [ebp + 14h]
L_130A5:
        mov ecx, dword ptr [ebp + 18h]
L_130A8:
        shr ecx, 2
L_130AB:
        rep movsd dword ptr es:[edi], dword ptr [esi]
L_130AD:
        mov ecx, dword ptr [ebp + 18h]
L_130B0:
        and ecx, 3
L_130B3:
        rep movsb byte ptr es:[edi], byte ptr [esi]
L_130B5:
        popad
L_130B6:
        ret
        ; Clip source and destination bounds before copying a screen rectangle.
        PUBLIC copy_clipped_screen_rectangle
copy_clipped_screen_rectangle LABEL NEAR
L_130B7:
        push ebp
L_130B8:
        mov ebp, esp
L_130BA:
        sub esp, 14h
L_130BD:
        pushad
L_130BE:
        mov eax, dword ptr [ebp + 0Ch]
L_130C1:
        mov ebx, dword ptr [ebp + 14h]
L_130C4:
        mov ecx, dword ptr [ebp + 10h]
L_130C7:
        mov edx, dword ptr [ebp + 18h]
L_130CA:
        cmp eax, ebx
L_130CC:
        jle short L_130D5
L_130CE:
        xchg eax, ebx
L_130CF:
        mov dword ptr [ebp + 0Ch], eax
L_130D2:
        mov dword ptr [ebp + 14h], ebx
L_130D5:
        cmp ecx, edx
L_130D7:
        jle short L_130E1
L_130D9:
        xchg ecx, edx
L_130DB:
        mov dword ptr [ebp + 10h], ecx
L_130DE:
        mov dword ptr [ebp + 18h], edx
L_130E1:
        cmp eax, dword ptr [g_e324+4Ah]
L_130E7:
        jge short L_130F1
L_130E9:
        mov eax, dword ptr [g_e324+4Ah]
L_130EE:
        mov dword ptr [ebp + 0Ch], eax
L_130F1:
        cmp ebx, dword ptr [g_e324+52h]
L_130F7:
        jle short L_13102
L_130F9:
        mov ebx, dword ptr [g_e324+52h]
L_130FF:
        mov dword ptr [ebp + 14h], ebx
L_13102:
        cmp ecx, dword ptr [g_e324+4Eh]
L_13108:
        jge short L_13113
L_1310A:
        mov ecx, dword ptr [g_e324+4Eh]
L_13110:
        mov dword ptr [ebp + 10h], ecx
L_13113:
        cmp edx, dword ptr [g_e324+56h]
L_13119:
        jle short L_13124
L_1311B:
        mov edx, dword ptr [g_e324+56h]
L_13121:
        mov dword ptr [ebp + 18h], edx
L_13124:
        sub ebx, eax
L_13126:
        inc ebx
L_13127:
        mov dword ptr [ebp - 10h], ebx
L_1312A:
        sub edx, ecx
L_1312C:
        inc edx
L_1312D:
        mov dword ptr [ebp - 14h], edx
L_13130:
        mov eax, dword ptr [ebp + 20h]
L_13133:
        mov ebx, dword ptr [ebp + 24h]
L_13136:
        cmp eax, dword ptr [g_e324+4Ah]
L_1313C:
        jge short L_13146
L_1313E:
        mov eax, dword ptr [g_e324+4Ah]
L_13143:
        mov dword ptr [ebp + 20h], eax
L_13146:
        mov edx, dword ptr [ebp - 10h]
L_13149:
        add edx, eax
L_1314B:
        sub edx, dword ptr [g_e324+52h]
L_13151:
        jle short L_1316E
L_13153:
        mov ecx, dword ptr [g_e324+52h]
L_13159:
        sub ecx, eax
L_1315B:
        jl near ptr L_13270
L_13161:
        inc ecx
L_13162:
        mov edx, dword ptr [ebp - 10h]
L_13165:
        mov dword ptr [ebp - 10h], ecx
L_13168:
        sub dword ptr [ebp + 14h], edx
L_1316B:
        add dword ptr [ebp + 14h], ecx
L_1316E:
        cmp ebx, dword ptr [g_e324+4Eh]
L_13174:
        jge short L_1317F
L_13176:
        mov ebx, dword ptr [g_e324+4Eh]
L_1317C:
        mov dword ptr [ebp + 24h], ebx
L_1317F:
        mov edx, dword ptr [ebp - 14h]
L_13182:
        add edx, ebx
L_13184:
        sub edx, dword ptr [g_e324+56h]
L_1318A:
        jle short L_131A7
L_1318C:
        mov ecx, dword ptr [g_e324+56h]
L_13192:
        sub ecx, ebx
L_13194:
        jl near ptr L_13270
L_1319A:
        inc ecx
L_1319B:
        mov edx, dword ptr [ebp - 14h]
L_1319E:
        mov dword ptr [ebp - 14h], ecx
L_131A1:
        sub dword ptr [ebp + 18h], edx
L_131A4:
        add dword ptr [ebp + 18h], ecx
L_131A7:
        cmp word ptr [g_e324], 0
L_131AF:
        je near ptr L_13275
L_131B5:
        cmp byte ptr [g_e324+61h], 0Fh
L_131BC:
        je short L_131CF
L_131BE:
        mov byte ptr [g_e324+61h], 0Fh
L_131C5:
        mov ax, 0F02h
L_131C9:
        mov dx, 3C4h
L_131CD:
        out dx, ax
L_131CF:
        cmp word ptr [g_e324], 1
L_131D7:
        jne short L_131F3
L_131D9:
        cmp byte ptr [g_e324+60h], 41h
L_131E0:
        je short L_131F3
L_131E2:
        mov byte ptr [g_e324+60h], 41h
L_131E9:
        mov ax, 4105h
L_131ED:
        mov dx, 3CEh
L_131F1:
        out dx, ax
L_131F3:
        mov ecx, dword ptr [g_e324+3Ah]
L_131F9:
        mov eax, dword ptr [ebp + 10h]
L_131FC:
        mul ecx
L_131FE:
        mov esi, eax
L_13200:
        mov ebx, dword ptr [ebp + 8]
L_13203:
        shl ebx, 2
L_13206:
        add esi, dword ptr [ebx + g_e324+2h]
L_1320C:
        add esi, dword ptr [ebx + g_e324+12h]
L_13212:
        add esi, dword ptr [ebx + g_e324+22h]
L_13218:
        mov ebx, dword ptr [ebp + 1Ch]
L_1321B:
        shl ebx, 2
L_1321E:
        mov edi, dword ptr [ebx + g_e324+2h]
L_13224:
        add edi, dword ptr [ebx + g_e324+12h]
L_1322A:
        add edi, dword ptr [ebx + g_e324+22h]
L_13230:
        mov eax, dword ptr [ebp + 24h]
L_13233:
        mul ecx
L_13235:
        add eax, dword ptr [ebp + 20h]
L_13238:
        add edi, eax
L_1323A:
        shr edi, 2
L_1323D:
        mov edx, esi
L_1323F:
        and edx, 3
L_13242:
        add esi, dword ptr [ebp + 0Ch]
L_13245:
        shr esi, 2
L_13248:
        mov eax, dword ptr [ebp + 0Ch]
L_1324B:
        mov ebx, dword ptr [ebp + 14h]
L_1324E:
        add eax, edx
L_13250:
        add ebx, edx
L_13252:
        shr eax, 2
L_13255:
        shr ebx, 2
L_13258:
        sub ebx, eax
L_1325A:
        inc ebx
L_1325B:
        shr ecx, 2
L_1325E:
        mov eax, ecx
L_13260:
        sub eax, ebx
L_13262:
        mov edx, dword ptr [ebp - 14h]
L_13265:
        mov ecx, ebx
L_13267:
        rep movsb byte ptr es:[edi], byte ptr [esi]
L_13269:
        add esi, eax
L_1326B:
        add edi, eax
L_1326D:
        dec edx
L_1326E:
        jne short L_13265
L_13270:
        popad
L_13271:
        mov esp, ebp
L_13273:
        pop ebp
L_13274:
        ret
L_13275:
        cmp byte ptr [g_e324+61h], 0Fh
L_1327C:
        je short L_1328F
L_1327E:
        mov byte ptr [g_e324+61h], 0Fh
L_13285:
        mov ax, 0F02h
L_13289:
        mov dx, 3C4h
L_1328D:
        out dx, ax
L_1328F:
        cmp byte ptr [g_e324+60h], 40h
L_13296:
        je short L_132A9
L_13298:
        mov byte ptr [g_e324+60h], 40h
L_1329F:
        mov ax, 4005h
L_132A3:
        mov dx, 3CEh
L_132A7:
        out dx, ax
L_132A9:
        mov ecx, dword ptr [g_e324+3Ah]
L_132AF:
        mov eax, dword ptr [ebp + 10h]
L_132B2:
        mul ecx
L_132B4:
        mov esi, dword ptr [ebp + 0Ch]
L_132B7:
        add esi, eax
L_132B9:
        mov ebx, dword ptr [ebp + 8]
L_132BC:
        shl ebx, 2
L_132BF:
        add esi, dword ptr [ebx + g_e324+2h]
L_132C5:
        add esi, dword ptr [ebx + g_e324+12h]
L_132CB:
        add esi, dword ptr [ebx + g_e324+22h]
L_132D1:
        mov ebx, dword ptr [ebp + 1Ch]
L_132D4:
        shl ebx, 2
L_132D7:
        mov edi, dword ptr [ebx + g_e324+2h]
L_132DD:
        add edi, dword ptr [ebx + g_e324+12h]
L_132E3:
        add edi, dword ptr [ebx + g_e324+22h]
L_132E9:
        mov eax, dword ptr [ebp + 24h]
L_132EC:
        mul ecx
L_132EE:
        add eax, dword ptr [ebp + 20h]
L_132F1:
        add edi, eax
L_132F3:
        mov edx, dword ptr [ebp - 10h]
L_132F6:
        mov ebx, edx
L_132F8:
        shr edx, 2
L_132FB:
        mov dword ptr [ebp - 8], edx
L_132FE:
        and ebx, 3
L_13301:
        mov dword ptr [ebp - 0Ch], ebx
L_13304:
        sub ecx, dword ptr [ebp - 10h]
L_13307:
        mov eax, ecx
L_13309:
        mov ebx, dword ptr [ebp - 14h]
L_1330C:
        mov ecx, dword ptr [ebp - 8]
L_1330F:
        rep movsd dword ptr es:[edi], dword ptr [esi]
L_13311:
        mov ecx, dword ptr [ebp - 0Ch]
L_13314:
        rep movsb byte ptr es:[edi], byte ptr [esi]
L_13316:
        add esi, eax
L_13318:
        add edi, eax
L_1331A:
        dec ebx
L_1331B:
        jne short L_1330C
L_1331D:
        popad
L_1331E:
        mov esp, ebp
L_13320:
        pop ebp
L_13321:
        ret
L_13322:
        add byte ptr [eax], al
copy_screen_span ENDP
_TEXT ENDS
        END
