/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern unsigned g_73ac;
extern unsigned char *g_73b4;
extern short g_73cc;
extern unsigned char g_e168[];
extern unsigned char g_e16c[];
extern unsigned char g_e170[];
extern short g_e1be;
void f_9e54(void)
{
    unsigned short v_4;
    if (*(short *)g_73b4 != 0) goto L_9e72;
    ++g_e1be;
L_9e72:;
    v_4 = 0;
L_9e79:;
    if ((unsigned short)v_4 < (unsigned short)g_73cc) goto L_9e8f;
    return;
L_9e87:;
    v_4++;
    goto L_9e79;
L_9e8f:;
    *(int *)(g_e170 + (v_4 * 0xc)) += g_73ac;
    if ((unsigned)*(int *)(g_e170 + (v_4 * 0xc)) < *(int *)(g_e16c + (v_4 * 0xc))) goto L_9ee5;
    *(int *)(g_e170 + (v_4 * 0xc)) -= *(int *)(g_e16c + (v_4 * 0xc));
    (*(int (**)())(g_e168 + (v_4 * 0xc)))();
L_9ee5:;
    goto L_9e87;
}
