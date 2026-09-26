/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern int g_e4c8;
extern unsigned char *g_e4d0;
extern int f_1065b(void *, void *);
extern int f_a658(int, int, int, int);
int f_a520(int a0, int a1, int a2)
{
    int v_4;
    v_4 = f_1065b((void *)a0, (void *)a1);
    if (v_4 != 0) goto L_a564;
    v_4 = f_a658(a0, (int)g_e4d0, a2, g_e4c8);
L_a564:;
    return v_4;
}
