.386
EXTRN g_7b14:WORD
EXTRN g_e324:WORD
EXTRN g_7b16:WORD
EXTRN L_13712:NEAR
_TEXT SEGMENT BYTE PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC read_vga_pixel
        ; Read one palette-indexed pixel while preserving the caller's VGA mode.
        PUBLIC read_vga_pixel_entry
read_vga_pixel_entry LABEL NEAR
read_vga_pixel PROC NEAR
        push ebp
L_13325:
        lea ebp, [esp]
L_13328:
        push ebx
L_13329:
        push edx
L_1332A:
        push esi
L_1332B:
        mov eax, dword ptr [ebp + 8]
L_1332E:
        mov ebx, dword ptr [ebp + 0Ch]
L_13331:
        cmp eax, dword ptr [g_e324+4Ah]
L_13337:
        jge short L_13341
L_13339:
        mov eax, dword ptr [g_e324+4Ah]
L_1333E:
        mov dword ptr [ebp + 8], eax
L_13341:
        cmp eax, dword ptr [g_e324+52h]
L_13347:
        jle short L_13351
L_13349:
        mov eax, dword ptr [g_e324+52h]
L_1334E:
        mov dword ptr [ebp + 8], eax
L_13351:
        cmp ebx, dword ptr [g_e324+4Eh]
L_13357:
        jge short L_13362
L_13359:
        mov ebx, dword ptr [g_e324+4Eh]
L_1335F:
        mov dword ptr [ebp + 0Ch], ebx
L_13362:
        cmp ebx, dword ptr [g_e324+56h]
L_13368:
        jle short L_13373
L_1336A:
        mov ebx, dword ptr [g_e324+56h]
L_13370:
        mov dword ptr [ebp + 0Ch], ebx
L_13373:
        cmp word ptr [g_e324], 0
L_1337B:
        je short L_133C6
L_1337D:
        movzx ebx, word ptr [g_7b14]
L_13384:
        shl ebx, 2
L_13387:
        mov esi, dword ptr [ebx + g_e324+2h]
L_1338D:
        add esi, dword ptr [ebx + g_e324+12h]
L_13393:
        add esi, dword ptr [ebx + g_e324+22h]
L_13399:
        mov eax, dword ptr [ebp + 0Ch]
L_1339C:
        mul dword ptr [g_e324+3Ah]
L_133A2:
        add eax, dword ptr [ebp + 8]
L_133A5:
        add esi, eax
L_133A7:
        mov eax, esi
L_133A9:
        shr esi, 2
L_133AC:
        shl eax, 8
L_133AF:
        and ah, 3
L_133B2:
        mov byte ptr [g_e324+62h], ah
L_133B8:
        mov al, 4
L_133BA:
        mov dx, 3CEh
L_133BE:
        out dx, ax
L_133C0:
        lodsb
L_133C1:
        pop esi
L_133C2:
        pop edx
L_133C3:
        pop ebx
L_133C4:
        pop ebp
L_133C5:
        ret
L_133C6:
        movzx ebx, word ptr [g_7b14]
L_133CD:
        shl ebx, 2
L_133D0:
        mov esi, dword ptr [ebx + g_e324+2h]
L_133D6:
        add esi, dword ptr [ebx + g_e324+12h]
L_133DC:
        add esi, dword ptr [ebx + g_e324+22h]
L_133E2:
        mov eax, dword ptr [ebp + 0Ch]
L_133E5:
        mul dword ptr [g_e324+3Ah]
L_133EB:
        add eax, dword ptr [ebp + 8]
L_133EE:
        add esi, eax
L_133F0:
        lodsb
L_133F1:
        pop esi
L_133F2:
        pop edx
L_133F3:
        pop ebx
L_133F4:
        pop ebp
L_133F5:
        ret
read_vga_pixel ENDP
        ASSUME CS:_TEXT
        ASSUME CS:_TEXT
        PUBLIC write_vga_pixel
        ; Write one palette-indexed pixel, selecting its VGA plane when needed.
        PUBLIC write_vga_pixel_entry
write_vga_pixel_entry LABEL NEAR
write_vga_pixel PROC NEAR
        push ebp
L_133F7:
        lea ebp, [esp]
L_133FA:
        push eax
L_133FB:
        push ebx
L_133FC:
        push edx
