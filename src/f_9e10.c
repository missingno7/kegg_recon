/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <conio.h>
extern int g_e1a8;
void f_9e10(void)
{
L_9e1c:;
    if (((unsigned char)inp(0x3da) & 8) == 0) goto L_9e35;
    ++g_e1a8;
    goto L_9e1c;
L_9e35:;
    if (((unsigned char)inp(0x3da) & 8) != 0) goto L_9e4e;
    ++g_e1a8;
    goto L_9e35;
L_9e4e:;
}
