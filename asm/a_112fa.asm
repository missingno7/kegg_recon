.386
EXTRN g_7487:BYTE
EXTRN g_7db0:WORD
EXTRN g_7db3:BYTE
EXTRN g_7dba:BYTE
EXTRN g_7dbb:BYTE
EXTRN f_11420:NEAR
EXTRN f_11494:NEAR
EXTRN f_114a0:NEAR
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_112fa
a_112fa:
        push eax
L_112FB:
        mov byte ptr [g_7db3], 14h
L_11302:
        call f_11420
L_11307:
        mov ax, word ptr [g_7db0]
L_1130D:
        dec ax
L_1130F:
        mov byte ptr [g_7db3], al
L_11314:
        call f_11420
L_11319:
        mov byte ptr [g_7db3], ah
L_1131F:
        call f_11420
L_11324:
        pop eax
L_11325:
        ret
L_11326:
        push eax
L_11327:
        mov byte ptr [g_7dba], 48h
L_1132E:
        mov al, byte ptr [g_7487]
L_11333:
        mov byte ptr [g_7dbb], al
L_11338:
        call f_114a0
L_1133D:
        pop eax
L_1133E:
        ret
L_1133F:
        push eax
L_11340:
        mov byte ptr [g_7dba], 58h
L_11347:
        mov al, byte ptr [g_7487]
L_1134C:
        mov byte ptr [g_7dbb], al
L_11351:
        call f_114a0
L_11356:
        pop eax
L_11357:
        ret
L_11358:
        mov byte ptr [g_7db3], 0D0h
L_1135F:
        call f_11420
L_11364:
        ret
L_11365:
        push eax
L_11366:
        mov al, byte ptr [g_7487]
L_1136B:
        mov byte ptr [g_7dbb], al
L_11370:
        call f_11494
L_11375:
        pop eax
L_11376:
        ret
ASM_TEXT ENDS
        END
