extern signed short g_747c, g_742c;
extern int g_7468;
extern int g_7448, g_7458, g_745c, g_744c;
extern short g_7db0;
extern void f_11358(void);
extern void f_11365(void);
extern void f_11377(int);
extern void f_112fa(void);

void f_c621(void)
{
    if (g_7458 != 0) {
        if (g_745c != g_744c) {
            g_745c = g_744c;
            f_11377(g_745c);
        }
        g_7db0 = (short)g_7458;
        f_112fa();
        g_742c = -1;
    } else {
        g_742c = 0;
    }
}
