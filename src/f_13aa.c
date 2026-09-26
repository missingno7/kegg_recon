extern int g_df48, g_df4c, g_df54;
extern unsigned char * g_dd40;
extern void f_13a48(void *, int, int, int);
void f_13aa(void) {
    f_13a48(g_dd40 + g_df48 * 3, 0xc0, 0x20, 0);
    --g_df4c;
    if (g_df4c == 0) {
        g_df4c = 2;
        if (g_df54 - 0x20 <= ++g_df48)
            g_df48 = 0;
    }
}
