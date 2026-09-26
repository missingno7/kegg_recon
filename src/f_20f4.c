/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern short g_7b34;
extern short g_7b3d;
extern unsigned short g_7bfe;
extern unsigned short g_7c00;
extern int g_df6c;
extern unsigned char g_e48b;
extern unsigned char g_e48e;
extern void f_10137(void);
extern void f_13b72(void);
extern void f_9d40(unsigned char);
extern void f_ed38(void);
void f_20f4(void)
{
    g_7b3d = 0;
    *(unsigned char *)&g_7b34 &= 0xfc;
    *(unsigned char *)&g_7b34 |= 4;
    f_ed38();
    f_10137();
L_2121:;
    f_9d40(3);
    f_13b72();
    if (g_7b3d != 0) goto L_2158;
    if (g_7bfe == g_7c00) goto L_2156;
    if ((*(unsigned char *)&g_7bfe & 7) != 0) goto L_2158;
L_2156:;
    goto L_215a;
L_2158:;
    goto L_2172;
L_215a:;
    if (g_e48b == g_e48e) goto L_2170;
    if (g_e48e == 1) goto L_2172;
L_2170:;
    goto L_2174;
L_2172:;
    return;
L_2174:;
    --g_df6c;
    if (g_df6c != 0) goto L_2121;
}
