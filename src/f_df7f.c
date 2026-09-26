#include <i86.h>
extern unsigned int g_75c0;
unsigned int f_df7f(void) {
    union REGS regs;
    regs.h.ah = 0x48;
    regs.w.bx = 0xffff;
    int386(0x21, &regs, &regs);
    g_75c0 = regs.w.bx << 4;
    return g_75c0;
}