L_133FD:
        push esi
L_133FE:
        mov eax, dword ptr [ebp + 8]
L_13401:
        mov ebx, dword ptr [ebp + 0Ch]
L_13404:
        cmp eax, dword ptr [g_e324+4Ah]
L_1340A:
        jl near ptr L_1349D
L_13410:
        cmp eax, dword ptr [g_e324+52h]
L_13416:
        jg near ptr L_1349D
L_1341C:
        cmp ebx, dword ptr [g_e324+4Eh]
L_13422:
        jl short L_1349D
L_13424:
        cmp ebx, dword ptr [g_e324+56h]
L_1342A:
        jg short L_1349D
L_1342C:
        cmp word ptr [g_e324], 0
L_13434:
        je short L_134A3
L_13436:
        cmp byte ptr [g_e324+60h], 40h
L_1343D:
        je short L_13450
L_1343F:
        mov byte ptr [g_e324+60h], 40h
L_13446:
        mov ax, 4005h
L_1344A:
        mov dx, 3CEh
L_1344E:
        out dx, ax
L_13450:
        movzx ebx, word ptr [g_7b16]
L_13457:
        shl ebx, 2
L_1345A:
        mov esi, dword ptr [ebx + g_e324+2h]
L_13460:
        add esi, dword ptr [ebx + g_e324+12h]
L_13466:
        add esi, dword ptr [ebx + g_e324+22h]
L_1346C:
        mov eax, dword ptr [ebp + 0Ch]
L_1346F:
        mul dword ptr [g_e324+3Ah]
L_13475:
        add eax, dword ptr [ebp + 8]
L_13478:
        add esi, eax
L_1347A:
        mov edx, ecx
L_1347C:
        mov ecx, esi
L_1347E:
        shr esi, 2
L_13481:
        and ecx, 3
L_13484:
        mov ah, 1
L_13486:
        rol ah, cl
L_13488:
        mov ecx, edx
L_1348A:
        mov byte ptr [g_e324+61h], ah
L_13490:
        mov al, 2
L_13492:
        mov dx, 3C4h
L_13496:
        out dx, ax
L_13498:
        mov eax, dword ptr [ebp + 10h]
L_1349B:
        mov byte ptr [esi], al
L_1349D:
        pop esi
L_1349E:
        pop edx
L_1349F:
        pop ebx
L_134A0:
        pop eax
L_134A1:
        pop ebp
L_134A2:
        ret
L_134A3:
        cmp byte ptr [g_e324+60h], 40h
L_134AA:
        je short L_134BD
L_134AC:
        mov byte ptr [g_e324+60h], 40h
L_134B3:
        mov ax, 4005h
L_134B7:
        mov dx, 3CEh
L_134BB:
        out dx, ax
L_134BD:
        movzx ebx, word ptr [g_7b16]
L_134C4:
        shl ebx, 2
L_134C7:
        mov esi, dword ptr [ebx + g_e324+2h]
L_134CD:
        add esi, dword ptr [ebx + g_e324+12h]
L_134D3:
        add esi, dword ptr [ebx + g_e324+22h]
L_134D9:
        mov eax, dword ptr [ebp + 0Ch]
L_134DC:
        mul dword ptr [g_e324+3Ah]
L_134E2:
        add eax, dword ptr [ebp + 8]
L_134E5:
        add esi, eax
L_134E7:
        mov eax, dword ptr [ebp + 10h]
L_134EA:
        mov byte ptr [esi], al
L_134EC:
        pop esi
L_134ED:
        pop edx
L_134EE:
        pop ebx
L_134EF:
        pop eax
L_134F0:
        pop ebp
L_134F1:
        ret
write_vga_pixel ENDP
        ASSUME CS:_TEXT
        PUBLIC fill_vga_span
        ; Fill a linear run of pixels on the selected page.
        PUBLIC fill_vga_span_entry
fill_vga_span_entry LABEL NEAR
fill_vga_span PROC NEAR
        pushad
L_134F3:
        lea ebp, [esp + 1Ch]
L_134F7:
        cmp dword ptr [ebp + 10h], 0
L_134FB:
        jg short L_134FF
L_134FD:
        popad
L_134FE:
        ret
L_134FF:
        cmp word ptr [g_e324], 0
L_13507:
        je short L_1357D
