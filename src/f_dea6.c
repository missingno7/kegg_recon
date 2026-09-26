#include <i86.h>
extern unsigned int g_75c4, g_e318, g_e31c, g_e320;
unsigned int f_dea6(int bytes) {
    union REGS regs;
    regs.w.ax = 0x100;
    regs.w.bx = (bytes + 15) >> 4;
    int386(0x31, &regs, &regs);
    if (regs.x.cflag != 0) {
        g_e31c = ((unsigned int)regs.w.bx << 4) | 1;
        g_75c4 = 0x504;
        return 0;
    }
    if (bytes == 0)
        g_75c4 = 0x504;
    else
        g_75c4 = 0;
    g_e318 = regs.w.ax;
    g_e31c = regs.w.dx;
    g_e320 = g_e318 << 4;
    return g_e320;
}

