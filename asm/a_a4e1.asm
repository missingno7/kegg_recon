.386
EXTRN g_73e0:DWORD
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_a4e1
a_a4e1:
        push esi
L_0A4E2:
        push edi
L_0A4E3:
        xchg dl, dh
L_0A4E5:
        rol edx, 10h
L_0A4E8:
        xchg dl, dh
L_0A4EA:
        mov edi, dword ptr [g_73e0]
L_0A4F0:
        mov ecx, dword ptr [edi + 4]
L_0A4F3:
        xchg cl, ch
L_0A4F5:
        rol ecx, 10h
L_0A4F8:
        xchg cl, ch
L_0A4FA:
        lea esi, [edi + 0Ch]
L_0A4FD:
        add ecx, esi
L_0A4FF:
        lodsd
L_0A500:
        cmp eax, edx
L_0A502:
        je short L_0A519
L_0A504:
        lodsd
L_0A505:
        xchg al, ah
L_0A507:
        rol eax, 10h
L_0A50A:
        xchg al, ah
L_0A50C:
        add esi, eax
L_0A50E:
        inc esi
L_0A50F:
        and esi, 0FFFFFFFEh
L_0A512:
        cmp esi, ecx
L_0A514:
        jl short L_0A4FF
L_0A516:
        stc
L_0A517:
        jmp short L_0A51C
L_0A519:
        clc
L_0A51A:
        mov ebx, esi
L_0A51C:
        pop edi
L_0A51D:
        pop esi
L_0A51E:
        ret
ASM_TEXT ENDS
        END
