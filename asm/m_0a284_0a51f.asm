.386
; Big-endian IFF chunk identifiers as they appear in the little-endian file words.
IFF_FORM_ID                 EQU 4D524F46h
IFF_BODY_ID                 EQU 424F4459h
IFF_BMHD_ID                 EQU 424D4844h
IFF_CMAP_ID                 EQU 434D4150h
IFF_ILBM_ID                 EQU 4D424C49h
IFF_FORM_TYPE_OFFSET        EQU 8
IFF_BITMAP_HEADER_WIDTH     EQU 4
IFF_BITMAP_HEADER_HEIGHT    EQU 6
IFF_BITMAP_HEADER_COMPRESSION EQU 0Eh
IFF_ERROR_CLASS             EQU 8
IFF_ERROR_INVALID_FORM      EQU 1
IFF_ERROR_BODY_MISSING      EQU 2
IFF_ERROR_HEADER_MISSING    EQU 3
IFF_ERROR_PALETTE_MISSING   EQU 4
IFF_ERROR_BODY_OVERRUN      EQU 5
BYTERUN1_NOOP_CONTROL       EQU 080h
IFF_COMPRESSION_NONE        EQU 0
; Dword fields written into the caller's decoded-image descriptor.
IFF_IMAGE_LAYOUT STRUC
IFF_IMAGE_PIXEL_BUFFER       DD ?
IFF_IMAGE_PALETTE_BUFFER     DD ?
IFF_IMAGE_WIDTH_PIXELS       DD ?
IFF_IMAGE_HEIGHT_PIXELS      DD ?
IFF_IMAGE_FORMAT_CODE        DD ?
IFF_IMAGE_PALETTE_ENTRIES    DD ?
IFF_IMAGE_FILE_BYTE_COUNT    DD ?
IFF_IMAGE_PALETTE_BYTE_COUNT DD ?
IFF_IMAGE_PIXEL_COUNT        DD ?
IFF_IMAGE_LAYOUT ENDS
DGROUP GROUP _DATA
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
EXTRN iff_width_pixels:DWORD
EXTRN iff_height_pixels:DWORD
EXTRN iff_output_byte_count:DWORD
EXTRN iff_decoded_pixel_count:DWORD
_DATA ENDS
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
        ASSUME CS:_TEXT, DS:DGROUP
        PUBLIC decode_iff_ilbm_image
; Picture loader calls the descriptive IFF/ILBM entry.
; Read FORM/BODY/BMHD/CMAP chunks, expand ByteRun1, then convert ILBM planes to indexed pixels.
decode_iff_ilbm_image PROC NEAR
        pushad
        lea     ebp,[esp+1Ch]
        mov     ebx,[ebp+0Ch]
        mov     esi,[ebp+8]
        mov     dword ptr g_iff_file,esi
        mov     dword ptr g_iff_pixels,ebx
        cmp     dword ptr [esi],IFF_FORM_ID
        je      short form_header_valid
        mov     al,IFF_ERROR_INVALID_FORM
set_iff_decode_error:
        ; The high byte is the decode error class; AL identifies the failed chunk or body check.
        mov     ah,IFF_ERROR_CLASS
        movzx   eax,ax
        mov     dword ptr g_iff_error,eax
        jmp     near ptr store_iff_result
