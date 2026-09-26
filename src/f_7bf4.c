struct Frame { unsigned img; int time; };
struct Spr {
    int a, b, c, d;
    int e;
    int speed;
    struct Frame *f;
    unsigned b0 : 1, anim : 4, f5 : 1, f6 : 1, f7 : 1, f8 : 1;
};
extern int g_dd84, g_dd88, g_8e20, g_8db4;
extern struct Spr *g_dd6c;
extern struct Spr g_94b8[];
extern unsigned char *g_de5c, *g_dee4;
extern int g_e24c, g_e250;
extern unsigned char *g_e2ec;
extern void f_7e12(void);
extern void f_8004(int *, int);

void f_7bf4(void)
{
    g_dd6c = g_94b8;
    for (g_dd88 = 0; g_dd88 < g_dd84; g_dd88++) {
        if (g_dd6c->f5) {
            g_dd6c->a = *(int *)g_dee4;
            g_dd6c->b = *(int *)(g_dee4 + 4);
            goto L_7ca4;
        }
        if (g_dd6c->anim == 0) {
            g_dd6c->a += g_dd6c->c;
            g_dd6c->b += g_dd6c->d;
            goto L_7ca4;
        }
        g_dd6c->anim--;
L_7ca4:
        if (--g_dd6c->speed > 0) goto L_7d2a;
        g_8e20 = (++g_dd6c->f)->time;
        if (g_8e20 >= 0) goto L_7d1c;
        if (g_dd6c->f6) *g_de5c |= 0x40;
        if (g_dd6c->f8) {
            f_7e12();
            continue;
        }
        g_dd6c->f += g_8e20;
        g_8e20 = g_dd6c->f->time;
L_7d1c:
        g_dd6c->speed = g_8e20;
L_7d2a:
        g_8db4 = g_dd6c->e + g_dd6c->f->img;
        if (g_dd6c->f7) {
            g_e24c = g_dd6c->a;
            g_e250 = g_dd6c->b;
            f_8004(&g_e24c, g_8db4);
            if (g_dd6c->a > 0x150) goto L_7d96;
            if (g_dd6c->a >= -0x10) goto L_7d98;
L_7d96:
            goto L_7da6;
L_7d98:
            if (g_dd6c->b <= 0xd8) goto L_7da8;
L_7da6:
            goto L_7db3;
L_7da8:
            if (g_dd6c->b >= -0x10) goto L_7dbd;
L_7db3:
            f_7e12();
            continue;
        }
L_7dbd:
        *(int *)g_e2ec = g_8db4;
        *(short *)(g_e2ec + 4) = g_dd6c->a;
        *(short *)(g_e2ec + 6) = g_dd6c->b;
        *(short *)(g_e2ec + 8) = 0;
        g_e2ec += 10;
        g_dd6c++;
    }
}
