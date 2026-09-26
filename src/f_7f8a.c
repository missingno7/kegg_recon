extern int g_e36a, g_e366, g_df38;
extern unsigned char *g_ddd8;
extern int g_2e9c[];
extern void f_12cbd(unsigned char *, int, int);

void f_7f8a(void)
{
    int x;
    int y;
    unsigned char *p;
    y = 16;
    while (y < g_e36a) {
        x = 0;
        while (x < g_e366) {
            p = g_ddd8 + g_2e9c[2 * g_df38];
            f_12cbd(p, x, y);
            x += *(short *)(p + 2);
        }
        y += *(short *)(p + 4);
    }
}
