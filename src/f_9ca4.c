#include <conio.h>
extern int g_e1ac, g_e164, g_73a8, g_e1b0;
short f_9ca4(void)
{
    unsigned short lo, hi;
    outp(0x43, 0);
    lo = inp(0x40);
    hi = inp(0x40);
    g_e164 = g_e1ac;
    g_e1ac = ((unsigned short)hi << 8) + (unsigned short)lo;
    if (g_e1ac < g_e164) {
    } else {
        g_e164 += g_73a8;
    }
    g_e1b0 = (g_e164 - g_e1ac) * 100 / 0x4280;
    return g_e1b0;
}

