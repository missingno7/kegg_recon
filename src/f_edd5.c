extern short g_7b14, g_7b20, g_7b22;
extern int g_e336[], g_e346[];
extern void f_ee65(int);
void f_edd5(void) {
    f_ee65(g_e336[g_7b14] + g_e346[g_7b14]);
    g_7b14 = g_7b20;
    g_7b20 = g_7b22;
    g_7b22 = g_7b14;
}
