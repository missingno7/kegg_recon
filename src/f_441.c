/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern unsigned char *g_dddc;
extern unsigned char g_e13b;
extern unsigned char g_e13e;
extern unsigned char g_e140;
extern unsigned char g_e142;
extern unsigned char g_e143;
extern unsigned char g_e144;
extern void f_18bc(void);
extern int f_6b02(void);
void f_441(void)
{
    if (g_e142 == 0) goto L_462;
    g_e142 = 0;
    return;
L_462:;
    if (g_e13e == 0) goto L_486;
    if (g_e140 == 6) goto L_4bb;
    ++g_e143;
    g_e13e = 0;
    return;
L_486:;
    ++g_e143;
    if (g_e13b != 0) goto L_52a;
    if (g_e143 < 0xa) goto L_4b9;
    if ((g_e143 % 0xa) == 0) goto L_4bb;
L_4b9:;
    goto L_4f8;
L_4bb:;
    g_e13e = (unsigned char)(g_e143 / 0xa);
    if (f_6b02() == 0) goto L_4ea;
    f_18bc();
    g_e144 = 0xff;
    goto L_4f6;
L_4ea:;
    *(int *)(g_dddc + 4) = -1;
L_4f6:;
    return;
L_4f8:;
    if ((g_e143 % 5) != 0) goto L_52a;
    g_e142 = (unsigned char)(((g_e143 / 5) + 1) >> 1);
L_52a:;
}
