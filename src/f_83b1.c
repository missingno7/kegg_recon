extern unsigned char *g_e158;
extern short g_e15c, g_e15e, g_e160, g_e162;

struct Iter24 {
    int first, second, start, count, step, unused;
};
struct Iter48 {
    int pad0, pad4, box, pad12, pad16; char *at20;
    int pad24, pad28, pad32, pad36, pad40, pad44;
};
extern struct Iter24 *g_e150;
extern int g_e148, g_e14c, g_e154;
extern unsigned char *g_e2ec;

void f_95ee(void)
{
    g_e160 = *(short *)(g_e158 + 2);
    g_e162 = *(short *)(g_e158 + 4);
    g_e15c = *(short *)(g_e158 + 10);
    g_e15e = *(short *)(g_e158 + 12);
}

void f_8004(int *rect, unsigned char *info)
{
    g_e158 = info;
    f_95ee();
    rect[0] += g_e15c;
    rect[1] += g_e15e;
    rect[2] = rect[0] + g_e160 - 2;
    rect[3] = rect[1] + g_e162 - 2;
}

void f_8066(int *rect, unsigned char *info)
{
    g_e158 = info;
    f_95ee();
    rect[0] += g_e15c + 3;
    rect[1] += g_e15e + 3;
    rect[2] = rect[0] - 3 + g_e160 - 5;
    rect[3] = rect[1] - 3 + g_e162 - 5;
}

void f_82cc(void)
{
    struct Iter24 *p = g_e150;
    int i = 0;
    for (; i < g_e148; i++) {
        p->count = (p->second - p->first) / p->step;
        p->start = p->first - p->count;
        p++;
    }
    g_e14c = 0;
}

void f_8345(void)
{
    struct Iter24 *p;
    int i;
    ++g_e14c;
    p = g_e150;
    i = 0;
    for (; i < g_e148; i++) {
        if (g_e14c >= p->step)
            p->start = p->second;
        else
            p->start += p->count;
        p++;
    }
}

void f_83b1(void)
{
    struct Iter48 *p = (struct Iter48 *)g_e150;
    int i = 0;
    for (; i < (g_e148 >> 1); i++) {
        *(int *)g_e2ec = (int)(p->at20 + g_e154);
        *(short *)(g_e2ec + 4) = (short)(p->box >> 4);
        *(short *)(g_e2ec + 6) = (short)(p->pad32 >> 4);
        *(short *)(g_e2ec + 8) = 0;
        g_e2ec += 10;
        ++p;
    }
}
