/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern unsigned char g_3be4[];
extern short g_746e;
extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int g_dd4c;
extern int g_e376;
extern int g_e37a;
extern void f_130b7(int, int, int, int, int, int, int, int);
extern void f_b1df(int, int, int);
extern void f_b541(int, int, unsigned char, int, int);
extern void f_b57a(int, int, int, int);
void f_16bb(volatile int a0)
{
    unsigned char v_4;
    int v_8;
    v_4 = *(signed char *)&g_7b16;
    v_8 = g_746e;
    f_b541((int)g_3be4, g_dd4c, 1, 1, 3);
    f_b57a(0x10, 0xda, g_e376, g_e37a);
    g_746e = 8;
    g_7b16 = g_7b20;
    f_130b7(g_7b18, 0, 0xd7, 0x13f, 0xef, g_7b16, 0, 0xd7);
    f_b1df(0x10, 0xda, a0);
    g_7b16 = g_7b22;
    f_130b7(g_7b18, 0, 0xd7, 0x13f, 0xef, g_7b16, 0, 0xd7);
    f_b1df(0x10, 0xda, a0);
    g_746e = (unsigned short)v_8;
    g_7b16 = (unsigned short)v_4;
}
