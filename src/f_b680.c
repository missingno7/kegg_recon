typedef struct { short pad; short dx; short dy; short tail; } FRectPart;

short f_b680(FRectPart a, FRectPart b, int ignored, int dx, int dy,
           int right, int bottom, int maxx, int maxy)
{
    int xend;
    int yend;
    if ((xend = b.dx + dx) <= maxx)
        if ((yend = b.dy + dy) <= maxy)
            if (a.dx + xend >= right)
                if (a.dy + yend >= bottom)
                    return -1;
    return 0;
}
