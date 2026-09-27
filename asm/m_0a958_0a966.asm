.386
; Empty renderer hooks retained as callable entry points by the original program.
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        PUBLIC noop_renderer_hook_one
noop_renderer_hook_one PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        popad
        ret
noop_renderer_hook_one ENDP
        PUBLIC noop_renderer_hook_two
noop_renderer_hook_two PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        popad
        ret
noop_renderer_hook_two ENDP
_TEXT ENDS
        END
