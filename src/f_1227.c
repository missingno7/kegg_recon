extern int *g_df70;
extern unsigned char *g_dd4c;
extern unsigned char *g_e2ec;
extern short g_e4c4, g_e4c6;
void f_1227(void) {
    *(int *)g_e2ec = (int)(g_dd4c + *(int *)g_df70);
    *(short *)(g_e2ec + 4) = g_e4c6;
    *(short *)(g_e2ec + 6) = g_e4c4;
    *(short *)(g_e2ec + 8) = 0;
    g_e2ec += 10;
}
