extern unsigned char *g_dd40;
extern unsigned char *g_dd48;
extern int g_e35a;
extern short g_7b14, g_7b20;
extern void f_ea9f(void *, int, int, int);
extern void f_a810(void *, int);
extern void f_ee33(int);
void f_8d3(void) {
    g_dd40 = g_dd48 + g_e35a;
    f_ea9f(g_dd40, 0, -63, -1);
    g_7b14 = g_7b20;
    f_a810(g_dd48, g_7b14);
    f_ee33(g_7b14);
    f_ea9f(g_dd40, -63, 0, 1);
}
