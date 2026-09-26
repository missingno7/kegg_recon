extern int g_9488, g_dd4c;
extern unsigned char * volatile g_9478;
extern unsigned char g_8e4c[];

void f_78dd(int a, int b, int x, int y, int c, int d, int e)
{
    if (g_9488 < 0x18) {
        g_9478 = g_8e4c;
        g_9478 += g_9488 * 0x24;
        *(int *)(g_9478 + 0) = a;
        *(int *)(g_9478 + 0x20) = b;
        *(int *)(g_9478 + 4) = x << 4;
        *(int *)(g_9478 + 8) = y << 4;
        *(int *)(g_9478 + 12) = c;
        *(int *)(g_9478 + 16) = d;
        *(int *)(g_9478 + 24) = 0;
        *(int *)(g_9478 + 28) = e - 8;
        *(int *)(g_9478 + 20) = g_dd4c;
        ++g_9488;
    }
}
