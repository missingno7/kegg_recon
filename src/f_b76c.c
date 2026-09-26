typedef struct { short r0; short w; short h; short r1; short r2; short ox; short oy; short r3; short r4; } Frame;

short f_b76c(Frame a, int ax0, int ay0, Frame b, int bx0, int by0, int mx, int my, int ix, int iy)
{
    int ys; int ax; int xs; int ay;
    if ((ax = ax0 + a.ox) + mx > (xs = bx0 + b.ox) + b.w) goto outside;
    if ((ay = ay0 + a.oy) + my > (ys = by0 + b.oy) + b.h) goto outside;
    if (a.w + ax - ix < xs) goto outside;
    if (a.h + ay - iy < ys) goto outside;
    return -1;
outside:
    return 0;
}
