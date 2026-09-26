extern int g_7470;
extern short g_746e;
extern int g_e2d4;
extern int g_e2d8;
extern int g_e2dc;
extern int g_e2ec;
extern int g_e2e8;
void f_c8c0(int a, int b, int c, int d, int e) {
    g_7470 = b;
    g_e2dc = a;
    g_e2ec = g_e2dc;
    g_746e = c;
    if (g_746e == 4) {
        g_e2d4 = d;
        g_e2e8 = g_e2d4;
        *(short *)g_e2e8 = 0;
        g_e2d8 = e;
        g_e2e8 = g_e2d8;
        *(short *)g_e2e8 = 0;
    }
}
