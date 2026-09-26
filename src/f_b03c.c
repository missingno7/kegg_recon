#include <stdlib.h>
extern int g_7404, g_7408, g_740c, g_7410, g_7414, g_e1f8;
extern float g_e1fc;
extern int f_dd53(int, int);
extern unsigned char f_13324(short, short);
extern void f_133f6(int, int, int);

void f_b03c(short x1, short y1, short x2, short y2, short x3, short y3)
{
    if (f_13324(x3, y3) != g_7404) return;
    g_e1f8 = f_dd53(g_7410, g_7414) + ((f_13324(x1, y1) + f_13324(x2, y2)) >> 1)
           + (int)(f_dd53(-g_740c >> 1, g_740c >> 1) * g_e1fc * (float)(abs(x1 - x2) + abs(y1 - y2)))
             / (g_740c >> 1);
    if (g_e1f8 < g_7404 + 1) g_e1f8 = g_7404 + 1;
    if (g_e1f8 > g_7408 - 1) g_e1f8 = g_7408 - 1;
    f_133f6(x3, y3, g_e1f8);
}
