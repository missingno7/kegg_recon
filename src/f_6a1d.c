/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
typedef struct { unsigned f:2; } BF4_0_2;
extern int g_8e20;
extern unsigned char g_a518[];
extern unsigned char *g_dd90;
extern int g_ddc8;
extern unsigned char *g_df34;
extern void f_c20d(int);
void f_6a1d(unsigned char a0, int a1, int a2, int a3)
{
    unsigned char v_4;
    if (g_ddc8 >= 0xc) goto L_6afd;
    g_dd90 = g_a518;
    g_dd90 += g_ddc8 * 0x14;
    *(int *)g_dd90 = a2;
    *(int *)(g_dd90 + 4) = a3;
    *(int *)(g_dd90 + 8) = a1;
    v_4 = a0;
    if (v_4 < 1) goto L_6a8a;
    if (v_4 <= 1) goto L_6aa6;
    if (v_4 == 2) goto L_6abc;
    goto L_6ad0;
L_6a8a:;
    if (v_4 != 0) goto L_6ad0;
    g_8e20 = 0x21e6;
    f_c20d(0x5a);
    goto L_6ad0;
L_6aa6:;
    g_8e20 = 0x2280;
    f_c20d(0x58);
    goto L_6ad0;
L_6abc:;
    g_8e20 = 0x2342;
    f_c20d(0x66);
L_6ad0:;
    *(int *)(g_dd90 + 0xc) = (int)(g_df34 + g_8e20);
    ((BF4_0_2 *)(g_dd90 + 0x10))->f = a0;
    ++g_ddc8;
L_6afd:;
}
