extern int g_8e20;
extern unsigned char *g_dddc;
extern unsigned char g_e13b, g_e13c, g_e13e, g_e140, g_e142, g_e143;
extern unsigned char g_df04[];
extern void f_6ca(int, int, int);
extern int f_dd53(int, int);
void f_3b2(void) {
    g_e143 = (unsigned char)(g_e140 * 10 - 1);
    g_8e20 = g_e13c;
    if (g_e13b) {
        g_e143 = (unsigned char)(f_dd53(0, 0x3b) - 1);
        g_8e20 = 0;
    }
    g_dddc = g_df04;
    f_6ca(1, g_8e20, 0);
    if (g_e140) {
        g_e13e = 1;
    } else {
        g_e13e = 0;
    }
    g_e142 = 0;
}
