/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern short g_747c;
extern unsigned char g_7486;
extern unsigned char g_7487;
extern unsigned char g_74fd[];
extern short g_e2fc;
extern int f_11530(int, int, int, int, int);
extern int f_115da(void);
extern void f_c011(void);
extern int free(int);
short f_bf24(volatile int a0, short a1)
{
    int unused_8;
    if (g_747c != -1) goto L_bf89;
    if (*(short *)g_74fd != -1) goto L_bf4d;
    f_c011();
L_bf4d:;
    free((int)f_115da);
    return f_11530(a0, a1, g_e2fc, g_7486, g_7487);
L_bf89:;
    return 0x605;
}
