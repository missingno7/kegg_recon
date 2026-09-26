extern unsigned char g_e48b, g_e48e;
extern short g_e3fc, g_e3fe, g_e418, g_e41a, g_e408, g_e40e, g_e424, g_e42a;
extern int outp(int, int), inp(int);
extern void f_efa0(void), f_fa42(void), f_f46c(void);
void f_f306(void) {
    do { outp(0x201, 0xff); } while ((inp(0x201) & 0xf0) != 0xf0);
    do { f_efa0(); f_fa42(); }
    while (g_e48b == g_e48e && (inp(0x201) & 0xf0) == 0xf0);
    g_e408 = g_e3fc;
    g_e40e = g_e3fe;
    g_e424 = g_e418;
    g_e42a = g_e41a;
    f_f46c();
    outp(0x201, 0xff);
}
