extern int g_e35a;
extern short g_7b20;
extern short g_7b22;
extern short g_7b18;
extern int *g_dee4;

void f_c3ab(void);
void f_61a6(void);
void f_12f9c(int, int, int, int, int);
void f_10502(int, int);

void f_353f(void)
{
    f_c3ab();
    f_61a6();
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    f_10502(g_dee4[0], g_dee4[1]);
}
