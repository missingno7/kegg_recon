struct Grad { short x, y; unsigned char r0, g0, b0, r1, g1, b1; };
extern void f_ecdf(unsigned char, unsigned char, unsigned char, unsigned char);

void f_9640(struct Grad *c, unsigned char *p)
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    int dr;
    int dg;
    int db;
    int idx;
    int step;
    int i;
    int n;
    while (c->x != -1) {
        if (c->x < 0) break;
        if (c->x > 255) break;
        if (c->y < 0) break;
        if (c->y > 255) break;
        if (p == 0)
            f_ecdf(c->x, c->r0, c->g0, c->b0);
        else {
            *p++ = c->r0;
            *p++ = c->g0;
            *p++ = c->b0;
        }
        dr = c->r1 - c->r0;
        dg = c->g1 - c->g0;
        db = c->b1 - c->b0;
        n = c->y - c->x;
        step = 1;
        if (n < 0) {
            n = -n;
            step = -1;
        }
        n++;
        idx = c->x;
        for (i = 1; i < n; i++) {
            r = dr * i / n + c->r0;
            g = dg * i / n + c->g0;
            b = db * i / n + c->b0;
            if (p == 0) {
                idx += step;
                f_ecdf(idx, r, g, b);
            } else {
                *p++ = r;
                *p++ = g;
                *p++ = b;
            }
        }
        c++;
    }
}
