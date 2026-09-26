/* Draft lifted from original instructions; verify with tools/check.py. */
extern int g_e219;
extern int g_e21d;
extern int g_e221;
extern int g_e225;
void f_b57a(int a0, int a1, int a2, int a3)
{
    int v_4;
    if (a0 > a2) {
    v_4 = a0;
    a0 = a2;
    a2 = v_4;
    }
L_b5a0:;
    if (a1 > a3) {
    v_4 = a1;
    a1 = a3;
    a3 = v_4;
    }
L_b5ba:;
    g_e219 = a0;
    g_e21d = a2;
    g_e221 = a1;
    g_e225 = a3;
}
