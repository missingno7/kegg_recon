extern unsigned char g_8e1c;
extern int g_8e20, g_ddb8;
extern unsigned char * volatile g_ddb4;

void f_9053(void)
{
    if (*(unsigned char *)&g_8e1c & 1) {
        g_8e20 = *(int *)(g_ddb4 + 8);
        if (g_8e20 > 0) {
            g_8e20 += g_ddb8 * 4;
            if (g_8e20 > 0x20) g_8e20 = 0x20;
            if (g_8e20 < 4) g_8e20 = 4;
        } else if (g_8e20 < 0) {
            g_8e20 -= g_ddb8 * 4;
            if (g_8e20 < -0x20) g_8e20 = -0x20;
            if (g_8e20 > -4) g_8e20 = -4;
        }
        *(int *)(g_ddb4 + 8) = g_8e20;
    }
    if (*(unsigned char *)&g_8e1c & 2) {
        g_8e20 = *(int *)(g_ddb4 + 12);
        if (g_8e20 > 0) {
            g_8e20 += g_ddb8 * 4;
            if (g_8e20 > 0x28) g_8e20 = 0x28;
            if (g_8e20 < 4) g_8e20 = 4;
        } else {
            g_8e20 -= g_ddb8 * 4;
            if (g_8e20 < -0x28) g_8e20 = -0x28;
            if (g_8e20 > -4) g_8e20 = -4;
        }
        *(int *)(g_ddb4 + 12) = g_8e20;
    }
}
