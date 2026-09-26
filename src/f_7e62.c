extern int g_dd84;
struct Spr {
    int a, b, c, d, e, speed, f;
    unsigned b0 : 1, anim : 4, f5 : 1, f6 : 1, f7 : 1, f8 : 1;
};
extern struct Spr *g_dd6c;
extern struct Spr g_94b8[];

void f_7e62(int a, int b, int c, int d, int e, int f, int flags)
{
    if (g_dd84 < 0x32) {
        g_dd6c = g_94b8;
        g_dd6c += g_dd84;
        g_dd6c->a = a;
        g_dd6c->b = b;
        g_dd6c->c = c;
        g_dd6c->d = d;
        if (flags & 0x10) {
            g_dd6c->speed = 0x3e80;
            g_dd6c->f = f;
        } else {
            g_dd6c->speed = 0;
            g_dd6c->f = f - 8;
        }
        g_dd6c->e = e;
        g_dd6c->anim = flags >> 4;
        g_dd6c->f5 = flags >> 3;
        g_dd6c->f6 = flags >> 2;
        g_dd6c->f7 = flags >> 1;
        g_dd6c->f8 = flags;
        g_dd84++;
    }
}
