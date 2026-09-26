extern short g_746e, g_7b16;
extern void f_12cbd();
void f_c9f4(int a, int b, int c, int d, int e) {
    int old_7b16 = g_7b16;
    int old_746e = g_746e;
    g_746e = b;
    g_7b16 = a;
    f_12cbd(c, d, e);
    g_7b16 = old_7b16;
    g_746e = old_746e;
}