L_13509:
        cmp byte ptr [g_e324+61h], 0Fh
L_13510:
        je short L_13523
L_13512:
        mov byte ptr [g_e324+61h], 0Fh
L_13519:
        mov ax, 0F02h
L_1351D:
        mov dx, 3C4h
L_13521:
        out dx, ax
L_13523:
        cmp byte ptr [g_e324+60h], 40h
L_1352A:
        je short L_1353D
L_1352C:
        mov byte ptr [g_e324+60h], 40h
L_13533:
        mov ax, 4005h
L_13537:
        mov dx, 3CEh
L_1353B:
        out dx, ax
L_1353D:
        mov ecx, dword ptr [ebp + 10h]
L_13540:
        shr ecx, 4
L_13543:
        mov ebx, dword ptr [ebp + 8]
L_13546:
        shl ebx, 2
L_13549:
        mov edi, dword ptr [ebx + g_e324+2h]
L_1354F:
        add edi, dword ptr [ebx + g_e324+12h]
L_13555:
        add edi, dword ptr [ebx + g_e324+22h]
L_1355B:
        add edi, dword ptr [ebp + 0Ch]
L_1355E:
        shr edi, 2
L_13561:
        mov ebx, dword ptr [ebp + 14h]
L_13564:
        mov bh, bl
L_13566:
        mov eax, ebx
L_13568:
        rol eax, 10h
L_1356B:
        mov ax, bx
L_1356E:
        rep stosd
L_13570:
        mov ecx, dword ptr [ebp + 10h]
L_13573:
        shr ecx, 2
L_13576:
        and ecx, 3
L_13579:
        rep stosb
L_1357B:
        popad
L_1357C:
        ret
L_1357D:
        cmp byte ptr [g_e324+61h], 0Fh
L_13584:
        je short L_13597
L_13586:
        mov byte ptr [g_e324+61h], 0Fh
L_1358D:
        mov ax, 0F02h
L_13591:
        mov dx, 3C4h
L_13595:
        out dx, ax
L_13597:
        cmp byte ptr [g_e324+60h], 40h
L_1359E:
        je short L_135B1
L_135A0:
        mov byte ptr [g_e324+60h], 40h
L_135A7:
        mov ax, 4005h
L_135AB:
        mov dx, 3CEh
L_135AF:
        out dx, ax
L_135B1:
        mov ecx, dword ptr [ebp + 10h]
L_135B4:
        shr ecx, 2
L_135B7:
        mov ebx, dword ptr [ebp + 8]
L_135BA:
        shl ebx, 2
L_135BD:
        mov edi, dword ptr [ebx + g_e324+2h]
L_135C3:
        add edi, dword ptr [ebx + g_e324+12h]
L_135C9:
        add edi, dword ptr [ebx + g_e324+22h]
L_135CF:
        mov ebx, dword ptr [ebp + 14h]
L_135D2:
        mov bh, bl
L_135D4:
        mov eax, ebx
L_135D6:
        rol eax, 10h
L_135D9:
        mov ax, bx
L_135DC:
        rep stosd
L_135DE:
        mov ecx, dword ptr [ebp + 10h]
L_135E1:
        and ecx, 3
L_135E4:
        rep stosb
L_135E6:
        popad
L_135E7:
        ret
        ; Normalize and clip a rectangle, then fill it with one palette index.
        PUBLIC fill_clipped_vga_rectangle
fill_clipped_vga_rectangle LABEL NEAR
L_135E8:
        push ebp
L_135E9:
        mov ebp, esp
L_135EB:
        sub esp, 14h
L_135EE:
        pushad
L_135EF:
        mov eax, dword ptr [ebp + 0Ch]
L_135F2:
        mov ebx, dword ptr [ebp + 14h]
L_135F5:
        mov ecx, dword ptr [ebp + 10h]
L_135F8:
        mov edx, dword ptr [ebp + 18h]
L_135FB:
        cmp eax, ebx
L_135FD:
        jle short L_13606
L_135FF:
        xchg eax, ebx
L_13600:
        mov dword ptr [ebp + 0Ch], eax
L_13603:
        mov dword ptr [ebp + 14h], ebx
L_13606:
        cmp ecx, edx
L_13608:
        jle short L_13612
L_1360A:
        xchg ecx, edx
