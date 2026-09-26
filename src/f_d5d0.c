#include <i86.h>
#include <string.h>
extern unsigned int g_74be;
extern short g_74bc;
int f_d5d0(void) {
    union REGS regs;
    struct SREGS sregs;
    if (*(unsigned int *)0x19c != 0) {
        memset(&sregs, 0, 12);
        regs.w.ax = 0xde00;
        int386x(0x67, &regs, &regs, &sregs);
        if (regs.h.ah == 0) {
            g_74be = regs.w.bx;
            return g_74bc = -1;
        }
    }
    return g_74bc = 0;
}
