extern signed short g_747c;
extern unsigned char g_7486, g_74da, g_74db, g_75a8, g_75ac;
extern signed short g_74c4;
extern void f_d656(void *, void (*)(void));
extern void f_bd8b(void);

void f_bd24(void)
{
    if (g_747c == -1) {
        g_74db = g_7486 + g_75a8;
        g_74da = g_74db;
        if (g_7486 >= 8)
            g_74da += g_75ac - 8 - g_75a8;
        f_d656(&g_74c4, f_bd8b);
    }
}
