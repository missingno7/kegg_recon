extern short g_7b14, g_7b16;
extern int g_e336[], g_e35e;
extern void f_ee65(int), f_eda0(void);
void f_e914(int a, int b) {
    g_7b14 = g_7b16;
    f_ee65(b * g_e35e + g_e336[g_7b14] + a);
    f_eda0();
}
