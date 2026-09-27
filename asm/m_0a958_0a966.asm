.386
; Two empty handler stubs (same frame idiom as the neighbouring TASM modules); nothing calls them directly.
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        PUBLIC a_a958
a_a958 PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        popad
        ret
a_a958 ENDP
        PUBLIC a_a95f
a_a95f PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        popad
        ret
a_a95f ENDP
_TEXT ENDS
        END
