/* Draft lifted from original instructions; verify with tools/check.py. */
extern short g_746c;
extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int g_df6c;
extern int g_e35a;
extern int f_111de();
extern int f_12f9c();
extern int f_1af4();
extern int f_1b7e();
extern int f_1bc2();
extern int f_1d80();
extern int f_1e87();
extern int f_20f4();
extern int f_c3ab();
extern int f_e095();
extern int f_ed38();
extern int f_ee33();
void f_19b9(void)
{
    f_c3ab();
    f_1af4();
    f_e095(1);
    f_1b7e();
    f_1d80();
    g_7b16 = g_7b18;
    f_1bc2();
    f_12f9c((short)g_7b18, 0, (short)g_7b20, 0, g_e35a);
    f_12f9c((short)g_7b18, 0, (short)g_7b22, 0, g_e35a);
    g_7b16 = g_7b22;
    f_ee33((short)g_7b16);
    f_1e87();
    g_7b16 = g_7b18;
    f_1bc2();
    f_12f9c((short)g_7b18, 0, (short)g_7b20, 0, g_e35a);
    f_12f9c((short)g_7b18, 0, (short)g_7b22, 0, g_e35a);
    g_7b16 = g_7b20;
    f_ed38();
    if (g_746c) {
    f_111de((short)g_746c, 0);
    }
L_1adb:;
    g_df6c = 1400;
    f_20f4();
    f_c3ab();
}
