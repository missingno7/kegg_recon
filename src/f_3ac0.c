extern short g_7b16;
extern short g_746e;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int g_df24;
extern int g_e158;
extern unsigned char g_2fe4[];

void f_12cbd(int, int, int);

void f_3ac0(int a, int b, int c)
{
    unsigned char saved_7b16;
    int x;
    int y;
    int saved_746e;

    saved_7b16 = (unsigned char)g_7b16;
    saved_746e = g_746e;
    g_746e = 8;
    x = a * 16 + 16;
    y = b / 18 * 8 + 24;
    g_e158 = g_df24 + *(int *)(g_2fe4 + c * 8);
    g_7b16 = g_7b18;
    f_12cbd(g_e158, x, y);
    g_7b16 = g_7b20;
    f_12cbd(g_e158, x, y);
    g_7b16 = g_7b22;
    f_12cbd(g_e158, x, y);
    g_746e = saved_746e;
    g_7b16 = saved_7b16;
}
