extern int *g_dddc;
extern int g_8db0;
void f_6ca(int a0, int a1, int a2) {
    g_dddc[0] = a0;
    g_dddc[1] = a1 * 113;
    g_dddc[5] = a2;
    g_8db0 = 0x7cf;
}
