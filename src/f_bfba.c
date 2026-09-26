#include <string.h>

extern signed short g_747c;
extern unsigned char g_7486, g_75a8, g_75ac;
extern unsigned char g_74da, g_74db, g_7513, g_7514;
extern signed short g_74fd, g_74c4;
extern int g_7424, g_7428;
extern char *g_741c;
extern void f_c3ab(void);
extern void f_115da(void);
extern void f_d7b8(void *);
extern void f_df49(int);
extern void f_d656(void *, void (*)(void));
extern int f_b804(char *);
extern void f_c011(void);

void f_bfba(void)
{
    if (g_747c == -1) {
        g_7514 = g_7486 + 8;
        g_7513 = g_7514;
        if (g_7486 >= 8)
            g_7513 += 0x60;
        f_d656(&g_74fd, f_c011);
    }
}
