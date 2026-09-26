#include <i86.h>
#include <string.h>
extern int g_7492;
extern short g_7490;
int f_d288(void) {
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    regs.w.ax = 0x3000;
    int386x(0x21, &regs, &regs, &sregs);
    g_7492 = (regs.h.al << 8) + regs.h.ah;
    return g_7490 = -1;
}

