extern signed short g_747c, g_742c;
extern int g_7468;
extern int g_7448, g_7458, g_745c, g_744c;
extern short g_7db0;
extern void f_11358(void);
extern void f_11365(void);
extern void f_11377(int);
extern void f_112fa(void);

void f_c2d0(int value)
{
    if (g_747c == -1) {
        if (value == 0) {
            if (g_7468 != 0)
                g_7468 = 0;
        } else if (value == -1) {
            if (g_7468 != -1)
                g_7468 = -1;
        }
    }
}
