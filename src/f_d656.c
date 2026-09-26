#include <conio.h>
#include <i86.h>
#include <stdlib.h>
#include <string.h>
extern void f_14197(void *);
void f_d656(unsigned char *p, int release) {
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    if (*(short *)(p + 2) == -1)
        return;
    if (p[0x19] & 2) {
        regs.x.eax = 0x204;
        regs.h.bl = p[0x16];
        int386x(0x31, &regs, &regs, &sregs);
        *(unsigned short *)(p + 0x0a) = regs.x.ecx;
        *(unsigned int *)(p + 6) = regs.x.edx;
    }
    if (p[0x19] & 4) {
        regs.x.eax = p[0x16];
        regs.h.ah = 0x35;
        int386x(0x21, &regs, &regs, &sregs);
        *(unsigned short *)(p + 0x10) = sregs.es;
        *(unsigned int *)(p + 0x0c) = regs.x.ebx;
    }
    if ((*(unsigned int *)(p + 0x19) & 1) == 1) {
        regs.x.eax = 0x200;
        regs.h.bl = p[0x16];
        int386(0x31, &regs, &regs);
        *(unsigned short *)(p + 0x14) = regs.w.cx;
        *(unsigned short *)(p + 0x12) = regs.w.dx;
    }
    if (p[0x17] < 0x10)
        p[0x18] = inp(0x21);
    else if (p[0x17] < 0x18)
        p[0x18] = inp(0xa1);
    if (*(short *)(p + 4) != -1) {
        if (release)
            atexit((void (*)(void))release);
        *(short *)(p + 4) = -1;
    }
    *(short *)(p + 2) = -1;
}
