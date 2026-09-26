extern int g_e4d0;
extern int g_df40;
extern int g_ddd8;
extern int g_6950;
extern int g_df30;
extern int g_6954;
extern int g_df28;
extern int g_5c78;
extern int g_e4c8;
extern int g_7c08;
extern int g_dd44;
extern short g_747c;
extern int g_6968;

void f_1085a(int);

void f_2da3(void)
{
    g_df40 = g_e4d0;
    g_ddd8 = g_e4d0;
    f_1085a(g_6950);
    g_df30 = g_e4d0;
    f_1085a(g_6954);
    g_df28 = g_e4d0;
    f_1085a(g_5c78);
    if (g_e4c8 != 0x8958)
        g_7c08 = 0;
    g_dd44 = g_e4d0;
    if (g_747c == -1)
        f_1085a(g_6968);
    g_e4d0 = g_df40;
}
