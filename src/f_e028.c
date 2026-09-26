#include <i86.h>
extern short g_756f, g_75ca;
extern unsigned char g_e381, g_e382;
extern void f_9afc(void);
extern void f_9b44(int);
void f_e028(void) {
    int old_756f = g_756f;
    union REGS regs;
    if (g_75ca == -1) {
        f_9afc();
        regs.h.ah = 0;
        g_e381 = g_e382;
        regs.h.al = g_e381;
        int386(0x10, &regs, &regs);
        g_75ca = 1;
        if (old_756f == -1)
            f_9b44(0);
    }
}
