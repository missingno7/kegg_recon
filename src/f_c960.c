extern unsigned int g_7470;
extern short g_746c;
extern int g_e2dc, g_e2e8, g_e2ec;
extern void f_12a9c();
void f_c960(volatile int a, volatile int b) {
    f_12a9c(a, b, g_e2dc, g_e2e8);
    *(short *)g_e2e8 = 0;
    if ((g_e2ec - g_e2dc) / 10 > g_7470 * 10)
        g_746c = 0x407;
    g_e2ec = g_e2dc;
}


