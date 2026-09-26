/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern int g_3902;
extern unsigned int g_8dfc;
extern unsigned int g_8e00;
extern unsigned int g_8e04;
extern unsigned int g_8e08;
extern int *g_dddc;
extern unsigned char g_e13c;
extern unsigned char g_e140;
extern unsigned char g_e143;
char *itoa(int, char *, int);
void f_18bc(void)
{
    g_8e04 = g_dddc[1] & 0x3f;
    g_8e00 = (g_e143 / 0xa) & 7;
    g_8dfc = ((g_8e00 ^ (((unsigned)g_8e04 >> 1) ^ ((unsigned)g_8e04 >> 3))) ^ (g_8e00 >> 1)) & 1;
    g_8e08 = ~((~(g_8e00 + 2) << 2) ^ g_8e04) & 0x3f;
    g_8e08 = ((((g_8e08 << 0xa) | (g_8dfc << 9)) | (g_8e00 << 6)) | g_8e04) ^ 0x7b69;
    g_8e08 = (g_8e08 << 4) | (g_8e08 >> 0xc);
    g_e13c = *(signed char *)&g_8e04;
    g_e140 = *(unsigned char *)&g_8e00;
    itoa(g_8e08 & 0xffff, (char *)(g_3902 + 0x32), 0x10);
}
