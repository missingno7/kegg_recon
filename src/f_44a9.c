extern unsigned char *g_de5c;
extern int *g_dee4;
extern int g_ddb8;
extern short g_e4c4;
extern short g_e4c6;
void f_c20d(int);
void f_10502(int, int);
void f_44a9(void)
{
    f_c20d(0x5b);
    if ((*g_de5c & 8) == 0) {
        if ((*g_de5c & 0x10) == 0) {
            *g_de5c |= 0x10;
            g_dee4[0] = g_dee4[4] + g_dee4[3] - g_e4c6;
            g_dee4[1] = g_e4c4;
            f_10502(g_dee4[0], g_dee4[1]);
            g_dee4[14] = g_ddb8 << 6;
        } else {
            g_dee4[14] += g_ddb8 << 6;
        }
    }
}
