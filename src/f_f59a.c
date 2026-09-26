extern short g_e3fc, g_e3fe, g_e418, g_e41a;
extern short g_e408, g_e40a, g_e40c, g_e40e, g_e410, g_e412;
extern short g_e424, g_e426, g_e428, g_e42a, g_e42c, g_e42e;
extern int g_7b2a;
extern int outp(int, int);
extern void f_efa0(void), f_f46c(void);
void f_f59a(void) {
    g_e3fc = 0; g_e3fe = 0; g_e418 = 0; g_e41a = 0;
    g_7b2a = 1;
    f_efa0(); f_efa0();
    g_7b2a = 0;
    g_e408 = 0; g_e40e = 0; g_e424 = 0; g_e42a = 0;
    g_e40a = g_e3fc; g_e410 = g_e3fe; g_e426 = g_e418; g_e42c = g_e41a;
    g_e40c = g_e3fc * 2; g_e412 = g_e3fe * 2;
    g_e428 = g_e418 * 2; g_e42e = g_e41a * 2;
    f_f46c();
    outp(0x201, 0xff);
}
