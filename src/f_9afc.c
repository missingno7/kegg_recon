/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <i86.h>
extern int g_73a8;
extern unsigned g_73ac;
extern unsigned char g_756f[];
extern int a_a03f(int);
extern void f_d7b8(void *);
void f_9afc(void)
{
    _disable();
    f_d7b8(g_756f);
    a_a03f(0);
    _enable();
    g_73ac = 0xffff;
    g_73a8 = g_73ac;
}
