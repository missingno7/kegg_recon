/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
typedef struct { int a, b, c; short d; int e; } S18;
extern unsigned char g_67b4[];
extern unsigned char g_8e4c[];
extern unsigned char g_91ac[];
extern unsigned char *g_9478;
extern int g_9480;
extern int g_9488;
extern int g_9490;
extern int g_94a8;
extern unsigned char *g_dd4c;
extern unsigned char *g_e2ec;
extern short g_e4c4;
extern short g_e4c6;
extern void f_788d(void);
extern void f_7e62(int, int, int, int, int, int, int);
extern int f_b17e(void *, void *);
extern int f_b76c(S18, int, int, S18, int, int, int, int, int, int);
extern void f_c20d(int);
void f_7551(void)
{
    S18 *v_4;
    S18 *v_8;
    g_9478 = g_8e4c;
    g_9480 = 0;
L_7571:;
    if (g_9480 < g_9488) goto L_758b;
    return;
L_7583:;
    ++g_9480;
    goto L_7571;
L_758b:;
    *(int *)(g_9478 + 4) += *(int *)(g_9478 + 0xc);
    *(int *)(g_9478 + 8) += *(int *)(g_9478 + 0x10);
    f_b17e(g_9478 + 0x18, g_9478 + 0x1c);
    v_8 = (S18 *)(*(int *)*(unsigned char * *)(g_9478 + 0x1c) + *(unsigned char * *)(g_9478 + 0x14));
    if ((*(int *)(g_9478 + 4) >> 4) > 0x150) goto L_7600;
    if ((*(int *)(g_9478 + 4) >> 4) >= -0x10) goto L_7602;
L_7600:;
    goto L_7614;
L_7602:;
    if ((*(int *)(g_9478 + 8) >> 4) <= 0xd8) goto L_7616;
L_7614:;
    goto L_7626;
L_7616:;
    if ((*(int *)(g_9478 + 8) >> 4) >= -0x10) goto L_7630;
L_7626:;
    f_788d();
    goto L_7583;
L_7630:;
    if (*(int *)g_9478 != 0) goto L_7771;
    v_4 = (S18 *)(g_dd4c + 0x395c);
    if (f_b76c(*v_4, 0xf6, 0x46,
               *v_8,
               *(int *)(g_9478 + 4) >> 4, *(int *)(g_9478 + 8) >> 4,
               0xa, 0, 0xa, 0) != -1) goto L_7701;
    g_94a8 -= *(int *)(g_9478 + 0x20);
    f_7e62(*(int *)(g_9478 + 4) >> 4, *(int *)(g_9478 + 8) >> 4, *(int *)(g_9478 + 0xc) >> 6, *(int *)(g_9478 + 0x10) >> 6, (int)g_dd4c, (int)g_67b4, 1);
    f_788d();
    goto L_7583;
L_7701:;
    if (((*(int *)(g_9478 + 4) >> 4) + 5) <= *(int *)(g_91ac + ((*(int *)(g_9478 + 8) >> 4) << 2))) goto L_776c;
    f_c20d(0x57);
    f_7e62(*(int *)(g_9478 + 4) >> 4, *(int *)(g_9478 + 8) >> 4, 0, 0, (int)g_dd4c, (int)g_67b4, 1);
    f_788d();
    goto L_7583;
L_776c:;
    goto L_7835;
L_7771:;
    v_4 = (S18 *)(g_dd4c + 0x35d6);
    if (f_b76c(*v_4,
               g_e4c6, g_e4c4,
               *v_8,
               (*(int *)(g_9478 + 4) >> 4) + 8, *(int *)(g_9478 + 8) >> 4,
               4, 4, 4, 4) != -1) goto L_7835;
    if (g_9490 <= 0) goto L_77f9;
    g_9490 -= *(int *)(g_9478 + 0x20);
L_77f9:;
    f_7e62(*(int *)(g_9478 + 4) >> 4, *(int *)(g_9478 + 8) >> 4, 0, 0, (int)g_dd4c, (int)g_67b4, 1);
    f_788d();
    goto L_7583;
L_7835:;
    *(int *)g_e2ec = v_8;
    *(short *)(g_e2ec + 4) = (unsigned short)(*(int *)(g_9478 + 4) >> 4);
    *(short *)(g_e2ec + 6) = (unsigned short)(*(int *)(g_9478 + 8) >> 4);
    *(short *)(g_e2ec + 8) = 0;
    g_e2ec += 0xa;
    g_9478 += 0x24;
    goto L_7583;
}
