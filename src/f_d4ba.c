/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <i86.h>
#include <string.h>
extern short g_74b4;
extern unsigned g_74b6;
extern unsigned char g_75a8;
extern unsigned char g_75ac;
extern int g_e308;
extern int g_e30c;
extern int g_e310;
int f_d4ba(void)
{
    unsigned char v_10[12];
    unsigned char v_2c[28];
    memset((void *)v_10, 0, 0xc);
    *(short *)v_2c = 0x1687;
    int386x(0x2f, (void *)v_2c, (void *)v_2c, (void *)v_10);
    if (*(short *)v_2c != 0) goto L_d5b5;
    g_74b6 = ((((*(unsigned short *)(v_2c + 12) % 0x3e8) / 0x64) << 8) | (((*(unsigned short *)(v_2c + 12) % 0x64) / 0xa) << 4)) | (*(unsigned short *)(v_2c + 12) % 0xa);
    g_e30c = *(unsigned short *)(v_2c + 16);
    g_e310 = *(unsigned short *)(v_2c + 20);
    g_e308 = *(unsigned short *)v_10;
    *(short *)v_2c = 0x400;
    int386x(0x31, (void *)v_2c, (void *)v_2c, (void *)v_10);
    *(int *)&g_75a8 = *(unsigned char *)(v_2c + 13);
    *(int *)&g_75ac = *(unsigned char *)(v_2c + 12);
    g_74b4 = -1;
    return g_74b4;
L_d5b5:;
    g_74b4 = 0;
    return g_74b4;
}
