/* Draft lifted from original instructions; verify with tools/check.py. */
extern short g_7b39;
extern int (*g_7b3f)(void);
extern int (*g_7b43)(void);
extern char g_e48e;
extern int f_fa42();
void f_fc1e(void)
{
    if (g_e48e != 25) goto L_fc81;
    g_7b3f();
    g_7b39 = 65535;
L_fc42:;
    while (g_e48e == 25) {
    f_fa42();
    }
L_fc52:;
    while (g_e48e != 25) {
    f_fa42();
    }
L_fc62:;
    while (g_e48e == 25) {
    f_fa42();
    }
L_fc72:;
    g_7b43();
    g_7b39 = 0;
L_fc81:;
}
