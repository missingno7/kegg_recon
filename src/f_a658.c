/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <stdlib.h>
#include <string.h>
extern unsigned char g_11ec[];
extern unsigned char g_11f4[];
extern unsigned char g_11fc[];
extern unsigned char g_1204[];
extern unsigned char g_120c[];
extern unsigned char g_1214[];
extern unsigned char g_e1d4[];
extern int g_e1d8;
extern int g_e1dc;
extern int g_e1e0;
extern int g_e1e4;
extern int g_e1e8;
extern int g_e1ec;
extern int a_a284(int, int, void *);
extern void f_13889(int, int, int);
extern int f_a0e0(int, int, void *);
extern int f_c685(int, void *, void *);
extern int f_c826(int, int, int);
int f_a658(int a0, int a1, int a2, int a3)
{
    int v_4;
    int v_8;
    unsigned char v_14[8];
    unsigned char v_20[12];
    unsigned char v_a4[132];
    _splitpath((char *)a0, (char *)&v_4, (char *)v_a4, (char *)v_20, (char *)v_14);
    if (_stricmp((char *)v_14, (char *)g_11ec) == 0) {
        f_13889(a1, a2, a3);
        v_8 = 0;
        *(int *)g_e1d4 = a2;
        g_e1dc = 0x140;
        g_e1e0 = 0xc8;
        g_e1e8 = 0x100;
        g_e1e4 = 0;
        g_e1d8 = (int)(*(unsigned char * *)g_e1d4 + (g_e1dc * g_e1e0));
        goto L_a7e7;
    }
    if (_stricmp((char *)v_14, (char *)g_11f4) != 0) {
        if (_stricmp((char *)v_14, (char *)g_11fc) != 0) goto L_a74d;
    }
    v_8 = a_a284(a1, a2, g_e1d4);
    goto L_a7e7;
L_a74d:;
    if (_stricmp((char *)v_14, (char *)g_1204) == 0) {
        v_8 = f_c826(a1, a2, (int)g_e1d4);
        goto L_a7e7;
    }
    if (_stricmp((char *)v_14, (char *)g_120c) == 0) {
        v_8 = f_a0e0(a1, a2, g_e1d4);
        goto L_a7e7;
    }
    if (_stricmp((char *)v_14, (char *)g_1214) == 0) {
        v_8 = f_c685(a1, (void *)a2, g_e1d4);
        goto L_a7e7;
    }
    v_8 = 0x301;
L_a7e7:;
    g_e1ec = (g_e1dc * g_e1e0) + 0x300;
    return v_8;
}
