#include <stdlib.h>
extern int g_e314, g_75bc;
int f_ddb9(int handle) {
    if (handle == 0) {
        g_e314 = 1;
        g_75bc = 0x505;
    } else {
        g_e314 = (int)malloc(handle);
        if (g_e314 == 0)
            g_75bc = 0x505;
        else
            g_75bc = 0;
    }
    return g_e314;
}
