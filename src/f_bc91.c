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

int f_bc91(char *name)
{
    int result;
    char *dot;
    if (!name)
        name = g_741c;
    result = f_b804(name);
    if (result)
        return result;
    dot = strchr(name, '.');
    if (dot) {
        if (dot[-1]++ == '9') {
            dot[-1] = '0';
            if (dot[-2]++ == '9') {
                dot[-2] = '0';
                dot[-3]++;
            }
        }
    }
    return 0;
}
