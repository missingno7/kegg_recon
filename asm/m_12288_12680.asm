.386
DGROUP GROUP _DATA
_DATA SEGMENT BYTE PUBLIC USE32 'DATA'
EXTRN sprite_record_cursor:BYTE
EXTRN vga_row_advance:DWORD
EXTRN background_plane_delta:DWORD
EXTRN sprite_source_column:WORD
EXTRN sprite_clip_top:DWORD
EXTRN sprite_clip_left:DWORD
EXTRN visible_sprite_width:DWORD
EXTRN vga_plane_index:DWORD
EXTRN sprite_row_width_remaining:DWORD
EXTRN current_vga_plane_mask:BYTE
EXTRN first_vga_plane_mask:BYTE
EXTRN render_page_base:DWORD
EXTRN vga_state:WORD
EXTRN screen_page_base:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ; Kind 5 renderer: record the sprite operation, then decode its clipped run stream.
        PUBLIC render_sprite_record_kind_5_entry
render_sprite_record_kind_5_entry LABEL NEAR
render_sprite_record_kind_5 PROC NEAR
        mov edx, dword ptr [sprite_record_cursor]
L_1228E:
        add dword ptr [sprite_record_cursor], 14h
L_12295:
        mov word ptr [edx], 5
L_1229A:
        mov word ptr [edx + 4], bx
L_1229E:
        mov word ptr [edx + 2], bp
L_122A2:
        movzx eax, word ptr [esi - 2]
L_122A6:
        add eax, esi
L_122A8:
        mov dword ptr [edx + 6], eax
L_122AB:
        mov eax, edi
L_122AD:
        sub eax, dword ptr [render_page_base]
L_122B3:
        mov dword ptr [edx + 0Ah], eax
L_122B6:
        mov eax, dword ptr [sprite_clip_left]
L_122BB:
        mov word ptr [edx + 0Eh], ax
L_122BF:
        mov eax, dword ptr [visible_sprite_width]
L_122C4:
        mov word ptr [edx + 10h], ax
L_122C8:
        mov eax, dword ptr [sprite_clip_top]
L_122CD:
        mov word ptr [edx + 12h], ax
        ; Shared kind 5 drawing entry used by the mode dispatch table.
        PUBLIC render_sprite_record_kind_5_draw
render_sprite_record_kind_5_draw LABEL NEAR
L_122D1:
        mov ecx, edi
L_122D3:
        shr edi, 2
L_122D6:
        and cl, 3
L_122D9:
        mov ch, 11h
L_122DB:
        rol ch, cl
L_122DD:
        mov byte ptr [current_vga_plane_mask], ch
L_122E3:
        mov byte ptr [first_vga_plane_mask], ch
L_122E9:
        neg ebp
L_122EB:
        cmp dword ptr [sprite_clip_left], 0
L_122F2:
        jne near ptr L_123B9
L_122F8:
        cmp dword ptr [visible_sprite_width], 0
L_122FF:
        jne near ptr L_124B8
L_12305:
        mov word ptr [sprite_source_column], 0Ah
L_1230E:
        push ebx
L_1230F:
        push ebp
L_12310:
        push esi
L_12311:
        push edi
L_12312:
        mov al, 2
L_12314:
        mov ah, byte ptr [current_vga_plane_mask]
L_1231A:
        mov dx, 3C4h
L_1231E:
        out dx, ax
L_12320:
        movzx eax, word ptr [sprite_source_column]
L_12327:
        sub esi, eax
L_12329:
        add ax, word ptr [esi]
L_1232C:
        add esi, eax
L_1232E:
        add word ptr [sprite_source_column], -2
L_12336:
        sub eax, eax
L_12338:
        mov ecx, dword ptr [sprite_clip_top]
L_1233E:
        jcxz L_12351
L_12341:
        mov dl, byte ptr [esi]
L_12343:
        inc esi
L_12344:
        lodsb
L_12345:
        or al, al
L_12347:
        jl short L_1234B
L_12349:
        add esi, eax
L_1234B:
        dec dl
L_1234D:
        jne short L_12344
L_1234F:
        loop L_12341
L_12351:
        mov edx, dword ptr [vga_state+3Ah]
L_12357:
        sub edx, ebx
L_12359:
        shr edx, 2
L_1235C:
        mov dword ptr [vga_row_advance], edx
L_12362:
        mov bh, byte ptr [esi]
