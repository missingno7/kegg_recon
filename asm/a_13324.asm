.386
EXTRN g_7b14:WORD
EXTRN g_e324:WORD
EXTRN g_e326:DWORD
EXTRN g_e336:DWORD
EXTRN g_e346:DWORD
EXTRN g_e35e:DWORD
EXTRN g_e36e:DWORD
EXTRN g_e372:DWORD
EXTRN g_e376:DWORD
EXTRN g_e37a:DWORD
EXTRN g_e386:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_13324
a_13324:
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
        cmp eax, dword ptr [g_e36e]
L_13337:
        jge short L_13341
L_13339:
        mov eax, dword ptr [g_e36e]
L_1333E:
        mov dword ptr [ebp + 8], eax
L_13341:
        cmp eax, dword ptr [g_e376]
L_13347:
        jle short L_13351
L_13349:
        mov eax, dword ptr [g_e376]
L_1334E:
        mov dword ptr [ebp + 8], eax
L_13351:
        cmp ebx, dword ptr [g_e372]
L_13357:
        jge short L_13362
L_13359:
        mov ebx, dword ptr [g_e372]
L_1335F:
        mov dword ptr [ebp + 0Ch], ebx
L_13362:
        cmp ebx, dword ptr [g_e37a]
L_13368:
        jle short L_13373
L_1336A:
        mov ebx, dword ptr [g_e37a]
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
        mov esi, dword ptr [ebx + g_e326]
L_1338D:
        add esi, dword ptr [ebx + g_e336]
L_13393:
        add esi, dword ptr [ebx + g_e346]
L_13399:
        mov eax, dword ptr [ebp + 0Ch]
L_1339C:
        mul dword ptr [g_e35e]
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
        mov byte ptr [g_e386], ah
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
        mov esi, dword ptr [ebx + g_e326]
L_133D6:
        add esi, dword ptr [ebx + g_e336]
L_133DC:
        add esi, dword ptr [ebx + g_e346]
L_133E2:
        mov eax, dword ptr [ebp + 0Ch]
L_133E5:
        mul dword ptr [g_e35e]
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
ASM_TEXT ENDS
        END
