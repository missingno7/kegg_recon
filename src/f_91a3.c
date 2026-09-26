/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
typedef struct { unsigned char :2; unsigned char f:6; } BF1_2_6;
typedef struct { unsigned char b[2]; } S2;
extern unsigned char g_5e68[];
extern unsigned char g_6098[];
extern unsigned char g_8e10[];
extern int g_8e20;
extern unsigned char g_a608[];
extern unsigned g_dd50;
extern int g_dd58;
extern int g_dd5c;
extern int g_dd60;
extern int g_dd64;
extern int g_dd68;
extern int g_dd70;
extern S2 * g_dd80;
extern unsigned char *g_ddb4;
extern int g_ddb8;
extern unsigned char *g_dddc;
extern unsigned char *g_de5c;
extern unsigned char *g_dee4;
extern unsigned char *g_df34;
extern void f_3918(int, int, int, int);
extern void f_3ac0(int, int, int);
extern void f_3e0f(int, int);
extern void f_4394(void);
extern void f_7e62(int, int, int, int, int, int, int);
extern void f_c26a(int, int);
int f_91a3(int a0, int a1)
{
    int v_4;
    int v_8;
    if (a1 >= 0x18) {
        if (a1 < 0x98) goto L_91c3;
    }
    goto L_9513;
L_91c3:;
    if (a0 >= 0x10) {
        if (a0 < 0x130) goto L_91d7;
    }
    goto L_9513;
L_91d7:;
    v_4 = (a0 - 0x10) >> 4;
    v_8 = ((a1 - 0x18) >> 3) * 0x12;
    if (v_4 == g_dd58) {
        if (v_8 == g_dd5c) goto L_920a;
    }
    goto L_920f;
L_920a:;
    goto L_9581;
L_920f:;
    g_dd80 = (S2 *)g_a608;
    g_dd80 += v_8;
    g_dd50 = *(unsigned char *)(((unsigned char *)g_dd80) + ((v_4 * 2) + 1));
    if (g_dd50 == 0) goto L_9513;
    if (g_dd50 == 0xf8) {
        *(int *)g_ddb4 += (g_dd68 - g_dd60) << 4;
        *(int *)(g_ddb4 + 4) += (g_dd70 - g_dd64) << 4;
        goto L_9522;
    }
    if (g_dd50 == 0xf9) {
        *(int *)g_ddb4 += (g_dd60 - g_dd68) << 4;
        *(int *)(g_ddb4 + 4) += (g_dd64 - g_dd70) << 4;
        goto L_9522;
    }
    if (g_dd50 >= 1) {
        if (g_dd50 <= 0x90) goto L_92de;
    }
    goto L_9392;
L_92de:;
    if (g_dd50 >= 1) {
        if (g_dd50 <= 0x10) goto L_9304;
    }
    if (g_dd50 >= 0x31) {
        if (g_dd50 <= 0x40) goto L_9304;
    }
    goto L_9306;
L_9304:;
    goto L_931a;
L_9306:;
    if (g_dd50 >= 0x61) {
        if (g_dd50 <= 0x70) goto L_931a;
    }
    goto L_9344;
L_931a:;
    f_3918(v_4, v_8, *(int *)(g_ddb4 + 8) >> 3, *(int *)(g_ddb4 + 0xc) >> 3);
    goto L_9377;
L_9344:;
    g_dd50 -= 0x10;
    *(unsigned char *)(((unsigned char *)g_dd80) + ((v_4 * 2) + 1)) = *(unsigned char *)&g_dd50;
    f_3ac0(v_4, v_8, g_dd50);
L_9377:;
    f_c26a(0xc, 0x19);
    *(int *)g_8e10 = (int)g_5e68;
    goto L_94ec;
L_9392:;
    if (g_dd50 >= 0x91) {
        if (g_dd50 <= 0x100) goto L_93af;
    }
    goto L_9513;
L_93af:;
    *(int *)g_8e10 = (int)g_6098;
    if ((*(unsigned char *)(g_de5c + 1) & 0x10) != 0) {
        f_3918(v_4, v_8, *(int *)(g_ddb4 + 8) >> 3, *(int *)(g_ddb4 + 0xc) >> 3);
        goto L_94ec;
    }
    if (g_dd50 >= 0xf5) {
        if (g_dd50 <= 0xf7) goto L_940b;
    }
    goto L_942b;
L_940b:;
    f_c26a(0x2e, 0x32);
    g_ddb8 = -1;
    f_4394();
    goto L_94ec;
L_942b:;
    if (g_dd50 >= 0xfa) {
        if (g_dd50 <= 0x100) goto L_9448;
    }
    goto L_94e0;
L_9448:;
    f_c26a(1, 9);
    if (--((BF1_2_6 *)(((unsigned char *)g_dd80) + (v_4 * 2)))->f != 0) goto L_94d4;
        *(int *)(g_dddc + 0x14) += 2 << *(int *)(g_dee4 + 0x24);
        f_3e0f(g_dd50, v_4 + v_8);
        f_3918(v_4, v_8, *(int *)(g_ddb4 + 8) >> 3, *(int *)(g_ddb4 + 0xc) >> 3);
L_94d4:;
    *(int *)g_8e10 = (int)g_5e68;
    goto L_94ec;
L_94e0:;
    f_c26a(0x2a, 0x2d);
L_94ec:;
    f_7e62(a0, a1, 0, 0, (int)g_df34, *(int *)g_8e10, 1);
    goto L_9581;
L_9513:;
    g_8e20 = 0;
    goto L_95de;
L_9522:;
    if (*(int *)(g_ddb4 + 0xc) > 0) {
        *(int *)(g_ddb4 + 4) += 0x80;
    } else {
        *(int *)(g_ddb4 + 4) -= 0x80;
    }
    if (*(int *)(g_ddb4 + 8) > 0) {
        *(int *)g_ddb4 += 0x100;
        goto L_9575;
    }
    if (*(int *)(g_ddb4 + 8) >= 0) goto L_9575;
    *(int *)g_ddb4 -= 0x100;
L_9575:;
    g_8e20 = 3;
    goto L_95de;
L_9581:;
    g_8e20 = 1;
    a0 &= 0xf;
    if (a0 > 7) {
        a0 = 0xf - a0;
    }
    a1 = (a1 * 2) & 0xf;
    if (a1 > 7) {
        a1 = 0xf - a1;
    }
    if (a1 > a0) {
        g_8e20 = 2;
    }
    g_dd58 = v_4;
    g_dd5c = v_8;
L_95de:;
    return g_8e20;
}
