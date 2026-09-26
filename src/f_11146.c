/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <conio.h>
#include <i86.h>
#include <stdio.h>
extern unsigned char g_2428[];
extern unsigned char g_242c[];
extern unsigned char g_7d84[];
extern unsigned g_e4dc;
extern unsigned g_e4e0;
void f_11146(void)
{
    outp(0x21, inp(0x21) & -2);
    if (g_e4dc == 0) goto L_111d4;
    printf((char *)*(int *)((*(unsigned char * *)(g_7d84 + (((int)(g_e4dc & 0xff00) >> 8) << 2))) + (((g_e4dc & 0xff) << 2) - 4)));
    if (g_e4e0 == 0) goto L_111c1;
    printf((char *)g_2428, g_e4e0);
L_111c1:;
    printf((char *)g_242c);
    getch();
L_111d4:;
    _enable();
}
