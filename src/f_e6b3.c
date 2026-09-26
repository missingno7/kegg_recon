#include <conio.h>
extern void f_e813(void);
extern void f_13f9f(void);
extern void f_13fa1(void);
void f_e6b3(unsigned char *p) {
    volatile int i;
    f_e813();
    f_13f9f();
    if (p[0] & 1) {
        for (i = 0; i <= 0x18; i++)
            outpw(0x3d4, ((unsigned int)p[i + 4] << 8) | i);
    }
    if (p[0] & 2) {
        for (i = 0; i <= 4; i++)
            outpw(0x3c4, ((unsigned int)p[i + 0x1d] << 8) | i);
    }
    if (p[0] & 4) {
        for (i = 0x10; i <= 0x14; i++) {
            outp(0x3c0, i | 0x20);
            outp(0x3c0, p[i + 0x22]);
        }
    }
    if (p[0] & 8) {
        for (i = 0; i <= 8; i++)
            outpw(0x3c4, ((unsigned int)p[i + 0x27] << 8) | i);
    }
    if (p[0] & 0x10) {
        outp(0x3c2, p[0x30]);
        outp(0x3da, p[0x31]);
    }
    f_13fa1();
    f_e813();
}
