extern short g_7b1a;
extern short g_7b16;
extern short g_7b18;
extern short g_746c;
extern unsigned short g_746e;
extern int g_df38;
extern int g_e35a;

void f_e855(int);
void f_7f8a(void);
void f_12f9c(int, int, int, int, int);
void f_111de(int, int);

void f_2f0c(void)
{
    f_e855(g_7b1a);
    g_746e = 8;
    g_7b16 = g_7b1a;
    ++g_df38;
    if (g_df38 >= 0x29)
        g_df38 = 0;
    f_7f8a();
    f_12f9c(g_7b1a, 0, g_7b18, 0, g_e35a);
    if (g_746c != 0)
        f_111de(g_746c, 0);
}
