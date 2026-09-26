extern int g_e150;
extern int g_6ae8;
extern int g_6b78;
extern int g_e14c;
extern short g_746c;
extern short g_7b20;
extern short g_7b22;
extern int g_e35a;
extern short g_7b18;

void f_843a(void);
void f_34b7(void);
void f_84a0(int);
void f_111de(int, int);
void f_12f9c(int, int, int, int, int);

void f_3406(void)
{
    f_843a();
    g_e150 = (int)&g_6ae8;
    f_34b7();
    f_84a0(0x78);
    if (g_e14c == 0x78) {
        g_e150 = (int)&g_6b78;
        f_34b7();
        if (g_746c != 0)
            f_111de(g_746c, 0);
        f_84a0(0x32);
    }
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
}
