#include <stdlib.h>
extern short g_7474;
extern int g_7476;
int f_ca6a(void)
{
    if (getenv("WINDIR")) {
        g_7476 = 0x310;
        g_7474 = -1;
        return g_7474;
    } else {
        g_7474 = 0;
        return g_7474;
    }
}
