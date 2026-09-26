/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern int g_7b0c;
extern int g_8e1c;
extern int g_8e20;
extern int g_dd40;
extern void f_9d40(unsigned char);
extern void f_c20d(int);
extern void f_ec76(void *);
int f_6e2e(void)
{
    f_c20d(0xb);
    g_7b0c = 2;
    g_8e1c = 0;
L_6e58:;
    if (g_8e1c < 0x3f) goto L_6e6e;
    goto L_6ecf;
L_6e66:;
    ++g_8e1c;
    goto L_6e58;
L_6e6e:;
    g_8e20 = 0;
L_6e78:;
    if (g_8e20 < 0x300) goto L_6e8f;
    goto L_6eb1;
L_6e86:;
    g_8e20 += 3;
    goto L_6e78;
L_6e8f:;
    if (++*(unsigned char *)(unsigned char *)(g_dd40 + g_8e20) <= 0x3f) goto L_6eaf;
    *(unsigned char *)(unsigned char *)(g_dd40 + g_8e20) = 0x3f;
L_6eaf:;
    goto L_6e86;
L_6eb1:;
    f_ec76((void *)g_dd40);
    f_9d40(3);
    goto L_6e66;
L_6ecf:;
    g_7b0c = 1;
    return 0;
}
