extern unsigned char g_e13e;
extern unsigned char g_e13b, g_e46b;
extern unsigned char *g_dd40;
extern void *g_dddc;
extern int g_8e1c, g_8e20, g_7b0c;
extern int g_94a0, g_9490;
extern int g_94a4, g_94a8, g_9494;
extern int g_94ac, g_946c, g_948c, g_949c;
extern int g_94b4, g_9498, g_9474;
extern int g_e36a, g_e366;
extern int g_6884[], g_6888[], g_688c[], g_6890[];
extern short g_7b20, g_7b22, g_7b24, g_7b26;
extern short g_7b14, g_7b16, g_7b18, g_7b1a;
extern unsigned short g_7bfe, g_7c00;
extern short g_e4a8, g_e4c2;
extern void f_c20d(int), f_ec76(void *), f_9d40(int);
extern void f_ea9f(void *, int, int, int);
extern void f_10369(void), f_103b9(void);
extern void f_c8c0();

int f_6e2e(void)
{
    f_c20d(11);
    g_7b0c = 2;
    for (g_8e1c = 0; g_8e1c < 0x3f; ++g_8e1c) {
        for (g_8e20 = 0; g_8e20 < 0x300; g_8e20 += 3) {
            ++g_dd40[g_8e20];
            if (g_dd40[g_8e20] > 0x3f)
                g_dd40[g_8e20] = 0x3f;
        }
        f_ec76(g_dd40);
        f_9d40(3);
    }
    g_7b0c = 1;
    return 0;
}

int f_6ee8(void)
{
    f_c20d(0x26);
    if (g_e13b == 0)
        *(int *)((unsigned char *)g_dddc + 20) += (5000 / g_94a0) * g_9490 + 5000;
    g_7b0c = 3;
    f_ea9f(g_dd40, 0, -63, -1);
    g_7b0c = 1;
    return 1;
}

void f_7068(void)
{
    g_94a4 = g_6884[4 * g_e13e];
    g_94a8 = g_94a4;
    g_9494 = g_94a8;
    g_94ac = g_6888[4 * g_e13e];
    g_946c = g_688c[4 * g_e13e];
    g_948c = g_946c;
    g_949c = g_6890[4 * g_e13e];
    g_94b4 = 0;
    g_9498 = 7;
    g_9474 = 15;
}

void f_80d8(void)
{
    if (g_e46b & 0x20) {
        if (g_7bfe != g_7c00) {
            if (g_7bfe == 1) {
                f_10369();
                g_e4a8 -= 2;
                g_e4c2 -= 2;
                f_103b9();
            } else if (g_7bfe == 2) {
                f_10369();
                g_e4a8 += 2;
                g_e4c2 += 2;
                f_103b9();
            }
        }
    }
}

void f_843a(void)
{
    extern unsigned char g_d340[], g_ab40[], g_bf40[];
    f_c8c0(g_d340, 0x100, 4, g_ab40, g_bf40);
    g_7b14 = g_7b20;
    g_7b16 = g_7b22;
    g_7b18 = g_7b24;
    g_7b1a = g_7b26;
}
