#include <string.h>
#pragma pack(1)
typedef struct { unsigned char bytes[9]; } Name;
typedef struct { Name name; unsigned char other[16]; } Row;
#pragma pack()
extern Row g_37f4[];
extern Name g_1032;
extern unsigned int g_37fd[];
extern unsigned g_3861;
extern unsigned char g_8e46;
extern unsigned g_dedc;
extern unsigned char g_e141;
extern char *strcpy(char *, const char *);
void f_1d80(void)
{
    if (g_dedc < g_3861) goto L_1e78;
    g_8e46 = 5;
L_1da4:;
    --g_8e46;
    if (g_dedc < *(unsigned int *)((unsigned char *)g_37fd + ((g_8e46 - 1) * 0x19))) goto L_1dcc;
    if (g_8e46 > 0) goto L_1da4;
L_1dcc:;
    g_e141 = 4;
L_1dd3:;
    --g_e141;
    if (g_8e46 > g_e141) goto L_1e34;
    strcpy((char *)g_37f4 + ((g_e141 + 1) * 0x19),
           (char *)g_37f4 + (g_e141 * 0x19));
    *(unsigned int *)((unsigned char *)g_37fd + ((g_e141 + 1) * 0x19)) =
        *(unsigned int *)((unsigned char *)g_37fd + (g_e141 * 0x19));
L_1e34:;
    if (g_e141 > 0) goto L_1dd3;
    g_37f4[g_8e46].name = g_1032;
    *(unsigned int *)((unsigned char *)g_37fd + (g_8e46 * 0x19)) = g_dedc;
    g_dedc = g_8e46;
    return;
L_1e78:;
    g_dedc = 0xffffffff;
}
