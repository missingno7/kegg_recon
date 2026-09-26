extern int g_e200;
extern int g_e204;
extern unsigned char g_e208;
extern int g_e209;
extern int g_e20d;
extern int g_e219;
extern int g_e21d;
extern int g_e221;
extern int g_e225;

int f_b5df(int *r, int *bounds)
{
    if (r[0] > bounds[2]) goto outside;
    if (r[1] > bounds[3]) goto outside;
    if (r[2] < bounds[0]) goto outside;
    if (r[3] < bounds[1]) goto outside;
    return -1;
outside:
    return 0;
}
