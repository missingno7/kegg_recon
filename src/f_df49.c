#include <i86.h>
void f_df49(unsigned int value) {
    union REGS regs;
    if (value != 0) {
        regs.w.ax = 0x101;
        regs.w.dx = value;
        int386(0x31, &regs, &regs);
    }
}
