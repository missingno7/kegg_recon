/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern short g_73cc;
extern unsigned char g_e168[];
extern unsigned char g_e16c[];
extern unsigned char g_e170[];
void f_9eec(int a0, int a1)
{
    if ((unsigned short)g_73cc >= 5) goto L_9f43;
    *(int *)(g_e168 + ((unsigned short)g_73cc * 0xc)) = a0;
    *(int *)(g_e16c + ((unsigned short)g_73cc * 0xc)) = a1;
    *(int *)(g_e170 + ((unsigned short)g_73cc * 0xc)) = 0;
    ++g_73cc;
L_9f43:;
}
