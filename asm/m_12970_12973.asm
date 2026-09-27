.386
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT
        ; Empty renderer callbacks occupy unused slots in the sprite mode table.
        PUBLIC noop_sprite_callback_12970
noop_sprite_callback_12970 LABEL NEAR
noop_sprite_callbacks_12970 PROC NEAR
        ret
        PUBLIC noop_sprite_callback_12971
noop_sprite_callback_12971 LABEL NEAR
L_12971:
        ret
        PUBLIC noop_sprite_callback_12972
noop_sprite_callback_12972 LABEL NEAR
L_12972:
        ret
noop_sprite_callbacks_12970 ENDP
_TEXT ENDS
        END
