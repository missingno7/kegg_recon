extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern short g_746e;
extern int *g_dddc;
extern int g_8db0;
extern int g_df20;
extern int g_e36e;
extern int g_e372;
extern int g_e376;
extern int g_e37a;
extern char g_269c[];

void f_c20d(int);
void f_b541(char *, int, unsigned char, int, int);
void f_b57a(int, int, int, int);
void f_b4a7(int, int, int, int, int);
void f_130b7(int, int, int, int, int, int, int, int);

void f_3702(void)
{
    unsigned char saved_7b16;
    int saved_746e;

    saved_7b16 = (unsigned char)g_7b16;
    saved_746e = g_746e;
    g_746e = 8;
    if (g_8db0 < g_dddc[5]) {
        g_dddc[1] += 0x71;
        g_8db0 += 0x7d0;
        f_c20d(0x3c);
    }

    if (g_dddc[5] != g_dddc[6] || g_dddc[1] != g_dddc[2]) {
        f_b541(g_269c, g_df20, 0, 8, 8);
    }
    f_b57a(g_e36e, g_e372, g_e376, g_e37a);

    if (g_dddc[5] != g_dddc[6]) {
        g_dddc[6] = g_dddc[5];
        g_7b16 = g_7b18;
        f_b4a7(0x38, 4, g_dddc[5], 10, 6);
        f_130b7(g_7b18, 0x38, 4, 0x66, 0xb, g_7b20, 0x38, 4);
        f_130b7(g_7b18, 0x38, 4, 0x66, 0xb, g_7b22, 0x38, 4);
    }

    if (g_dddc[1] != g_dddc[2]) {
        g_dddc[2] = g_dddc[1];
        g_7b16 = g_7b18;
        f_b4a7(0x99, 4, g_dddc[1] / 0x71, 10, 2);
        f_130b7(g_7b18, 0x99, 4, 0xa6, 0xb, g_7b20, 0x99, 4);
        f_130b7(g_7b18, 0x99, 4, 0xa6, 0xb, g_7b22, 0x99, 4);
    }

    g_7b16 = saved_7b16;
    g_746e = saved_746e;
}
