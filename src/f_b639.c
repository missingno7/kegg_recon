typedef struct { short pad; short dx; short dy; short tail; } FRectPart;

void f_b639(int *r, FRectPart a, FRectPart b)
{
    r[0] += b.dx;
    r[1] += b.dy;
    r[2] = r[0] + a.dx;
    r[3] = r[1] + a.dy;
}
