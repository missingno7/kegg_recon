/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern unsigned g_7c0c;
extern unsigned char g_e1d4[];
extern int g_e1d8;
extern int g_e1ec;
extern int g_e4c8;
extern unsigned char *g_e4d0;
extern int f_1065b(void *, void *);
extern void f_13889(int, int, int);
extern int f_a658(int, int, int, int);
int f_a574(int a0)
{
    int v_4;
    int v_8;
    v_4 = f_1065b((void *)a0, g_e4d0);
    if (v_4 != 0) goto L_a648;
    v_8 = (int)((g_e4d0 + g_e4c8) + 3) & -4;
    v_4 = f_a658(a0, (int)g_e4d0, v_8, g_e4c8);
    if (v_4 != 0) goto L_a648;
    if ((*(int *)g_e1d4 + g_e1ec) <= g_7c0c) goto L_a5f3;
    v_4 = 0x302;
    goto L_a648;
L_a5f3:;
    f_13889(v_8, (int)g_e4d0, g_e1ec);
    g_e1d8 = (int)(g_e4d0 + (g_e1d8 - *(int *)g_e1d4));
    *(int *)g_e1d4 = (int)g_e4d0;
    g_e4d0 += g_e1ec;
    g_e4d0 = (unsigned char *)((int)(g_e4d0 + 3) & -4);
L_a648:;
    return v_4;
}
