#include <conio.h>
extern void f_13a88(void), f_f8bb(void), f_fce4(void);
extern unsigned char g_7b34;
void __interrupt f_ff5c(void) {
    f_13a88();
    f_f8bb();
    outp(0x20, 0x20);
    if (g_7b34 & 8)
        f_fce4();
}
