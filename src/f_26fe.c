/* Draft lifted from original instructions; verify with tools/check.py. */
extern int g_68f8, g_6940, g_6944, g_6948, g_694c, g_6958, g_695c, g_6960, g_6964;
extern int g_6928;
extern int g_692c;
extern int g_6930;
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
void f_26fe(void)
{
    g_e4d0 = g_7c08;
    g_dd48 = g_e4d0;
    f_a574(g_6928);
    f_1085a(g_68f8);
    if ((short)g_747c == -1) {
    g_dd44 = g_e4d0;
    f_1085a(g_6930);
    f_c14b(g_dd44, g_e4c8, 12000, -1);
    }
L_2779:;
    g_dd4c = g_e4d0;
    f_1085a(g_692c);
}
