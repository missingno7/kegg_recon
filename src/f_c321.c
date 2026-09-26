/* Draft lifted from original instructions; verify with tools/check.py. */
extern int g_7420;
extern int g_7430;
extern int g_744c;
extern int g_7450;
extern int g_7458;
extern int g_745c;
extern int g_7460;
extern int g_7464;
extern int g_7db4;
extern short g_7db8;
extern int g_e26c;
extern int f_1133f();
extern int f_11377();
extern int f_c3fb();
extern int f_c621();
void f_c321(void)
{
    g_7db4 = g_e26c;
    g_7db8 = g_7420;
    f_1133f();
    g_7430 = 0;
    g_7458 = (g_7420 >> 1);
    g_7464 = 0;
    g_7460 = g_7450;
    if (g_745c != g_744c) {
    g_745c = g_744c;
    f_11377(g_745c);
    }
L_c397:;
    f_c3fb();
    f_c621();
    f_c3fb();
}
