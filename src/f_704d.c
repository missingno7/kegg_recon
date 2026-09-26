typedef unsigned int size_t;
extern int g_9480, g_9488, g_9490, g_94a0, g_94b0;
extern int g_8e20;
extern int g_dd84, g_dd88, g_dda8, g_ddac;
extern char *g_9478, *g_dd6c, *g_ddb4;
extern char g_df78[];
extern void f_7551(void), f_7bf4(void), f_85a4(void);
extern void f_100fa(int, int, int, int), f_10502(int, int), f_10137(void);
void *memmove(void *, const void *, size_t);

void f_704d(void) { g_dd84 = 0; }

void f_7112(void)
{
    f_100fa(4, 16, 4, 0x95);
    f_10502(4, 0x52);
    for (g_8e20 = 0; g_8e20 < 4; ++g_8e20)
        f_10137();
    g_94a0 = 100;
    g_9490 = g_94a0;
    g_94b0 = g_9490;
}

void f_7532(void) { if (g_9488) f_7551(); }

void f_788d(void)
{
    --g_9488;
    if (g_9480 != g_9488)
        memmove(g_9478, g_9478 + 0x24, (g_9488 - g_9480) * 0x24);
    --g_9480;
}

void f_7bd5(void) { if (g_dd84) f_7bf4(); }

void f_7e12(void)
{
    --g_dd84;
    if (g_dd88 != g_dd84)
        memmove(g_dd6c, g_dd6c + 0x20, (g_dd84 - g_dd88) * 0x20);
    --g_dd88;
}

void f_8585(void) { if (g_dda8) f_85a4(); }

void f_8f7e(void)
{
    --g_dda8;
    if (g_ddac != g_dda8)
        memmove(g_ddb4, g_ddb4 + 0x12, (g_dda8 - g_ddac) * 0x12);
    --g_ddac;
}

void f_8fce(int x, int y)
{
    if (g_dda8 < 0x19) {
        g_ddb4 = g_df78 + g_dda8 * 0x12;
        *(int *)(g_ddb4 + 0) = x << 4;
        *(int *)(g_ddb4 + 4) = y << 4;
        *(int *)(g_ddb4 + 8) = 10;
        *(int *)(g_ddb4 + 12) = -20;
        g_ddb4[17] &= (char)~1;
        g_ddb4[17] &= (char)~2;
        g_ddb4[16] = 0;
        ++g_dda8;
    }
}
