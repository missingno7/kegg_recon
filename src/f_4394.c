extern int *g_dee4;
extern int g_ddb8;
extern int g_8e20;
extern unsigned char *g_de5c;
void f_c20d(int);
void f_4394(void)
{
    if (g_ddb8 > 0)
        f_c20d(0x29);
    *g_de5c |= 0x20;
    g_dee4[22] = 3;
    g_8e20 = g_dee4[7] + g_ddb8;
    if (g_8e20 > 12) g_8e20 = 12;
    if (g_8e20 < 0) g_8e20 = 0;
    g_dee4[23] = g_8e20;
}
