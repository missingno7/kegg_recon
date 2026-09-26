/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern short g_746c;
extern short g_7b34;
extern short g_7b3d;
extern unsigned char g_ab40[];
extern unsigned char g_bf40[];
extern unsigned char g_d340[];
extern int g_df5c;
extern void f_111de(unsigned, unsigned);
extern void f_1202(void);
extern void f_1227(void);
extern void f_127e(void);
extern void f_13aa(void);
extern void f_13b72(void);
extern void f_1416(void);
extern void f_80d8(void);
extern void f_9d40(unsigned char);
extern void f_c8c0(int, int, short, int, int);
extern void f_c9ce(int, int);
extern void f_ed38(void);
void f_113c(void)
{
    g_7b34 = (((g_7b34 & 0xfffe) & 0xfffd) & 0xfffb) & 0xff7f;
    g_df5c = 0;
    g_7b3d = 0;
    f_c8c0((int)g_d340, 0x100, 4, (int)g_ab40, (int)g_bf40);
L_1194:;
    if (g_df5c != 0) goto L_11fd;
    f_9d40(3);
    f_80d8();
    f_13b72();
    f_1202();
    f_1227();
    f_127e();
    f_13aa();
    f_1416();
    f_c9ce(0, 0);
    if (g_746c == 0) goto L_11f6;
    f_111de(g_746c, 0);
L_11f6:;
    f_ed38();
    goto L_1194;
L_11fd:;
}
