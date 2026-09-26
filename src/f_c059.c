/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern void __far a_70(void);
extern short g_747c;
extern unsigned char g_7486;
extern unsigned char g_74c4[];
extern unsigned char g_74fd[];
extern int g_7516;
extern int g_751e;
extern int g_7522;
extern int g_7532;
extern unsigned g_75c4;
extern short g_e2fc;
extern void f_bfba(void);
extern int f_da01(void *);
int f_c059(int a0)
{
    unsigned char * v_4;
    if (a0 != 0) goto L_c085;
    a0 = g_7516;
    if (a0 != 0) goto L_c085;
    return 0;
L_c085:;
    if (g_747c != -1) goto L_c09d;
    if (*(short *)g_74c4 != -1) goto L_c0a9;
L_c09d:;
    return 0x606;
L_c0a9:;
    g_7516 = a0;
    f_bfba();
    g_751e = (int)a_70;
    g_7522 = (int)((unsigned char __far *)a_70 + 0x69);
    if (f_da01(g_74fd) == -1) goto L_c13c;
    if ((a0 & 1) != 1) goto L_c12a;
    v_4 = (unsigned char *)g_7532;
    *(short *)(v_4 + 2) = g_e2fc;
    *(short *)(v_4 + 4) = 0x90;
    if (g_7486 >= 8) goto L_c121;
    *(short *)(v_4 + 6) = 0x20;
    goto L_c12a;
L_c121:;
    *(short *)(v_4 + 6) = 0xa0;
L_c12a:;
    if (g_75c4 == 0) goto L_c13c;
    return 0x607;
L_c13c:;
    return 0;
}
