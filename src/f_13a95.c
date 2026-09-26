/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <stdlib.h>
extern short g_742e;
extern int g_7c08;
extern unsigned g_7c0c;
extern int (* g_7c10)();
extern unsigned char g_ab40[];
extern unsigned char g_bf40[];
extern unsigned char g_d340[];
extern unsigned char *g_e4d0;
extern int a_982c(void);
extern int f_108a9(int, int, unsigned);
extern void f_10d72(int, int, int, int);
extern int f_11051(unsigned);
extern void f_111de(unsigned, unsigned);
extern void f_708(void);
extern void f_c8c0(int, int, short, int, int);
extern int f_ddb9(int);
void f_13a95(void)
{
    if (f_108a9(0x1f40, 0x55730, -1) == 0) goto L_13ac5;
    f_111de(0, 0);
L_13ac5:;
    if (f_11051(7) == 0) goto L_13adf;
    f_111de(0, 0);
L_13adf:;
    f_10d72(-1, -1, 0, -1);
    g_7c08 = f_ddb9(0x55730);
    g_e4d0 = (unsigned char *)g_7c08;
    g_7c0c = g_7c08 + 0x55730;
    g_7c10 = (int)a_982c;
    g_742e = -1;
    f_c8c0((int)g_d340, 0x100, 4, (int)g_ab40, (int)g_bf40);
    f_708();
    f_111de(0, 0);
    exit(0);
}
