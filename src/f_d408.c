#include <i86.h>
#include <string.h>
extern unsigned int g_1258, g_74ae;
extern short g_74ac;
int f_d408(void) {
    union REGS regs;
    struct SREGS sregs;
    unsigned int p;
    memset(&sregs, 0, 12);
    regs.w.ax = 0x3567;
    int386x(0x21, &regs, &regs, &sregs);
    p = ((unsigned int)sregs.es << 4) + (unsigned short)regs.x.edi;
    if (*(unsigned int *)p == (unsigned int)&g_1258) {
        regs.h.ah = 0x46;
        int386x(0x21, &regs, &regs, &sregs);
        if (regs.h.ah == 0)
            g_74ae = regs.w.ax << 4;
        return g_74ac = -1;
    }
    return g_74ac = 0;
}

