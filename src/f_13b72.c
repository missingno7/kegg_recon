/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern unsigned char g_e48d;
extern unsigned char g_e48f;
extern void f_c294(void);
void f_13b72(void)
{
    if (g_e48d == 0x53) goto L_13b90;
    if (g_e48f == 0x53) goto L_13b92;
L_13b90:;
    return;
L_13b92:;
    f_c294();
}