L_12364:
        neg bh
L_12366:
        inc esi
L_12367:
        mov cl, byte ptr [esi]
L_12369:
        inc esi
L_1236A:
        or cl, cl
L_1236C:
        jl short L_1238B
L_1236E:
        test edi, 1
L_12374:
        je short L_12378
L_12376:
        movsb
L_12377:
        dec ecx
L_12378:
        shr ecx, 1
L_1237A:
        rep movsw
L_1237D:
        jae short L_12380
L_1237F:
        movsb
L_12380:
        inc bh
L_12382:
        jne short L_12367
L_12384:
        add edi, edx
L_12386:
        inc ebp
L_12387:
        jne short L_12362
L_12389:
        jmp short L_12398
L_1238B:
        neg cl
L_1238D:
        add edi, ecx
L_1238F:
        inc bh
L_12391:
        jne short L_12367
L_12393:
        add edi, edx
L_12395:
        inc ebp
L_12396:
        jne short L_12362
L_12398:
        pop edi
L_12399:
        pop esi
L_1239A:
        pop ebp
L_1239B:
        pop ebx
L_1239C:
        dec ebx
L_1239D:
        rol byte ptr [current_vga_plane_mask], 1
L_123A3:
        adc edi, 0
L_123A6:
        mov dl, byte ptr [current_vga_plane_mask]
L_123AC:
        cmp dl, byte ptr [first_vga_plane_mask]
L_123B2:
        jne near ptr L_1230E
L_123B8:
        ret
L_123B9:
        mov edx, dword ptr [sprite_clip_left]
L_123BF:
        and edx, 3
L_123C2:
        mov dword ptr [vga_plane_index], edx
L_123C8:
        sar dword ptr [sprite_clip_left], 2
L_123CF:
        mov edx, dword ptr [vga_state+3Ah]
L_123D5:
        shr edx, 2
L_123D8:
        mov dword ptr [vga_row_advance], edx
L_123DE:
        push ebx
L_123DF:
        push ebp
L_123E0:
        push esi
L_123E1:
        push edi
L_123E2:
        mov al, 2
L_123E4:
        mov ah, byte ptr [current_vga_plane_mask]
L_123EA:
        mov dx, 3C4h
L_123EE:
        out dx, ax
L_123F0:
        mov edx, 0FFFFFFFBh
L_123F5:
        add edx, dword ptr [vga_plane_index]
L_123FB:
        add edx, edx
L_123FD:
        movzx edx, word ptr [esi + edx]
L_12401:
        add esi, edx
L_12403:
        sub eax, eax
L_12405:
        mov ecx, dword ptr [sprite_clip_top]
L_1240B:
        jcxz L_1241E
L_1240E:
        mov dl, byte ptr [esi]
L_12410:
        inc esi
L_12411:
        lodsb
L_12412:
        or al, al
L_12414:
        jl short L_12418
L_12416:
        add esi, eax
L_12418:
        dec dl
L_1241A:
        jne short L_12411
L_1241C:
        loop L_1240E
L_1241E:
        mov edx, edi
L_12420:
        mov edi, edx
L_12422:
        mov ebx, dword ptr [sprite_clip_left]
L_12428:
        sub edi, ebx
L_1242A:
        mov bh, byte ptr [esi]
L_1242C:
        neg bh
L_1242E:
        inc esi
L_1242F:
        mov cl, byte ptr [esi]
L_12431:
        inc esi
L_12432:
        or cl, cl
L_12434:
        jl short L_12468
L_12436:
        mov eax, edi
L_12438:
        sub eax, edx
L_1243A:
        jge short L_1244E
L_1243C:
        add eax, ecx
L_1243E:
        jg short L_12446
L_12440:
        add esi, ecx
L_12442:
        add edi, ecx
L_12444:
        jmp short L_12460
L_12446:
        sub ecx, eax
L_12448:
        add esi, ecx
L_1244A:
        add edi, ecx
L_1244C:
        mov ecx, eax
L_1244E:
        test edi, 1
L_12454:
        je short L_12458
L_12456:
        movsb
L_12457:
        dec ecx
L_12458:
        shr ecx, 1
L_1245A:
        rep movsw
L_1245D:
        jae short L_12460
L_1245F:
        movsb
L_12460:
        sub eax, eax
L_12462:
        inc bh
L_12464:
        jne short L_1242F
