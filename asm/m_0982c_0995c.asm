.386
; Trailer markers select the decoder variant and the stored trailer length.
ASSET_TAG_COD0_ID        EQU 30444F43h
ASSET_TAG_COD1_ID        EQU 31444F43h
ASSET_TAG_COD2_ID        EQU 32444F43h
ASSET_CODEC_COD0         EQU 1
ASSET_CODEC_COD2         EQU 2
ASSET_COD0_TRAILER_BYTES EQU 0Ah
ASSET_COD2_TRAILER_BYTES EQU 8
ASSET_DECODE_LIMIT       EQU 400h
ASSET_CHECKSUM_SEED      EQU 1234h
ASSET_DECODE_FAILURE     EQU 0FFFFFFFFh
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
        PUBLIC g_asset_checksum
        PUBLIC g_asset_trailer_size
        PUBLIC g_asset_encoding
g_asset_checksum  DW 0
g_asset_trailer_size  DD 0
g_asset_encoding  DW 0
_DATA ENDS
DGROUP GROUP _DATA
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        ASSUME CS:_TEXT
        PUBLIC decode_and_verify_asset
        PUBLIC decode_and_verify_asset
; Decode the tagged payload in place and return its trailer size after checksum validation.
decode_and_verify_asset LABEL NEAR
decode_and_verify_asset PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        mov     esi,[ebp+8]
        mov     edi,esi
        mov     ecx,[ebp+0Ch]
        mov     word ptr g_asset_checksum,0
        mov     dword ptr g_asset_trailer_size,0
        mov     word ptr g_asset_encoding,0FFFFh
        cmp     dword ptr [esi+ecx-4],ASSET_TAG_COD0_ID
        je      decode_cod0_or_cod1_payload
        cmp     dword ptr [esi+ecx-4],ASSET_TAG_COD1_ID
        je      decode_cod0_or_cod1_payload
        cmp     dword ptr [esi+ecx-4],ASSET_TAG_COD2_ID
        je      short decode_cod2_payload
        popad
        mov     eax,ASSET_DECODE_FAILURE
        ret
asset_decode_result:
        popad
        mov     eax,dword ptr g_asset_trailer_size
        cmp     word ptr g_asset_checksum,0
        je      short asset_checksum_error
        mov     eax,ASSET_DECODE_FAILURE
asset_checksum_error:
        ret
decode_cod2_payload:
        mov     eax,ASSET_COD2_TRAILER_BYTES
        mov     word ptr g_asset_encoding,ASSET_CODEC_COD2
        mov     dword ptr g_asset_trailer_size,eax
        sub     ecx,eax
        lea     ebx,[esi+ecx]
        mov     ax,word ptr [ebx]
        mov     word ptr g_asset_checksum,ax
        mov     ax,word ptr [ebx+2]
        ror     ax,7
        shl     eax,10h
        mov     ax,word ptr [ebx+2]
        rol     ax,3
        mov     ebx,eax
        cmp     ecx,ASSET_DECODE_LIMIT
        jle     short asset_decode_limit_400_bytes
        mov     ecx,ASSET_DECODE_LIMIT
asset_decode_limit_400_bytes:
        shr     ecx,2
        sub     ebp,ebp
decode_cod2_words:
        lodsd
        xor     eax,ebx
        stosd
        xor     ebp,eax
        rol     ebx,1
        loop    short decode_cod2_words
        xor     word ptr g_asset_checksum,bp
        shr     ebp,10h
        xor     word ptr g_asset_checksum,bp
        jmp     short asset_decode_result
decode_cod0_or_cod1_payload:
        mov     eax,ASSET_COD0_TRAILER_BYTES
        mov     word ptr g_asset_encoding,ASSET_CODEC_COD0
        mov     dword ptr g_asset_trailer_size,eax
        sub     ecx,eax
        lea     ebx,[esi+ecx]
        mov     ax,word ptr [ebx]
        mov     word ptr g_asset_checksum,ax
        mov     ebx,dword ptr [ebx+2]
        ror     ebx,7
        shr     ecx,2
        mov     ebp,ASSET_CHECKSUM_SEED
decode_cod0_or_cod1_words:
        lodsd
        xor     eax,ebx
        stosd
        xor     ebp,eax
        rol     ebx,1
        loop    short decode_cod0_or_cod1_words
        xor     word ptr g_asset_checksum,bp
        shr     ebp,10h
        xor     word ptr g_asset_checksum,bp
        jmp     near ptr asset_decode_result
        mov     dword ptr g_asset_trailer_size,0
        mov     eax,0
        ret
decode_and_verify_asset ENDP
_TEXT ENDS
        END
