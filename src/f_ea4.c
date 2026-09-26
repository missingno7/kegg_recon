/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern int g_68f8;
extern int g_6904;
extern int g_6908;
extern int g_690c;
extern short g_747c;
extern int g_7c08;
extern int g_8428;
extern unsigned char *g_dd40;
extern int g_dd44;
extern unsigned char *g_dd48;
extern unsigned char *g_dd4c;
extern int g_df54;
extern int g_e4c8;
extern unsigned char *g_e4d0;
extern int f_1085a(void *);
extern void f_111de(unsigned, unsigned);
extern int f_a574(int);
extern void f_c14b(int, int, int, int);
void f_ea4(void)
{
    g_e4d0 = (unsigned char *)g_7c08;
    g_dd48 = g_e4d0;
    g_8428 = f_a574(g_6904);
    if (g_8428 == 0) goto L_ef4;
    f_111de(g_8428, g_6904);
L_ef4:;
    g_dd4c = g_e4d0;
    g_8428 = f_1085a((void *)g_6908);
    if (g_8428 == 0) goto L_f2e;
    f_111de(g_8428, g_6908);
L_f2e:;
    if (g_747c != -1) goto L_f8f;
    g_dd44 = (int)g_e4d0;
    g_8428 = f_1085a((void *)g_690c);
    if (g_8428 == 0) goto L_f74;
    f_111de(g_8428, g_690c);
L_f74:;
    f_c14b(g_dd44, g_e4c8, 0x1f40, -1);
L_f8f:;
    g_dd40 = g_e4d0 + 0xc00;
    g_8428 = f_1085a((void *)g_68f8);
    if (g_8428 == 0) goto L_fce;
    f_111de(g_8428, g_68f8);
L_fce:;
    g_df54 = (g_e4c8 - 0xc00) / 3;
}
