extern short g_7b14, g_7b16;
extern int g_e336[];
extern unsigned char g_e356[];
extern void f_ee65(int), f_eda0(void);
void f_ed38(void) {
    g_7b14 = g_7b16;
    if (g_e356[g_7b14] == 8) f_ee65(g_e336[g_7b14] << 2);
    else f_ee65(g_e336[g_7b14]);
    f_eda0();
}
