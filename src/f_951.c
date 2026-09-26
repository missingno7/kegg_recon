/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern short g_742c;
extern short g_747c;
extern short g_7b34;
extern short g_7b3d;
extern unsigned short g_7bfe;
extern unsigned short g_7c00;
extern unsigned char *g_dd40;
extern unsigned char g_e48b;
extern unsigned char g_e48e;
extern void f_13b72(void);
extern void f_9d40(unsigned char);
extern void f_ea9f(void *, int, int, int);
void f_951(void)
{
    g_7b34 = (g_7b34 & 0xfffe) & 0xfffd;
    g_7b3d = 0;
L_976:;
    f_9d40(3);
    f_13b72();
    if (g_7b3d != 0) goto L_9ae;
    if (g_7bfe == g_7c00) goto L_9ac;
    if (g_7bfe != 0) goto L_9ae;
L_9ac:;
    goto L_9b0;
L_9ae:;
    goto L_9c8;
L_9b0:;
    if (g_e48b == g_e48e) goto L_9c6;
    if (g_e48e == 1) goto L_9c8;
L_9c6:;
    goto L_9ca;
L_9c8:;
    goto L_9e0;
L_9ca:;
    if (g_747c == 0) goto L_976;
    if (g_742c == -1) goto L_976;
L_9e0:;
    f_ea9f(g_dd40, 0, 0x3f, 3);
}
