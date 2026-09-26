.386
EXTRN g_746e:WORD
EXTRN g_7b16:WORD
EXTRN g_7b18:WORD
EXTRN g_8360:DWORD
EXTRN g_83a2:DWORD
EXTRN g_e2e0:DWORD
EXTRN g_e2e4:DWORD
EXTRN g_e2e8:DWORD
EXTRN g_e324:WORD
EXTRN g_e326:DWORD
EXTRN g_e336:DWORD
EXTRN g_e346:DWORD
EXTRN g_e384:BYTE
EXTRN g_e385:BYTE
ASM_TEXT SEGMENT PARA PUBLIC USE32 'CODE'
        ASSUME CS:ASM_TEXT
        PUBLIC a_12b94
a_12b94:
        pushad
L_12B95:
        lea ebp, [esp + 1Ch]
L_12B99:
        movzx ebx, word ptr [g_7b16]
L_12BA0:
        shl ebx, 2
L_12BA3:
        mov edi, dword ptr [ebx + g_e326]
L_12BA9:
        add edi, dword ptr [ebx + g_e336]
L_12BAF:
        add edi, dword ptr [ebx + g_e346]
L_12BB5:
        movzx ebx, word ptr [g_7b18]
L_12BBC:
        shl ebx, 2
L_12BBF:
        mov esi, dword ptr [ebx + g_e326]
L_12BC5:
        add esi, dword ptr [ebx + g_e336]
L_12BCB:
        add esi, dword ptr [ebx + g_e346]
L_12BD1:
        cmp word ptr [g_e324], 1
L_12BD9:
        jne short L_12C1B
L_12BDB:
        cmp byte ptr [g_e385], 0Fh
L_12BE2:
        je short L_12BF5
L_12BE4:
        mov byte ptr [g_e385], 0Fh
L_12BEB:
        mov ax, 0F02h
L_12BEF:
        mov dx, 3C4h
L_12BF3:
        out dx, ax
L_12BF5:
        cmp word ptr [g_e324], 1
L_12BFD:
        jne short L_12C19
L_12BFF:
        cmp byte ptr [g_e384], 41h
L_12C06:
        je short L_12C19
L_12C08:
        mov byte ptr [g_e384], 41h
L_12C0F:
        mov ax, 4105h
L_12C13:
        mov dx, 3CEh
L_12C17:
        out dx, ax
L_12C19:
        jmp short L_12C4F
L_12C1B:
        cmp byte ptr [g_e385], 0Fh
L_12C22:
        je short L_12C35
L_12C24:
        mov byte ptr [g_e385], 0Fh
L_12C2B:
        mov ax, 0F02h
L_12C2F:
        mov dx, 3C4h
L_12C33:
        out dx, ax
L_12C35:
        cmp byte ptr [g_e384], 40h
L_12C3C:
        je short L_12C4F
L_12C3E:
        mov byte ptr [g_e384], 40h
L_12C45:
        mov ax, 4005h
L_12C49:
        mov dx, 3CEh
L_12C4D:
        out dx, ax
L_12C4F:
        mov dword ptr [g_e2e4], edi
L_12C55:
        mov dword ptr [g_e2e0], esi
L_12C5B:
        cmp word ptr [g_746e], 4
L_12C63:
        jne short L_12C8F
L_12C65:
        mov ebx, dword ptr [g_e2e8]
L_12C6B:
        mov dword ptr [g_8360], ebx
L_12C71:
        jmp short L_12C80
L_12C73:
        call dword ptr [ecx*4 + g_83a2]
L_12C7A:
        mov ebx, dword ptr [g_8360]
L_12C80:
        add dword ptr [g_8360], 14h
L_12C87:
        movzx ecx, word ptr [ebx]
L_12C8A:
        cmp ecx, 0
L_12C8D:
        jne short L_12C73
L_12C8F:
        cmp word ptr [g_e324], 0
L_12C97:
        je short L_12CBB
L_12C99:
        mov byte ptr [g_e385], 0Fh
L_12CA0:
        mov ax, 0F02h
L_12CA4:
        mov dx, 3C4h
L_12CA8:
        out dx, ax
L_12CAA:
        mov byte ptr [g_e384], 40h
L_12CB1:
        mov ax, 4005h
L_12CB5:
        mov dx, 3CEh
L_12CB9:
        out dx, ax
L_12CBB:
        popad
L_12CBC:
        ret
ASM_TEXT ENDS
        END
