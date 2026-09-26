extern short g_75cc;
extern unsigned char g_e380, g_e37f, g_e383;
extern void f_13a0a(unsigned int);
extern void f_139c4(unsigned int);
extern void f_13a29(unsigned int);
void f_e41d(void) {
    if (g_75cc == -1) {
        f_13a0a(g_e380);
        f_139c4(g_e37f);
        f_13a29(g_e383);
        g_75cc = 1;
    }
}
