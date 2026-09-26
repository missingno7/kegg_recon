extern int g_dd74;
extern int g_dd7c;
extern int g_9af8;
extern int g_68b7[];

void f_3e0f(int a, int b)
{
    if (g_dd74 < 0x120) {
        g_dd7c = (int)&g_9af8;
        g_dd7c += g_dd74 * 9;
        *(int *)(g_dd7c + 4) = g_68b7[a];
        *(int *)g_dd7c = b;
        *(unsigned char *)(g_dd7c + 8) = (unsigned char)a;
        ++g_dd74;
    }
}