form_header_valid:
        mov     edx,IFF_BODY_ID
        call    find_iff_chunk
        mov     al,IFF_ERROR_BODY_MISSING
        jb      short set_iff_decode_error
        mov     eax,[ebx]
        xchg    al,ah
        rol     eax,10h
        xchg    al,ah
        lea     esi,[ebx+IFF_BITMAP_HEADER_WIDTH]
        mov     edi,dword ptr g_iff_pixels
        mov     edx,IFF_BMHD_ID
        call    find_iff_chunk
        mov     al,IFF_ERROR_HEADER_MISSING
        jb      short set_iff_decode_error
        movzx   eax,word ptr [ebx+IFF_BITMAP_HEADER_WIDTH]
        movzx   ecx,word ptr [ebx+IFF_BITMAP_HEADER_HEIGHT]
        xchg    al,ah
        xchg    cl,ch
        mov     dword ptr iff_width_pixels,eax
        mov     dword ptr iff_height_pixels,ecx
        mul     ecx
        mov     dword ptr g_iff_pixel_count,eax
        lea     ecx,[edi+eax]
        mov     dword ptr g_iff_decoder_workspace,ecx
        cmp     byte ptr [ebx+IFF_BITMAP_HEADER_COMPRESSION],IFF_COMPRESSION_NONE
        je      short copy_uncompressed_body
        sub     ecx,ecx
byterun_next_control:
        mov     cl,byte ptr [esi]
        inc     esi
        cmp     cl,BYTERUN1_NOOP_CONTROL
        jb      short byterun_literal_run
        ja      short byterun_repeat_run
        jmp     short byterun_next_control
byterun_repeat_run:
        neg     cl
        inc     cl
        lodsb
        rep     stosb
        cmp     edi,dword ptr g_iff_decoder_workspace
        jl      short byterun_next_control
        mov     al,IFF_ERROR_BODY_OVERRUN
        jne     near ptr set_iff_decode_error
        jmp     short find_cmap_chunk
byterun_literal_run:
        inc     ecx
        rep     movsb
        cmp     edi,dword ptr g_iff_decoder_workspace
        jl      short byterun_next_control
        mov     al,IFF_ERROR_BODY_OVERRUN
        jne     near ptr set_iff_decode_error
        jmp     short find_cmap_chunk
copy_uncompressed_body:
        mov     ecx,dword ptr g_iff_pixel_count
        shr     ecx,2
        rep     movsd
find_cmap_chunk:
        mov     edx,IFF_CMAP_ID
        call    find_iff_chunk
        mov     al,IFF_ERROR_PALETTE_MISSING
        jb      near ptr set_iff_decode_error
        mov     esi,ebx
        lodsd
        mov     ecx,eax
        xchg    cl,ch
        rol     ecx,10h
        xchg    cl,ch
        mov     dword ptr g_iff_palette_bytes,ecx
scale_palette_component:
        lodsb
        shr     al,2
        stosb
        loop    short scale_palette_component
        sub     edi,dword ptr g_iff_pixels
        mov     esi,dword ptr g_iff_file
        cmp     dword ptr [esi+IFF_FORM_TYPE_OFFSET],IFF_ILBM_ID
        jne     near ptr store_iff_result
        mov     esi,dword ptr g_iff_pixels
        mov     edi,dword ptr g_iff_file
        mov     ecx,dword ptr iff_height_pixels
ilbm_row_loop:
        push    ecx
        mov     ecx,dword ptr iff_width_pixels
        shr     ecx,4
ilbm_sixteen_pixel_group_loop:
        push    ecx
        mov     ecx,2
ilbm_eight_pixel_block_loop:
        push    ecx
        mov     ecx,8
ilbm_plane_bit_loop:
        sub     al,al
        dec     ecx
        mov     edx,dword ptr iff_width_pixels
        add     esi,edx
        shr     edx,3
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        sub     esi,edx
        add     al,al
        bt      word ptr [esi],cx
        adc     al,ch
        inc     ecx
        stosb
        dec     cx
        je      short ilbm_pixel_block_complete
        jmp     short ilbm_plane_bit_loop
ilbm_pixel_block_complete:
        inc     esi
        pop     ecx
        dec     cx
        je      short ilbm_byte_block_complete
        jmp     short ilbm_eight_pixel_block_loop
ilbm_byte_block_complete:
        pop     ecx
        dec     cx
        je      short ilbm_sixteen_pixel_group_complete
        jmp     near ptr ilbm_sixteen_pixel_group_loop
