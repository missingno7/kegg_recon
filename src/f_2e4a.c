extern int g_e4d0;
extern int g_df40;
extern int g_df30;
extern int g_dd44;
extern short g_747c;
extern int g_6954;
extern int g_6970;

void f_1085a(int);

void f_2e4a(void)
{
    g_df40 = g_e4d0;
    g_df30 = g_e4d0;
    f_1085a(g_6954);
    g_dd44 = g_e4d0;
    if (g_747c == -1)
        f_1085a(g_6970);
    g_e4d0 = g_df40;
}
