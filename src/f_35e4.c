extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int *g_dddc;
extern int g_df20;
extern int g_e35a;
extern int g_e36e;
extern int g_e372;
extern int g_e376;
extern int g_e37a;
extern char g_269c[];
extern int g_37fd;

void f_b541(char *, int, unsigned char, int, int);
void f_b57a(int, int, int, int);
void f_b4a7(int, int, int, int, int);
void f_12f9c(int, int, int, int, int);

void f_35e4(void)
{
    f_b541(g_269c, g_df20, 0, 8, 8);
    f_b57a(g_e36e, g_e372, g_e376, g_e37a);
    g_7b16 = g_7b18;
    g_dddc[2] = -1;
    g_dddc[6] = -1;
    f_b4a7(0x38, 4, g_dddc[5], 10, 6);
    f_b4a7(0x99, 4, g_dddc[1] / 0x71, 10, 2);
    f_b4a7(0x108, 4, g_37fd, 10, 6);
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    g_7b16 = g_7b20;
}