L_12466:
        jmp short L_12470
L_12468:
        neg cl
L_1246A:
        add edi, ecx
L_1246C:
        inc bh
L_1246E:
        jne short L_1242F
L_12470:
        add edx, dword ptr [vga_row_advance]
L_12476:
        inc ebp
L_12477:
        jne short L_12420
L_12479:
        inc dword ptr [vga_plane_index]
L_1247F:
        cmp dword ptr [vga_plane_index], 4
L_12486:
        jne short L_12498
L_12488:
        mov dword ptr [vga_plane_index], 0
L_12492:
        inc dword ptr [sprite_clip_left]
L_12498:
        pop edi
L_12499:
        pop esi
L_1249A:
        pop ebp
L_1249B:
        pop ebx
L_1249C:
        rol byte ptr [current_vga_plane_mask], 1
L_124A2:
        adc edi, 0
L_124A5:
        mov dl, byte ptr [current_vga_plane_mask]
L_124AB:
        cmp dl, byte ptr [first_vga_plane_mask]
L_124B1:
        jne near ptr L_123DE
L_124B7:
        ret
L_124B8:
        mov word ptr [sprite_source_column], 0Ah
L_124C1:
        mov edx, dword ptr [visible_sprite_width]
L_124C7:
        mov dword ptr [sprite_row_width_remaining], edx
L_124CD:
        mov edx, dword ptr [vga_state+3Ah]
L_124D3:
        shr edx, 2
L_124D6:
        mov dword ptr [vga_row_advance], edx
L_124DC:
        push ebx
L_124DD:
        push ebp
L_124DE:
        push esi
L_124DF:
        push edi
L_124E0:
        mov al, 2
L_124E2:
        mov ah, byte ptr [current_vga_plane_mask]
L_124E8:
        mov dx, 3C4h
L_124EC:
        out dx, ax
L_124EE:
        movzx eax, word ptr [sprite_source_column]
L_124F5:
        sub esi, eax
L_124F7:
        add ax, word ptr [esi]
L_124FA:
        add esi, eax
L_124FC:
        add word ptr [sprite_source_column], -2
L_12504:
        sub eax, eax
L_12506:
        mov ecx, dword ptr [sprite_clip_top]
L_1250C:
        jcxz L_1251F
L_1250F:
        mov dl, byte ptr [esi]
L_12511:
        inc esi
L_12512:
        lodsb
L_12513:
        or al, al
L_12515:
        jl short L_12519
L_12517:
        add esi, eax
L_12519:
        dec dl
L_1251B:
        jne short L_12512
L_1251D:
        loop L_1250F
L_1251F:
        mov edx, dword ptr [sprite_row_width_remaining]
L_12525:
        add edx, 3
L_12528:
        sar edx, 2
L_1252B:
        mov dword ptr [visible_sprite_width], edx
L_12531:
        dec dword ptr [sprite_row_width_remaining]
L_12537:
        mov edx, edi
L_12539:
        mov edi, edx
L_1253B:
        mov bh, byte ptr [esi]
L_1253D:
        neg bh
L_1253F:
        inc esi
L_12540:
        mov cl, byte ptr [esi]
L_12542:
        inc esi
L_12543:
        or cl, cl
L_12545:
        jl short L_1257D
L_12547:
        mov eax, edi
L_12549:
        sub eax, edx
L_1254B:
        sub eax, dword ptr [visible_sprite_width]
L_12551:
        jge short L_1255B
L_12553:
        add eax, ecx
L_12555:
        jl short L_1255F
L_12557:
        sub ecx, eax
L_12559:
        jmp short L_12561
L_1255B:
        add esi, ecx
L_1255D:
        jmp short L_12575
L_1255F:
        sub eax, eax
L_12561:
        test edi, 1
L_12567:
        je short L_1256B
L_12569:
        movsb
L_1256A:
        dec ecx
L_1256B:
        shr ecx, 1
L_1256D:
        rep movsw
L_12570:
        jae short L_12573
L_12572:
        movsb
L_12573:
        add esi, eax
L_12575:
        sub eax, eax
L_12577:
        inc bh
L_12579:
        jne short L_12540
L_1257B:
        jmp short L_12585
L_1257D:
        neg cl
L_1257F:
        add edi, ecx
L_12581:
        inc bh
L_12583:
        jne short L_12540
