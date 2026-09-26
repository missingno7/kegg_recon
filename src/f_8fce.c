extern int g_dda8;
extern unsigned char * g_ddb4;
extern unsigned char g_df78[];

void f_8fce(int x, int y)
{
    if (g_dda8 < 0x19) {
        g_ddb4 = g_df78;
        g_ddb4 += g_dda8 * 0x12;
        *(int *)(g_ddb4 + 0) = x << 4;
        *(int *)(g_ddb4 + 4) = y << 4;
        *(int *)(g_ddb4 + 8) = 10;
        *(int *)(g_ddb4 + 12) = -20;
        g_ddb4[17] &= (unsigned char)~1;
        g_ddb4[17] &= (unsigned char)~2;
        g_ddb4[16] = 0;
        ++g_dda8;
    }
}
