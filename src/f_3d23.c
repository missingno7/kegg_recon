#include <string.h>
typedef struct { unsigned char prefix:2; unsigned char code:6; } CodeBits;
extern int g_dd74;
extern unsigned char *g_dd7c;
extern int g_dd54;
extern unsigned int g_dd80;
extern int g_8e20;
extern int g_8e1c;
extern unsigned char g_a608;
extern unsigned char g_6b9e[];

void f_3ac0(int, int, int);

void f_3d23(void)
{
    g_8e20 = (*(int *)g_dd7c) % 18;
    g_8e1c = *(int *)g_dd7c - g_8e20;
    g_dd80 = (unsigned int)&g_a608;
    g_dd80 += (g_8e20 + g_8e1c) * 2;
    ((unsigned char *)g_dd80)[1] = g_dd7c[8];
    ((CodeBits *)g_dd80)->code = g_6b9e[g_dd7c[8]];
    f_3ac0(g_8e20, g_8e1c, g_dd7c[8]);
    --g_dd74;
    if (g_dd54 != g_dd74)
        memcpy(g_dd7c, g_dd7c + 9, (g_dd74 - g_dd54) * 9);
    --g_dd54;
}
