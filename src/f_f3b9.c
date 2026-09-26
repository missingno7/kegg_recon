extern unsigned char g_e48b, g_e48e;
extern short g_e3fc, g_e3fe, g_e418, g_e41a, g_e40c, g_e412, g_e428, g_e42e;
extern int outp(int, int), inp(int);
extern void f_efa0(void), f_fa42(void), f_f46c(void);
void f_f3b9(void) {
    do { outp(0x201, 0xff); } while ((inp(0x201) & 0xf0) != 0xf0);
    do { f_efa0(); f_fa42(); }
    while (g_e48b == g_e48e && (inp(0x201) & 0xf0) == 0xf0);
    g_e40c = g_e3fc;
    g_e412 = g_e3fe;
    g_e428 = g_e418;
    g_e42e = g_e41a;
    f_f46c();
    outp(0x201, 0xff);
}
