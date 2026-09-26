typedef struct { short r0; short w; short h; short r1; short r2; short ox; short oy; short r3; short r4; } Frame;

short f_b6e4(Frame a, int ax0, int ay0, Frame b, int bx0, int by0)
{
    int ax; int ys; int xs; int ay;
    if ((xs = bx0 + b.ox) + b.w < (ax = ax0 + a.ox)) goto outside;
    if ((ys = by0 + b.oy) + b.h < (ay = ay0 + a.oy)) goto outside;
    if (a.w + ax < xs) goto outside;
    if (a.h + ay < ys) goto outside;
    return -1;
outside:
    return 0;
}
