/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern int g_389a;
extern unsigned char g_43ec[];
extern unsigned char g_4bf4[];
extern short g_746c;
extern short g_7b14;
extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int g_dd4c;
extern int g_df6c;
extern int g_e35a;
extern int g_e36e;
extern int g_e372;
extern int g_e376;
extern int g_e37a;
extern void f_111de(unsigned, unsigned);
extern void f_12f9c(int, int, int, int, int);
extern int f_20f4(void);
extern void f_b1df(int, int, int);
extern void f_b541(int, int, unsigned char, int, int);
extern void f_b57a(int, int, int, int);
void f_2565(volatile int a0)
{
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    g_7b16 = g_7b14;
    f_b541((int)g_43ec, g_dd4c, 0, 0x11, 0x17);
    f_b57a(g_e36e, g_e372, g_e376, g_e37a);
    f_b1df(0x40, 0xa, g_389a);
    f_b541((int)g_4bf4, g_dd4c, 0, 0xa, 0xd);
    f_b57a(g_e36e, g_e372, g_e376, g_e37a);
    f_b1df(7, 0x1e, a0);
    if (g_746c == 0) goto L_2675;
    f_111de(g_746c, 0);
L_2675:;
    g_df6c = 0x834;
    f_20f4();
}
