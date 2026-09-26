typedef unsigned int size_t;
extern int g_dda0;
extern int g_ddc8;
extern char *g_dd90;
void *memcpy(void *, const void *, size_t);

void f_69cd(void)
{
    --g_ddc8;
    if (g_dda0 != g_ddc8)
        memcpy(g_dd90, g_dd90 + 20, (g_ddc8 - g_dda0) * 20);
    --g_dda0;
}
