/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
typedef struct { unsigned char :2; unsigned char f:6; } BF1_2_6;
typedef struct { unsigned char b[2]; } S2;
extern unsigned char g_25ec[];
extern unsigned char g_5e68[];
extern unsigned char g_6098[];
extern int g_8db4;
extern unsigned char g_8e10[];
extern unsigned char g_a518[];
extern unsigned char g_a608[];
extern unsigned char g_a848[];
extern unsigned g_dd50;
extern int g_dd60;
extern int g_dd64;
extern int g_dd68;
extern int g_dd70;
extern S2 * g_dd80;
extern unsigned char *g_dd90;
extern unsigned char *g_dd94;
extern int g_dda0;
extern int g_ddb8;
extern int g_ddc4;
extern int g_ddc8;
extern int g_ddcc;
extern unsigned char *g_dddc;
extern unsigned char *g_dee4;
extern unsigned char *g_df2c;
extern unsigned char *g_df34;
extern unsigned char g_e22c[];
extern int g_e230;
extern unsigned char g_e24c[];
extern int g_e250;
extern int g_e254;
extern int g_e258;
extern unsigned char *g_e2ec;
extern void f_3918(int, int, int, int);
extern void f_3ac0(int, int, int);
extern void f_3e0f(int, int);
extern void f_4394(void);
extern void f_69cd(void);
extern void f_7e62(int, int, int, int, int, int, int);
extern void f_8004(void *, void *);
extern void f_8066(void *, void *);
extern int f_b5df(void *, void *);
extern void f_c20d(int);
extern void f_c26a(int, int);
void f_6327(void)
{
    int v_4;
    int v_8;
    int v_c;
    int v_10;
    int v_14;
    g_dd90 = g_a518;
    for (g_dda0 = 0; g_dda0 < g_ddc8; ++g_dda0) {
        *(int *)(g_dd90 + 4) += *(int *)(g_dd90 + 8);
        g_8db4 = *(int *)(g_dd90 + 0xc);
        *(int *)g_e24c = *(int *)g_dd90;
        g_e250 = *(int *)(g_dd90 + 4);
        f_8004(g_e24c, (void *)g_8db4);
        if (g_e254 <= 0x140) {
            if (*(int *)g_e24c >= 0) goto L_63c3;
        }
        goto L_63cf;
L_63c3:;
        if (g_e258 > 0xdc) {
L_63cf:;
        } else {
            if (g_e250 >= 0x14) goto L_6464;
        }
        if ((*(int *)(g_dd90 + 0x10) & 3) == 2) {
            f_7e62((*(int *)g_e24c + g_e254) >> 1, (g_e250 + g_e258) >> 1, 0, 0, (int)g_df2c, (int)g_25ec, 1);
            f_c20d(0x57);
        } else {
            f_7e62((*(int *)g_e24c + g_e254) >> 1, 0x18, 0, 0, (int)g_df34, (int)g_6098, 1);
        }
        f_69cd();
        continue;
L_6464:;
        g_dd94 = g_a848;
        for (g_ddc4 = 0; g_ddc4 < g_ddcc; ++g_ddc4) {
            *(int *)g_e22c = *(int *)g_dd94;
            g_e230 = *(int *)(g_dd94 + 4);
            f_8066(g_e22c, (void *)*(int *)(g_dd94 + 0x10));
            if (f_b5df(g_e24c, g_e22c) != 0) {
                *(unsigned char *)(g_dd94 + 0x24) |= 2;
                if ((*(int *)(g_dd90 + 0x10) & 3) == 2) {
                    *(int *)(g_dd94 + 0x20) = 0;
                    goto L_6527;
                }
                if ((*(int *)(g_dd90 + 0x10) & 3) == 1) {
                    *(int *)(g_dd94 + 0x20) -= 4;
                    goto L_6527;
                }
                --*(int *)(g_dd94 + 0x20);
L_6527:;
                if (*(int *)(g_dd94 + 0x20) <= 0) {
                    *(int *)(g_dd94 + 0xc) += *(int *)(g_dd90 + 8) >> 1;
                }
                if ((*(int *)(g_dd90 + 0x10) & 3) != 2) {
                    f_7e62((*(int *)g_e24c + g_e254) >> 1, (g_e250 + g_e258) >> 1, 0, 0, (int)g_df34, (int)g_5e68, 1);
                    f_69cd();
                    goto L_69c3;
                }
            }
            g_dd94 += 0x28;
        }
        v_14 = 0;
        v_4 = (g_e250 + g_e258) >> 1;
        if (v_4 >= 0x18) {
            if (v_4 <= 0x98) goto L_65cc;
        }
        goto L_6975;
L_65cc:;
        if (g_e254 >= 0x10) {
            if (*(int *)g_e24c <= 0x130) goto L_65e6;
        }
        goto L_6975;
L_65e6:;
        g_dd80 = (S2 *)g_a608;
        v_8 = (*(int *)g_e24c - 0x10) >> 4;
        v_c = (g_e254 - 0x10) >> 4;
        v_10 = ((v_4 - 0x18) >> 3) * 0x12;
        if (v_8 < 0) {
            v_8 = 0;
        }
        if (v_c > 0x11) {
            v_c = 0x11;
        }
        if (v_10 > 0x10e) {
            v_10 = 0x10e;
        }
        g_dd80 += v_10;
        for (v_4 = v_8; v_4 <= v_c; v_4++) {
            g_dd50 = *(unsigned char *)(((unsigned char *)g_dd80) + ((v_4 * 2) + 1));
            if (g_dd50 == 0xf8) {
                *(int *)g_dd90 += g_dd68 - g_dd60;
                *(int *)(g_dd90 + 4) += (g_dd70 - g_dd64) - 8;
                v_4 = v_c;
                goto L_6963;
            }
            if (g_dd50 == 0xf9) {
                *(int *)g_dd90 += g_dd60 - g_dd68;
                *(int *)(g_dd90 + 4) += (g_dd64 - g_dd70) - 8;
                v_4 = v_c;
                goto L_6963;
            }
            if (g_dd50 != 0) {
                if ((*(int *)(g_dd90 + 0x10) & 3) == 2) goto L_671f;
            }
            goto L_673e;
L_671f:;
            f_3918(v_4, v_10, 0, *(int *)(g_dd90 + 8));
            goto L_6963;
L_673e:;
            if (g_dd50 >= 1) {
                if (g_dd50 <= 0x90) goto L_6758;
            }
            goto L_67fe;
L_6758:;
            if (g_dd50 >= 1) {
                if (g_dd50 <= 0x10) goto L_677e;
            }
            if (g_dd50 >= 0x31) {
                if (g_dd50 <= 0x40) goto L_677e;
            }
            goto L_6780;
L_677e:;
            goto L_6794;
L_6780:;
            if (g_dd50 >= 0x61) {
                if (g_dd50 <= 0x70) goto L_6794;
            }
            goto L_67b0;
L_6794:;
            f_3918(v_4, v_10, 0, *(int *)(g_dd90 + 8));
            goto L_67e3;
L_67b0:;
            g_dd50 -= 0x10;
            *(unsigned char *)(((unsigned char *)g_dd80) + ((v_4 * 2) + 1)) = *(unsigned char *)&g_dd50;
            f_3ac0(v_4, v_10, g_dd50);
L_67e3:;
            f_c26a(0xc, 0x19);
            *(int *)g_8e10 = (int)g_5e68;
            goto L_691c;
L_67fe:;
            if (g_dd50 >= 0x91) {
                if (g_dd50 <= 0x100) goto L_681b;
            }
            goto L_6963;
L_681b:;
            if (g_dd50 >= 0xf5) {
                if (g_dd50 <= 0xf7) goto L_6835;
            }
            goto L_685f;
L_6835:;
            f_c26a(0x2e, 0x32);
            g_ddb8 = -1;
            f_4394();
            *(int *)g_8e10 = (int)g_6098;
            goto L_691c;
L_685f:;
            if (g_dd50 >= 0xfa) {
                if (g_dd50 <= 0x100) goto L_687c;
            }
            goto L_6906;
L_687c:;
            f_c26a(1, 9);
            if (--((BF1_2_6 *)(((unsigned char *)g_dd80) + (v_4 * 2)))->f == 0) {
                *(int *)(g_dddc + 0x14) += 2 << *(int *)(g_dee4 + 0x24);
                f_3e0f(g_dd50, v_4 + v_10);
                f_3918(v_4, v_10, 0, *(int *)(g_dd90 + 8));
            }
            *(int *)g_8e10 = (int)g_5e68;
            goto L_691c;
L_6906:;
            f_c26a(0x2a, 0x2d);
            *(int *)g_8e10 = (int)g_6098;
L_691c:;
            f_7e62((v_4 << 4) + 0x18, ((v_10 / 0x12) << 3) + 0x1c, 0, 0, (int)g_df34, *(int *)g_8e10, 1);
            v_14 = -1;
            continue;
L_6963:;
        }
        if (v_14 == 0) goto L_6975;
        f_69cd();
        goto L_69c3;
L_6975:;
        *(int *)g_e2ec = *(int *)(g_dd90 + 0xc);
        *(short *)(g_e2ec + 4) = *(short *)g_dd90;
        *(short *)(g_e2ec + 6) = *(short *)(g_dd90 + 4);
        *(short *)(g_e2ec + 8) = 0;
        g_e2ec += 0xa;
        g_dd90 += 0x14;
L_69c3:;
    }
}
