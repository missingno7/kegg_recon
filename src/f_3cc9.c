extern int g_dd74;
extern int g_dd7c;
extern int g_9af8;
extern int g_dd54;
void f_3d23(void);
void f_3cc9(void)
{
    g_dd7c = (int)&g_9af8;
    for (g_dd54 = 0; g_dd54 < g_dd74; ++g_dd54) {
        if (--((int *)g_dd7c)[1] == 0) {
            f_3d23();
            continue;
        } else {
            g_dd7c += 9;
        }
    }
}
