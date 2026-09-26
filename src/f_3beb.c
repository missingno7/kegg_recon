extern short g_7b14;
extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int g_e35a;

void f_12f9c(int, int, int, int, int);
void f_ed38(void);
void f_9d40(unsigned char);
void f_ec76(int);

void f_3beb(int a)
{
    short value;
    if (g_7b14 != g_7b20)
        value = g_7b20;
    else
        value = g_7b22;
    g_7b16 = value;
    f_12f9c(g_7b18, 0, g_7b16, 0, g_e35a);
    f_ed38();
    f_9d40(0);
    f_ec76(a);
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
}
