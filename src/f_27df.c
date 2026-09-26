extern short g_7b14;
extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern short g_746c;
extern int g_dd4c;
extern int g_df6c;
extern int g_389a;
extern int g_e35a;
extern int g_e36e;
extern int g_e372;
extern int g_e376;
extern int g_e37a;
extern char g_43ec[];
extern char g_4bf4[];
extern void f_12f9c(int, int, int, int, int);
extern void f_b541(char *, int, unsigned char, int, int);
extern void f_b57a(int, int, int, int);
extern void f_b1df(int, int, int);
extern void f_111de(int, int);
extern void f_20f4(void);

void f_27df(volatile int a)
{
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    g_7b16 = g_7b14;
    f_b541(g_43ec, g_dd4c, 0, 17, 23);
    f_b57a(g_e36e, g_e372, g_e376, g_e37a);
    f_b1df(0x40, 10, g_389a);
    f_b541(g_4bf4, g_dd4c, 0, 10, 13);
    f_b57a(g_e36e, g_e372, g_e376, g_e37a);
    f_b1df(7, 30, a);
    if (g_746c != 0)
        f_111de(g_746c, 0);
    g_df6c = 0x5208;
    f_20f4();
}


