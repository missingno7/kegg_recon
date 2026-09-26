extern void f_b03c(short, short, short, short, short, short);
extern unsigned char f_13324(short, short);
extern void f_133f6(int, int, int);
extern int g_7404;

void f_ae56(short x1, short y1, short x2, short y2)
{
    short midx, midy;
    if (x2 - x1 >= 2)
        goto subdivide;
    if (y2 - y1 < 2)
        return;
subdivide:
    midx = (x1 + x2) >> 1;
    midy = (y1 + y2) >> 1;
    f_b03c(x1, y1, x2, y1, midx, y1);
    f_b03c(x2, y1, x2, y2, x2, midy);
    f_b03c(x1, y2, x2, y2, midx, y2);
    f_b03c(x1, y1, x1, y2, x1, midy);
    if (f_13324(midx, midy) == g_7404) {
        f_133f6(midx, midy,
                (f_13324(x1, y1) + f_13324(x2, y1) +
                 f_13324(x2, y2) + f_13324(x1, y2)) >> 2);
    }
    f_ae56(x1, y1, midx, midy);
    f_ae56(midx, y1, x2, midy);
    f_ae56(midx, midy, x2, y2);
    f_ae56(x1, midy, midx, y2);
}