L_1360C:
        mov dword ptr [ebp + 10h], ecx
L_1360F:
        mov dword ptr [ebp + 18h], edx
L_13612:
        cmp eax, dword ptr [g_e324+4Ah]
L_13618:
        jge short L_13622
L_1361A:
        mov eax, dword ptr [g_e324+4Ah]
L_1361F:
        mov dword ptr [ebp + 0Ch], eax
L_13622:
        cmp ebx, dword ptr [g_e324+52h]
L_13628:
        jle short L_13633
L_1362A:
        mov ebx, dword ptr [g_e324+52h]
L_13630:
        mov dword ptr [ebp + 14h], ebx
L_13633:
        cmp ecx, dword ptr [g_e324+4Eh]
L_13639:
        jge short L_13644
L_1363B:
        mov ecx, dword ptr [g_e324+4Eh]
L_13641:
        mov dword ptr [ebp + 10h], ecx
L_13644:
        cmp edx, dword ptr [g_e324+56h]
L_1364A:
        jle short L_13655
L_1364C:
        mov edx, dword ptr [g_e324+56h]
L_13652:
        mov dword ptr [ebp + 18h], edx
L_13655:
        sub ebx, eax
L_13657:
        inc ebx
L_13658:
        mov dword ptr [ebp - 10h], ebx
L_1365B:
        sub edx, ecx
L_1365D:
        inc edx
L_1365E:
        mov dword ptr [ebp - 14h], edx
L_13661:
        cmp dword ptr [ebp - 10h], 0
L_13665:
        jle short L_1366D
L_13667:
        cmp dword ptr [ebp - 14h], 0
L_1366B:
        jg short L_13672
L_1366D:
        popad
L_1366E:
        mov esp, ebp
L_13670:
        pop ebp
L_13671:
        ret
L_13672:
        cmp word ptr [g_e324], 0
L_1367A:
        je near ptr L_13712
L_13680:
        cmp byte ptr [g_e324+61h], 0Fh
L_13687:
        je short L_1369A
L_13689:
        mov byte ptr [g_e324+61h], 0Fh
L_13690:
        mov ax, 0F02h
L_13694:
        mov dx, 3C4h
L_13698:
        out dx, ax
L_1369A:
        cmp byte ptr [g_e324+60h], 40h
L_136A1:
        je short L_136B4
L_136A3:
        mov byte ptr [g_e324+60h], 40h
L_136AA:
        mov ax, 4005h
L_136AE:
        mov dx, 3CEh
L_136B2:
        out dx, ax
L_136B4:
        mov ecx, dword ptr [g_e324+3Ah]
L_136BA:
        mov ebx, dword ptr [ebp + 8]
L_136BD:
        shl ebx, 2
L_136C0:
        mov edi, dword ptr [ebx + g_e324+2h]
L_136C6:
        add edi, dword ptr [ebx + g_e324+12h]
L_136CC:
        add edi, dword ptr [ebx + g_e324+22h]
L_136D2:
        mov eax, dword ptr [ebp + 10h]
L_136D5:
        mul ecx
L_136D7:
        add edi, eax
L_136D9:
        mov edx, edi
L_136DB:
        and edx, 3
L_136DE:
        add edi, dword ptr [ebp + 0Ch]
L_136E1:
        shr edi, 2
L_136E4:
        mov eax, dword ptr [ebp + 0Ch]
L_136E7:
        mov ebx, dword ptr [ebp + 14h]
L_136EA:
        add eax, edx
L_136EC:
        add ebx, edx
L_136EE:
        shr eax, 2
L_136F1:
        shr ebx, 2
L_136F4:
        sub ebx, eax
L_136F6:
        inc ebx
L_136F7:
        shr ecx, 2
L_136FA:
        mov esi, ecx
L_136FC:
        sub esi, ebx
L_136FE:
        mov edx, dword ptr [ebp - 14h]
L_13701:
        mov eax, dword ptr [ebp + 1Ch]
L_13704:
        mov ecx, ebx
L_13706:
        rep stosb
L_13708:
        add edi, esi
L_1370A:
        dec edx
L_1370B:
        jne short L_13704
L_1370D:
        popad
L_1370E:
        mov esp, ebp
L_13710:
        pop ebp
L_13711:
        ret
fill_vga_span ENDP
_TEXT ENDS
END
