/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <process.h>
extern unsigned char g_101d[];
extern unsigned char g_101e[];
extern unsigned char g_74c4[];
extern unsigned char g_7536[];
extern unsigned char g_756f[];
extern void f_9afc(void);
extern void f_9b44(int);
extern void f_bd8b(void);
extern int f_bdd3(int);
extern void f_c2d0(int);
extern void f_dfc3(void);
extern void f_e028(void);
extern void f_ec9c(void);
extern void f_f6d9(void);
extern int f_f6f8(int);
void f_9f9(void)
{
    int v_4;
    int v_8;
    int v_c;
    v_4 = *(short *)g_756f;
    v_8 = *(short *)g_7536;
    v_c = *(short *)g_74c4;
    f_c2d0(0);
    f_9afc();
    f_f6d9();
    f_bd8b();
    f_e028();
    spawnlp(0, (char *)g_101e, (char *)g_101d, 0);
    f_ec9c();
    f_dfc3();
    if (v_4 != -1) goto L_a73;
    f_9b44(0);
L_a73:;
    if (v_8 != -1) goto L_a83;
    f_f6f8(0);
L_a83:;
    if (v_c != -1) goto L_a93;
    f_bdd3(0);
L_a93:;
    f_c2d0(-1);
}
