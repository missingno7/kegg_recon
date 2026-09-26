.386
EXTRN g_7b16:WORD
EXTRN g_e324:WORD
EXTRN g_e326:DWORD
EXTRN g_e336:DWORD
EXTRN g_e346:DWORD
EXTRN g_e35e:DWORD
EXTRN g_e36e:DWORD
EXTRN g_e372:DWORD
EXTRN g_e376:DWORD
EXTRN g_e37a:DWORD
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_133f6
a_133f6:
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
        cmp eax, dword ptr [g_e36e]
L_1340A:
        jl near ptr L_1349D
L_13410:
        cmp eax, dword ptr [g_e376]
L_13416:
        jg near ptr L_1349D
L_1341C:
        cmp ebx, dword ptr [g_e372]
L_13422:
        jl short L_1349D
L_13424:
        cmp ebx, dword ptr [g_e37a]
L_1342A:
        jg short L_1349D
L_1342C:
        cmp word ptr [g_e324], 0
L_13434:
        je short L_134A3
L_13436:
        cmp byte ptr [g_e384], 40h
L_1343D:
        je short L_13450
L_1343F:
        mov byte ptr [g_e384], 40h
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
        mov esi, dword ptr [ebx + g_e326]
L_13460:
        add esi, dword ptr [ebx + g_e336]
L_13466:
        add esi, dword ptr [ebx + g_e346]
L_1346C:
        mov eax, dword ptr [ebp + 0Ch]
L_1346F:
        mul dword ptr [g_e35e]
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
        mov byte ptr [g_e385], ah
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
        cmp byte ptr [g_e384], 40h
L_134AA:
        je short L_134BD
L_134AC:
        mov byte ptr [g_e384], 40h
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
        mov esi, dword ptr [ebx + g_e326]
L_134CD:
        add esi, dword ptr [ebx + g_e336]
L_134D3:
        add esi, dword ptr [ebx + g_e346]
L_134D9:
        mov eax, dword ptr [ebp + 0Ch]
L_134DC:
        mul dword ptr [g_e35e]
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
ASM_TEXT ENDS
        END