L_12585:
        add edx, dword ptr [vga_row_advance]
L_1258B:
        inc ebp
L_1258C:
        jne short L_12539
L_1258E:
        pop edi
L_1258F:
        pop esi
L_12590:
        pop ebp
L_12591:
        pop ebx
L_12592:
        rol byte ptr [current_vga_plane_mask], 1
L_12598:
        adc edi, 0
L_1259B:
        mov dl, byte ptr [current_vga_plane_mask]
L_125A1:
        cmp dl, byte ptr [first_vga_plane_mask]
L_125A7:
        jne near ptr L_124DC
L_125AD:
        ret
        ; Restore a saved rectangle or replay its encoded source, according to record flags.
        PUBLIC restore_sprite_background_record
restore_sprite_background_record LABEL NEAR
L_125AE:
        mov cx, word ptr [ebx + 0Eh]
L_125B2:
        or cx, word ptr [ebx + 10h]
L_125B6:
        jcxz L_125FF
L_125B9:
        mov esi, dword ptr [screen_page_base]
L_125BF:
        mov edi, dword ptr [render_page_base]
L_125C5:
        mov eax, dword ptr [ebx + 0Ah]
L_125C8:
        add esi, eax
L_125CA:
        add edi, eax
L_125CC:
        mov edx, edi
L_125CE:
        and edx, 3
L_125D1:
        movzx eax, word ptr [ebx + 4]
L_125D5:
        add edx, eax
L_125D7:
        add edx, 3
L_125DA:
        shr edx, 2
L_125DD:
        mov eax, dword ptr [vga_state+3Ah]
L_125E2:
        shr eax, 2
L_125E5:
        sub eax, edx
L_125E7:
        shr esi, 2
L_125EA:
        shr edi, 2
L_125ED:
        movzx ebp, word ptr [ebx + 2]
L_125F1:
        neg ebp
L_125F3:
        mov ecx, edx
L_125F5:
        rep movsb
L_125F7:
        add esi, eax
L_125F9:
        add edi, eax
L_125FB:
        inc ebp
L_125FC:
        jne short L_125F3
L_125FE:
        ret
L_125FF:
        mov eax, dword ptr [render_page_base]
L_12604:
        mov edi, dword ptr [screen_page_base]
L_1260A:
        sub edi, eax
L_1260C:
        mov dword ptr [background_plane_delta], edi
L_12612:
        mov ebp, dword ptr [ebx + 0Ah]
L_12615:
        add ebp, eax
L_12617:
        mov dx, word ptr [ebx + 2]
L_1261B:
        neg dl
L_1261D:
        sub eax, eax
L_1261F:
        mov esi, dword ptr [ebx + 6]
L_12622:
        movzx ecx, word ptr [ebx + 12h]
L_12626:
        jcxz L_1262E
L_12629:
        lodsb
L_1262A:
        add esi, eax
L_1262C:
        loop L_12629
L_1262E:
        mov ebx, esi
L_12630:
        mov ecx, ebp
L_12632:
        mov dh, byte ptr [ebx]
L_12634:
        neg dh
L_12636:
        inc ebx
L_12637:
        mov al, byte ptr [ebx]
L_12639:
        inc ebx
L_1263A:
        or al, al
L_1263C:
        jl short L_1266D
L_1263E:
        mov edi, ecx
L_12640:
        and ecx, 3
L_12643:
        add ecx, eax
L_12645:
        add ecx, 3
L_12648:
        shr ecx, 2
L_1264B:
        add eax, edi
L_1264D:
        mov esi, dword ptr [background_plane_delta]
L_12653:
        add esi, edi
L_12655:
        shr esi, 2
L_12658:
        shr edi, 2
L_1265B:
        rep movsb
L_1265D:
        xchg eax, ecx
L_1265E:
        inc dh
L_12660:
        jne short L_12637
L_12662:
        add ebp, dword ptr [vga_state+3Ah]
L_12668:
        inc dl
L_1266A:
        jne short L_12630
L_1266C:
        ret
L_1266D:
        neg al
L_1266F:
        add ecx, eax
L_12671:
        inc dh
L_12673:
        jne short L_12637
L_12675:
        add ebp, dword ptr [vga_state+3Ah]
L_1267B:
        inc dl
L_1267D:
        jne short L_12630
L_1267F:
        ret
render_sprite_record_kind_5 ENDP
_TEXT ENDS
        END
