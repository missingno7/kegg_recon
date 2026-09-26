#include <i86.h>
#include <string.h>
extern unsigned int g_75b8;
unsigned int f_de3e(void) {
    union REGS regs;
    struct SREGS sregs;
    unsigned char info[48];
    memset(&sregs, 0, 12);
    regs.x.eax = 0x500;
    sregs.es = FP_SEG(info);
    regs.x.edi = (unsigned int)info;
    int386x(0x31, &regs, &regs, &sregs);
    g_75b8 = *(unsigned int *)info;
    return g_75b8;
}