ilbm_sixteen_pixel_group_complete:
        mov     edx,dword ptr iff_width_pixels
        mov     ecx,edx
        shr     ecx,3
        sub     edx,ecx
        add     esi,edx
        pop     ecx
        dec     cx
        je      short ilbm_image_complete
        jmp     near ptr ilbm_row_loop
ilbm_image_complete:
        mov     eax,dword ptr iff_width_pixels
        mul     dword ptr iff_height_pixels
        mov     ecx,eax
        mov     edi,dword ptr g_iff_pixels
        mov     esi,dword ptr g_iff_file
        std
        add     edi,ecx
        add     esi,ecx
        dec     esi
        dec     edi
        rep     movsb
        cld
        mov     edi,eax
        add     edi,dword ptr g_iff_palette_bytes
store_iff_result:
        mov     dword ptr iff_output_byte_count,edi
        mov     edi,dword ptr g_iff_pixel_count
        mov     dword ptr iff_decoded_pixel_count,edi
        mov     ebx,[ebp+10h]
        mov     eax,[ebp+0Ch]
        mov     [ebx+IFF_IMAGE_PIXEL_BUFFER],eax
        mov     eax,dword ptr iff_width_pixels
        mul     dword ptr iff_height_pixels
        mov     [ebx+IFF_IMAGE_PIXEL_COUNT],eax
        add     eax,[ebp+0Ch]
        mov     [ebx+IFF_IMAGE_PALETTE_BUFFER],eax
        mov     eax,dword ptr iff_width_pixels
        mov     [ebx+IFF_IMAGE_WIDTH_PIXELS],eax
        mov     eax,dword ptr iff_height_pixels
        mov     [ebx+IFF_IMAGE_HEIGHT_PIXELS],eax
        mov     dword ptr [ebx+IFF_IMAGE_PALETTE_ENTRIES],100h
        mov     dword ptr [ebx+IFF_IMAGE_PALETTE_BYTE_COUNT],300h
        mov     eax,[ebx+IFF_IMAGE_PALETTE_BYTE_COUNT]
        add     eax,[ebx+IFF_IMAGE_PIXEL_COUNT]
        mov     [ebx+IFF_IMAGE_FILE_BYTE_COUNT],eax
        mov     dword ptr [ebx+IFF_IMAGE_FORMAT_CODE],1
        popad
        mov     eax,dword ptr g_iff_error
        ret
decode_iff_ilbm_image ENDP
        PUBLIC find_iff_chunk
find_iff_chunk PROC NEAR
        push    esi
        push    edi
        xchg    dl,dh
        rol     edx,10h
        xchg    dl,dh
        mov     edi,dword ptr g_iff_file
        mov     ecx,[edi+4]
        xchg    cl,ch
        rol     ecx,10h
        xchg    cl,ch
        lea     esi,[edi+0Ch]
        add     ecx,esi
scan_iff_chunks:
        lodsd
        cmp     eax,edx
        je      short iff_chunk_found
        lodsd
        xchg    al,ah
        rol     eax,10h
        xchg    al,ah
        add     esi,eax
        inc     esi
        and     esi,-2
        cmp     esi,ecx
        jl      short scan_iff_chunks
        stc
        jmp     short iff_chunk_search_result
iff_chunk_found:
        clc
        mov     ebx,esi
iff_chunk_search_result:
        pop     edi
        pop     esi
        ret
find_iff_chunk ENDP
_TEXT ENDS
_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
        PUBLIC g_iff_error
g_iff_error	DD 0
        PUBLIC g_iff_file
g_iff_file	DD 0
        PUBLIC g_iff_pixels
g_iff_pixels	DD 0
        PUBLIC g_iff_pixel_count
g_iff_pixel_count	DD 0
        PUBLIC g_iff_palette_bytes
g_iff_palette_bytes	DD 0
        PUBLIC g_iff_decoder_workspace
g_iff_decoder_workspace	DD 5 DUP (0)
_DATA ENDS
        END
