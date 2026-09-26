extern short g_7b14;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern short g_7b34;
extern int g_e35a;
extern int g_7c08;
extern int g_e4d0;
extern int g_e4c8;
extern int g_dd44;
extern int g_e150;
extern int g_6c08;

void f_12f9c(int, int, int, int, int);
void f_c3ab(void);
void f_2eab(void);
void f_c14b(int, int, int, int);
void f_35b5(void);
void f_84a0(int);

void f_2ccc(void)
{
    f_12f9c(g_7b14, 0, g_7b18, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    f_c3ab();
    g_e4d0 = g_7c08;
    f_2eab();
    f_c14b(g_dd44, g_e4c8, 0x1f40, -1);
    g_e150 = (int)&g_6c08;
    f_35b5();
    g_7b34 = ((*(short *)&g_7b34) & 0xfffe) & 0xfffd;
    f_84a0(0x8ca);
    f_c3ab();
}
