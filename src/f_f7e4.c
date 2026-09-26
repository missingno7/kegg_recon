extern short g_7b6f;
extern unsigned char g_e48c;
#include <stdlib.h>
extern void f_f82d(void);
void f_f7e4(void) {
    int p;
    p = 0x417;
    if (g_7b6f != -1) {
        g_e48c = *(unsigned char *)p >> 4;
        atexit(f_f82d);
        g_7b6f = -1;
    }
}
