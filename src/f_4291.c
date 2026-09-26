#include <string.h>
extern int g_ddc0;
extern int g_ddbc;
extern int g_dda4;
void f_4291(void)
{
    --g_ddc0;
    if (g_ddbc != g_ddc0)
        memmove((void *)g_dda4, (void *)(g_dda4 + 0x12), (g_ddc0 - g_ddbc) * 0x12);
    --g_ddbc;
}
