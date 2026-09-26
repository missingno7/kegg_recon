extern int g_7b04, g_7b08, g_7b0c;
extern void f_13a48(void *, int, int, int);
extern void f_9d40(short);
void f_ea9f(void * a, int x, int y, int step) {
    int i;
    if (x < -63) x = -63;
    if (x > 63) x = 63;
    if (y < -63) y = -63;
    if (y > 63) y = 63;
    if (g_7b04 < 0) g_7b04 = 0;
    if (g_7b04 > 255) g_7b04 = 255;
    if (g_7b08 < 1) g_7b08 = 1;
    if (g_7b08 > 256) g_7b08 = 256;
    if (!step) {
        if (y - x > 0) step = 1;
        else step = -1;
    }
    do {
        x += step;
        if (step > 0) {
            if (x > y) x = y;
        } else if (x < y) x = y;
        f_13a48(a, g_7b04, g_7b08, x);
        for (i = 0; i < g_7b0c; i++) f_9d40(0);
    } while (x != y);
}
