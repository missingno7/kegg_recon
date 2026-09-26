#include <i86.h>
#include <string.h>
extern short g_7498;
int f_d2f0(void) {
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    regs.w.ax = 0x1a00;
    int386x(0x10, &regs, &regs, &sregs);
    if (regs.h.al == 0x1a)
        if (regs.h.bl >= 7 && regs.h.bl <= 0xc)
            return g_7498 = -1;
    return g_7498 = 0;
}

