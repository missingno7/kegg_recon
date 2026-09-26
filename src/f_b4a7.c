/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern int strcpy(int, int);
extern int ltoa(int, int, int);
extern int strlen(int);
extern void f_b1df(int, int, int);
void f_b4a7(int a0, int a1, int a2, int a3, int a4)
{
    int v_4;
    int v_8;
    unsigned char v_28[32];
    ltoa(a2, (int)((v_28 + a4) + 1), a3);
    v_8 = a4 - strlen((int)((v_28 + a4) + 1));
    if (v_8 >= 0) goto L_b4f0;
    v_8 = 0;
L_b4f0:;
    strcpy((int)(v_28 + v_8), (int)((v_28 + a4) + 1));
    v_4 = (int)v_28;
L_b50d:;
    if (v_8 > 0) goto L_b51d;
    goto L_b528;
L_b515:;
    v_8--;
    goto L_b50d;
L_b51d:;
    *(unsigned char *)(unsigned char *)(v_4++) = 0x30;
    goto L_b515;
L_b528:;
    f_b1df(a0, a1, (int)v_28);
}
