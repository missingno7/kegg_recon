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
extern int f_a658(int, int, int, int);
extern unsigned g_7c0c;
extern void f_13889(int, int, int);
extern int a_a284(int, int, void *);
extern int f_a0e0(int, int, void *);
extern int f_c685(int, void *, void *);
extern int f_c826(int, int, int);

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_a520  load a picture file                                    лл
  лл                                                                лл
  лл Reads the file into the load buffer, then decodes it into      лл
  лл the current picture (g_e1d4).                                  лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int f_a520(int a0, int a1, int a2)
{
    int v_4;
    v_4 = f_1065b((void *)a0, (void *)a1);
    if (v_4 != 0) goto L_a564;
    v_4 = f_a658(a0, (int)g_e4d0_wfmxdlyju, a2, g_e4c8);
L_a564:;
    return v_4;
}

/* picture in memory */
unsigned char *g_e1d4;
int g_e1d8;
int g_e1dc;
int g_e1e0_3;
int g_e1e4_n;
int g_e1e8_1p;
int g_e1ec_f;

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_a574_wrvhrpegbz  load a picture, keep it in work memory      лл
  лл                                                                лл
  лл Decodes behind the load buffer and moves the pixels down.      лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int f_a574_wrvhrpegbz(int a0)
{
    int v_4;
    int v_8;
    v_4 = f_1065b((void *)a0, g_e4d0_wfmxdlyju);
    if (v_4 != 0) goto L_a648;
    v_8 = (int)((g_e4d0_wfmxdlyju + g_e4c8) + 3) & -4;
    v_4 = f_a658(a0, (int)g_e4d0_wfmxdlyju, v_8, g_e4c8);
    if (v_4 != 0) goto L_a648;
    if ((*(int *)((unsigned char *)&g_e1d4) + g_e1ec_f) <= g_7c0c) goto L_a5f3;
    v_4 = 0x302;
    goto L_a648;
L_a5f3:;
    f_13889(v_8, (int)g_e4d0_wfmxdlyju, g_e1ec_f);
    g_e1d8 = (int)(g_e4d0_wfmxdlyju + (g_e1d8 - *(int *)((unsigned char *)&g_e1d4)));
    *(int *)((unsigned char *)&g_e1d4) = (int)g_e4d0_wfmxdlyju;
    g_e4d0_wfmxdlyju += g_e1ec_f;
    g_e4d0_wfmxdlyju = (unsigned char *)((int)(g_e4d0_wfmxdlyju + 3) & -4);
L_a648:;
    return v_4;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_a658  decode by extension: .VGA .IFF .LBM .GIF .PCX .RAW     лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int f_a658(int a0, int a1, int a2, int a3)
{
    int v_4;
    int v_8;
    unsigned char v_14[8];
    unsigned char v_20[12];
    unsigned char v_a4[132];
    _splitpath((char *)a0, (char *)&v_4, (char *)v_a4, (char *)v_20, (char *)v_14);
    if (_stricmp((char *)v_14, (char *)".VGA") == 0) {
        f_13889(a1, a2, a3);
        v_8 = 0;
        *(int *)((unsigned char *)&g_e1d4) = a2;
        g_e1dc = 0x140;
        g_e1e0_3 = 0xc8;
        g_e1e8_1p = 0x100;
        g_e1e4_n = 0;
        g_e1d8 = (int)(*(unsigned char * *)((unsigned char *)&g_e1d4) + (g_e1dc * g_e1e0_3));
        goto L_a7e7;
    }
    if (_stricmp((char *)v_14, (char *)".IFF") != 0) {
        if (_stricmp((char *)v_14, (char *)".LBM") != 0) goto L_a74d;
    }
    v_8 = a_a284(a1, a2, ((unsigned char *)&g_e1d4));
    goto L_a7e7;
L_a74d:;
    if (_stricmp((char *)v_14, (char *)".GIF") == 0) {
        v_8 = f_c826(a1, a2, (int)((unsigned char *)&g_e1d4));
        goto L_a7e7;
    }
    if (_stricmp((char *)v_14, (char *)".PCX") == 0) {
        v_8 = f_a0e0(a1, a2, ((unsigned char *)&g_e1d4));
        goto L_a7e7;
    }
    if (_stricmp((char *)v_14, (char *)".RAW") == 0) {
        v_8 = f_c685(a1, (void *)a2, ((unsigned char *)&g_e1d4));
        goto L_a7e7;
    }
    v_8 = 0x301;
L_a7e7:;
    g_e1ec_f = (g_e1dc * g_e1e0_3) + 0x300;
    return v_8;
}
