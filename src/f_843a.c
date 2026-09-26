/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern short g_7b14;
extern short g_7b16;
extern short g_7b18;
extern short g_7b1a;
extern short g_7b20;
extern short g_7b22;
extern short g_7b24;
extern short g_7b26;
extern unsigned char g_ab40[];
extern unsigned char g_bf40[];
extern unsigned char g_d340[];
extern void f_c8c0(int, int, short, int, int);
void f_843a(void)
{
    f_c8c0((int)g_d340, 0x100, 4, (int)g_ab40, (int)g_bf40);
    g_7b14 = g_7b20;
    g_7b16 = g_7b22;
    g_7b18 = g_7b24;
    g_7b1a = g_7b26;
}
