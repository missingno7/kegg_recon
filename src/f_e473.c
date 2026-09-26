/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <conio.h>
#include <i86.h>
extern short g_e324;
extern void f_139c4(short);
extern void f_13a0a(short);
extern void f_13a29(short);
void f_e473(int a0)
{
    if (a0 != 1) goto L_e582;
    _disable();
    outp(0x3ce, 5);
    outp(0x3cf, inp(0x3cf) & 0xef);
    outp(0x3ce, 6);
    outp(0x3cf, inp(0x3cf) & 0xfd);
    outp(0x3c4, 4);
    outp(0x3c5, (inp(0x3c5) & 0xf7) | 4);
    outp(0x3d4, 0x14);
    outp(0x3d5, inp(0x3d5) & 0xbf);
    outp(0x3d4, 0x17);
    outp(0x3d5, inp(0x3d5) | 0x40);
    _enable();
    goto L_e67e;
L_e582:;
    if (a0 != 0) goto L_e67e;
    _disable();
    outp(0x3ce, 5);
    outp(0x3cf, inp(0x3cf) | -0xf0);
    outp(0x3ce, 6);
    outp(0x3cf, inp(0x3cf) | -0xfe);
    outp(0x3c4, 4);
    outp(0x3c5, inp(0x3c5) | -0xf8);
    outp(0x3d4, 0x14);
    outp(0x3d5, inp(0x3d5) | -0xc0);
    outp(0x3d4, 0x17);
    outp(0x3d5, inp(0x3d5) & -0x41);
    _enable();
L_e67e:;
    f_13a0a(0);
    f_139c4(0xf);
    f_13a29(0x40);
    g_e324 = (unsigned short)a0;
}
