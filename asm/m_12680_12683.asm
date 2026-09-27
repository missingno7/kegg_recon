.386
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        ; These dispatch slots intentionally return without changing video state.
        PUBLIC noop_sprite_callback_12680
noop_sprite_callback_12680 LABEL NEAR
noop_sprite_callbacks_12680 PROC NEAR
        ret
        PUBLIC noop_sprite_callback_12681
noop_sprite_callback_12681 LABEL NEAR
L_12681:
        ret
        PUBLIC noop_sprite_callback_12682
noop_sprite_callback_12682 LABEL NEAR
L_12682:
        ret
noop_sprite_callbacks_12680 ENDP
_TEXT ENDS
        END
