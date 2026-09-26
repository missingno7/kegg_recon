#include <i86.h>
#include <string.h>
extern unsigned int g_e300, g_e304;
extern short g_74a4;
int f_d36c(void) {
    int unused;
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    regs.w.ax = 0x4300;
    int386x(0x2f, &regs, &regs, &sregs);
    if (regs.h.al == 0x80) {
        regs.w.ax = 0x4310;
        int386x(0x2f, &regs, &regs, &sregs);
        g_e300 = regs.w.bx;
        g_e304 = regs.w.ax;
        return g_74a4 = -1;
    }
    return g_74a4 = 0;
}

