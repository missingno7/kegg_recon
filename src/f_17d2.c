/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern volatile unsigned int g_8dfc;
extern volatile unsigned int g_8e00;
extern volatile unsigned int g_8e04;
extern volatile unsigned int g_8e08;
extern volatile unsigned char g_e13c;
extern volatile unsigned char g_e140;
int f_17d2(volatile unsigned long a0)
{
    a0 = (a0 >> 4) | (a0 << 0xc);
    a0 ^= 0x7b69;
    g_8e04 = a0 & 0x3f;
    g_8e00 = (a0 >> 6) & 7;
    g_8dfc = (a0 >> 9) & 1;
    g_8e08 = ~(((a0 >> 0xa) & 0x3f) ^ (~(g_8e00 + 2) << 2)) & 0x3f;
    if (g_8e04 != g_8e08) goto L_189c;
    g_8e08 = ((g_8e00 ^ (((unsigned)g_8e04 >> 1) ^ ((unsigned)g_8e04 >> 3))) ^ (g_8e00 >> 1)) & 1;
    if (g_8dfc != g_8e08) goto L_189a;
    g_e13c = *(signed char *)&g_8e04;
    g_e140 = *(unsigned char *)&g_8e00;
L_189a:;
    goto L_18aa;
L_189c:;
    g_e13c = 4;
    g_e140 = 0;
L_18aa:;
    return g_e140;
}
