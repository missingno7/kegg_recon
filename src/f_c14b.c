/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern short g_742c;
extern short g_742e;
extern int g_7434;
extern int g_7438;
extern int g_743c;
extern int g_7440;
extern int g_7444;
extern int g_7448;
extern int g_744c;
extern int g_7450;
extern short g_747c;
extern unsigned char g_74c4[];
extern void f_111de(unsigned, unsigned);
extern int f_bdd3(int);
extern void f_c321(void);
extern void f_c3ab(void);
void f_c14b(int a0, int a1, int a2, int a3)
{
    volatile int v_4;
    if (g_747c != -1) goto L_c208;
    if (*(short *)g_74c4 == -1) goto L_c194;
    v_4 = f_bdd3(4);
    if (v_4 == 0) goto L_c194;
    f_111de(v_4, 0);
L_c194:;
    if (g_742e != -1) goto L_c1b1;
    if (g_742c != -1) goto L_c1b1;
    f_c3ab();
L_c1b1:;
    if (g_742c != 0) goto L_c208;
    g_7434 = a0;
    g_7438 = a1;
    g_743c = a2;
    g_7440 = a3;
    g_7444 = g_7434;
    g_7448 = g_7438;
    g_744c = g_743c;
    g_7450 = g_7440;
    f_c321();
L_c208:;
}
