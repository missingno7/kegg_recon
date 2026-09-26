extern int g_ddc0;
extern int g_dda4;
extern short g_7474;
extern int g_60f0[];
extern char g_a990[];
void f_c20d(int);
void f_42e1(unsigned char a, unsigned char b, int c, int d)
{
    if (g_ddc0 < 0x18 && a < 0x1c) {
        g_dda4 = (int)g_a990;
        g_dda4 += g_ddc0 * 0x12;
        if (g_7474 == -1)
            g_ddc0 = 0x1388;
        *(int *)g_dda4 = c;
        *(int *)(g_dda4 + 4) = d;
        *(int *)(g_dda4 + 0xc) = g_60f0[a];
        *(int *)(g_dda4 + 8) = 0;
        *(unsigned char *)(g_dda4 + 0x10) = b;
        *(unsigned char *)(g_dda4 + 0x11) = a;
        f_c20d(0x1e);
        ++g_ddc0;
    }
}
