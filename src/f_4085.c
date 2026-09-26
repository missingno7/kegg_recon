extern unsigned char *g_dee4;
extern unsigned char g_e142;
extern unsigned char *g_dda4;
extern unsigned char *g_e2ec;
extern int g_ddbc;
extern int g_ddc0;
extern int g_ddb8;
extern int g_df34;
extern int g_e24c;
extern int g_e250;
extern int g_e22c;
extern int g_e230;
extern int g_8db4;
extern int g_8db8;
extern int g_8e20;
extern int *g_dddc;
extern unsigned char g_a990;
extern void (*g_6160[])(void);

void f_8004(int *, int);
int f_b17e(void *, void *);
int f_b5df(void *, void *);
void f_4291(void);

void f_4085(void)
{
    int unused;

    g_e24c = *(int *)g_dee4;
    g_e250 = *(int *)(g_dee4 + 4);
    f_8004(&g_e24c, *(int *)(g_dee4 + 0x78));
    g_dda4 = &g_a990;
    for (g_ddbc = 0; g_ddbc < g_ddc0; ++g_ddbc) {
        *(int *)g_dda4 = *(int *)g_dda4;
        ++*(int *)(g_dda4 + 4);
        if (g_e142 != 0)
            ++*(int *)(g_dda4 + 4);
        g_8db4 = f_b17e(g_dda4 + 8, g_dda4 + 0xc) + g_df34;
        if (*(int *)g_dda4 > 0x140 || *(int *)g_dda4 < 0 ||
            *(int *)(g_dda4 + 4) > 0xd8 || *(int *)(g_dda4 + 4) < 0) {
            f_4291();
            continue;
        }
        if (*(int *)(g_dda4 + 4) < 0xc3) {
            g_e22c = *(int *)g_dda4;
            g_e230 = *(int *)(g_dda4 + 4);
            f_8004(&g_e22c, g_8db4);
            if (f_b5df(&g_e24c, &g_e22c) != 0) {
                g_ddb8 = ((unsigned char *)g_dda4)[0x10] + 1;
                g_8e20 = ((unsigned char *)g_dda4)[0x11];
                if (g_8e20 >= 0x1c)
                    g_8e20 -= 0x1c;
                g_8db8 = (int)g_6160[g_8e20];
                ((void (*)(void))g_8db8)();
                g_dddc[5] += 2 << *(int *)(g_dee4 + 0x24);
                f_4291();
                continue;
            }
        }
        *(int *)g_e2ec = g_8db4;
        *(short *)(g_e2ec + 4) = *(short *)g_dda4;
        *(short *)(g_e2ec + 6) = *(short *)(g_dda4 + 4);
        *(short *)(g_e2ec + 8) = 0;
        g_e2ec += 0xa;
        g_dda4 += 0x12;
    }
}
