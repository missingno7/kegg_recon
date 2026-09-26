#include <i86.h>
#include <stdlib.h>
extern short g_75ca, g_75c8;
extern unsigned char g_e382;
extern void f_9afc(void);
extern void f_9b44(int);
extern void f_e028(void);
void f_dfc3(void) {
    union REGS regs;
    if (g_75ca != -1) {
        regs.h.ah = 0x0f;
        int386(0x10, &regs, &regs);
        g_e382 = regs.h.al;
        if (g_75c8 == 0) {
            free((void *)f_e028);
            g_75c8 = -1;
        }
        g_75ca = -1;
    }
}
