extern int g_9490, g_68f4;
extern char *g_dd4c;
extern unsigned short g_7bfe, g_7c00;
extern short g_e4ba, g_e4c4, g_e4c6;
extern unsigned char *g_e2ec;
extern unsigned char g_6864[];
extern void f_c20d(int);
extern void f_78dd(int, int, int, int, int, int, int);

void f_73c0(void)
{
    if (g_9490 <= 0) goto bottom;
    if (g_68f4 != 0) goto reset_gate;
    if (g_7bfe == 3) goto reset_gate;
    goto first_compare;
reset_gate:
    goto reset_flag;
first_compare:
    if (g_7bfe == g_7c00) goto second_gate;
    if ((g_7bfe & 1) == 1) goto first_side;
second_gate:
    goto second_side;
first_side:
    f_c20d(0x66);
    f_78dd(0, 4, g_e4c6, g_e4c4, 0x40, 0, (int)g_6864);
    g_e4ba -= 7;
    g_68f4 = 1;
    goto second_exit;
second_side:
    if (g_7bfe == g_7c00) goto second_skip;
    if (g_7bfe & 2) goto second_actions;
second_skip:
    goto second_exit;
second_actions:
    f_c20d(0x66);
    f_78dd(0, 1, g_e4c6, g_e4c4, 0x60, 0x18, (int)g_6864);
    f_78dd(0, 1, g_e4c6, g_e4c4, 0x60, -0x18, (int)g_6864);
    g_e4ba -= 7;
    g_68f4 = 1;
second_exit:
    goto bottom;
reset_flag:
    g_68f4 = 0;
bottom:
    *(int *)g_e2ec = (int)(g_dd4c + 0x35d6);
    *(short *)(g_e2ec + 4) = g_e4c6;
    *(short *)(g_e2ec + 6) = g_e4c4;
    *(short *)(g_e2ec + 8) = 0;
    g_e2ec += 10;
}
