extern int g_e2d4, g_e2d8, g_e2e8;
int f_c886(void) {
    g_e2e8 = g_e2d8;
    g_e2d8 = g_e2d4;
    g_e2d4 = g_e2e8;
    return g_e2d4;
}
