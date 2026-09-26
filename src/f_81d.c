/* Draft lifted from original instructions; verify with tools/check.py. */
extern int g_68fc;
extern int g_6900;
extern short g_747c;
extern int g_7c08;
extern int g_8428;
extern int g_dd44;
extern int g_dd48;
extern int g_e4c8;
extern int g_e4d0;
extern int f_1085a();
extern int f_111de();
extern int f_a574();
extern int f_c14b();
void f_81d(void)
{
    g_e4d0 = g_7c08;
    g_dd48 = g_e4d0;
    g_8428 = f_a574(g_68fc);
    if (g_8428) {
    f_111de(g_8428, g_68fc);
    }
L_86d:;
    if ((short)g_747c != -1) goto L_8ce;
    g_dd44 = g_e4d0;
    g_8428 = f_1085a(g_6900);
    if (g_8428) {
    f_111de(g_8428, g_6900);
    }
L_8b3:;
    f_c14b(g_dd44, g_e4c8, 8000, 0);
L_8ce:;
}
