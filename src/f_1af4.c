/* Draft lifted from original instructions; verify with tools/check.py. */
extern int g_6918;
extern int g_691c;
extern int g_6920;
extern short g_747c;
extern int g_7c08;
extern int g_dd44;
extern int g_dd48;
extern int g_e4c8, g_dd40, g_df24, g_df34, g_df2c, g_df20, g_dd48, g_dd4c, g_ddc8;
extern int g_e4c8;
extern int g_e4d0;
extern int f_1085a();
extern int f_a574();
extern int f_c14b();
void f_1af4(void)
{
    g_e4d0 = g_7c08;
    g_dd48 = g_e4d0;
    f_a574(g_6918);
    g_dd4c = g_e4d0;
    f_1085a(g_691c);
    if ((short)g_747c != -1) goto L_1b79;
    g_dd44 = g_e4d0;
    f_1085a(g_6920);
    f_c14b(g_dd44, g_e4c8, 8000, -1);
L_1b79:;
}
