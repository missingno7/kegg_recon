/*
 * PICTURE.C  -  picture file loader
 *
 * Loads a picture into the work buffer; the file format
 * is chosen from the file name extension:
 *
 *     .VGA          320 x 200 screen dump
 *     .IFF / .LBM   Deluxe Paint ILBM
 *     .GIF          CompuServe GIF
 *     .PCX          ZSoft Paintbrush
 *     .RAW          raw bitmaps
 *
 * Returns 0, else an error (0x301 bad type).
 */
#include <stdlib.h>
#include <string.h>
extern int g_e4c8;
extern unsigned char *g_e4d0_wfmxdlyju;
extern int f_1065b(void *, void *);
extern int decode_picture(int, int, int, int);
extern unsigned g_7c0c;
extern void f_13889(int, int, int);
extern int a_a284(int, int, void *);
extern int f_a0e0(int, int, void *);
extern int f_c685(int, void *, void *);
extern int f_c826(int, int, int);

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл load_picture  load a picture file                                    лл
  лл                                                                лл
  лл Reads the file into the load buffer, then decodes it into      лл
  лл the current picture (picture_pixels8).                                  лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int load_picture(int filename, int load_buffer, int image_buffer)
{
    int status;
    status = f_1065b((void *)filename, (void *)load_buffer);
    if (!status) {
        status = decode_picture(filename, (int)g_e4d0_wfmxdlyju, image_buffer, g_e4c8);
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
                                                                      
                                                                      
 /*ллл     ллл     л
     ллл     ллл*/                                                                      
                                                                      
                                     

int load_picture_keep(int filename)
{
    int status;
    int aligned_buffer_end;
    status = f_1065b((void *)filename, g_e4d0_wfmxdlyju);
    if (!status) {
        aligned_buffer_end = (int)((g_e4d0_wfmxdlyju + g_e4c8) + 3) & -4;
        status = decode_picture(filename, (int)g_e4d0_wfmxdlyju, aligned_buffer_end, g_e4c8);
        if (!status) {
            if ((*(int *)((unsigned char *)&picture_pixels8) + picture_bytes77) > g_7c0c) {
                status = 0x302;
            } else {
                f_13889(aligned_buffer_end, (int)g_e4d0_wfmxdlyju, picture_bytes77);
                pic_of = (int)(g_e4d0_wfmxdlyju + (pic_of - *(int *)((unsigned char *)&picture_pixels8)));
                *(int *)((unsigned char *)&picture_pixels8) = (int)g_e4d0_wfmxdlyju;
                g_e4d0_wfmxdlyju += picture_bytes77;
                g_e4d0_wfmxdlyju = (unsigned char *)((int)(g_e4d0_wfmxdlyju + 3) & -4);
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
        f_13889(source_buffer, destination, source_size);
        status = 0;
        *(int *)((unsigned char *)&picture_pixels8) = destination;
        picture_width8 = 0x140;
        picture_height0 = 0xc8;
        picture_y9n = 0x100;
        picture_x5 = 0;
        pic_of = (int)(*(unsigned char * *)((unsigned char *)&picture_pixels8) + (picture_width8 * picture_height0));
    } else if (!_stricmp((char *)extension, (char *)".IFF") || !_stricmp((char *)extension, (char *)".LBM")) {
        status = a_a284(source_buffer, destination, ((unsigned char *)&picture_pixels8));
    } else if (!_stricmp((char *)extension, (char *)".GIF")) {
        status = f_c826(source_buffer, destination, (int)((unsigned char *)&picture_pixels8));
    } else if (!_stricmp((char *)extension, (char *)".PCX")) {
        status = f_a0e0(source_buffer, destination, ((unsigned char *)&picture_pixels8));
    } else if (!_stricmp((char *)extension, (char *)".RAW")) {
        status = f_c685(source_buffer, (void *)destination, ((unsigned char *)&picture_pixels8));
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
