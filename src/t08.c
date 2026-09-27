/* Load VGA, IFF/LBM, GIF, PCX, and raw picture files. */
#include <stdlib.h>
#include <string.h>
extern int n_read;
extern unsigned char *file_buf_ptr;
extern int read_file_with_decoder(void *, void *);
extern int decode_picture(int, int, int, int);
extern unsigned buf_lim;
extern void mov_mem(int, int, int);
extern int decode_iff_ilbm_image(int, int, void *);
extern int decode_pcx_image(int, int, void *);
extern int decode_game_bitmap(int, void *, void *);
extern int decode_gif_with_workspace(int, int, int);




int load_picture(int filename, int load_buffer, int image_buffer)
{
    int status;
    status = read_file_with_decoder((void *)filename, (void *)load_buffer);
    if (!status) {
        status = decode_picture(filename, (int)file_buf_ptr, image_buffer, n_read);
    }
    return status;
}

/* picture in memory */
unsigned char *picture_pixels8;
int pic_of;
int picture_width8;
int picture_height0;
int picture_x5;
int picture_y9n;
int picture_bytes77;

/* Picture decoder keeps decoded pixels in shared work memory. */                                                                      
/*                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             */
 /*ллл     ллл     л
     ллл     ллл*/                                                                      
                                                                      
                                     

int load_picture_keep(int filename)
{
    int status;
    int aligned_buffer_end;
    status = read_file_with_decoder((void *)filename, file_buf_ptr);
    if (!status) {
        aligned_buffer_end = (int)((file_buf_ptr + n_read) + 3) & -4;
        status = decode_picture(filename, (int)file_buf_ptr, aligned_buffer_end, n_read);
        if (!status) {
            if ((*(int *)((unsigned char *)&picture_pixels8) + picture_bytes77) > buf_lim) {
                status = 0x302;
            } else {
                mov_mem(aligned_buffer_end, (int)file_buf_ptr, picture_bytes77);
                pic_of = (int)(file_buf_ptr + (pic_of - *(int *)((unsigned char *)&picture_pixels8)));
                *(int *)((unsigned char *)&picture_pixels8) = (int)file_buf_ptr;
                file_buf_ptr += picture_bytes77;
                file_buf_ptr = (unsigned char *)((int)(file_buf_ptr + 3) & -4);
            }
        }
    }
    return status;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл decode_picture  decode by extension: .VGA .IFF .LBM .GIF .PCX .RAW     лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int decode_picture(int filename, int source_buffer, int destination, int source_size)
{
    int drive;
    int status;
    unsigned char extension[8];
    unsigned char directory[12];
    unsigned char base_name[132];
    _splitpath((char *)filename, (char *)&drive, (char *)base_name, (char *)directory, (char *)extension);
    if (!_stricmp((char *)extension, (char *)".VGA")) {
        mov_mem(source_buffer, destination, source_size);
        status = 0;
        *(int *)((unsigned char *)&picture_pixels8) = destination;
        picture_width8 = 0x140;
        picture_height0 = 0xc8;
        picture_y9n = 0x100;
        picture_x5 = 0;
        pic_of = (int)(*(unsigned char * *)((unsigned char *)&picture_pixels8) + (picture_width8 * picture_height0));
    } else if (!_stricmp((char *)extension, (char *)".IFF") || !_stricmp((char *)extension, (char *)".LBM")) {
        status = decode_iff_ilbm_image(source_buffer, destination, ((unsigned char *)&picture_pixels8));
    } else if (!_stricmp((char *)extension, (char *)".GIF")) {
        status = decode_gif_with_workspace(source_buffer, destination, (int)((unsigned char *)&picture_pixels8));
    } else if (!_stricmp((char *)extension, (char *)".PCX")) {
        status = decode_pcx_image(source_buffer, destination, ((unsigned char *)&picture_pixels8));
    } else if (!_stricmp((char *)extension, (char *)".RAW")) {
        status = decode_game_bitmap(source_buffer, (void *)destination, ((unsigned char *)&picture_pixels8));
    } else {
        status = 0x301;
    }
    picture_bytes77 = (picture_width8 * picture_height0) + 0x300;
    return status;
}

/* The block-character banner near the top of this file is not decoration: wcc386 -ot pads the CONST
   literals with stale bytes of its 4 KiB source-read buffer, and the original KE.EXE holds CP437 0xDB
   bytes there (source offsets 2146-2148, 2154-2156, 2162-2164, 2170-2172, 2178-2180). Keep those bytes
   in place and this file 2181..6242 bytes long (docs/compiler-notes.md "-ot CONST literal padding"). */
