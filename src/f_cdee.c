#include <stdlib.h>
#include <string.h>
struct Config { unsigned char b[57]; };
struct ChoiceList { unsigned char b[7]; };
extern unsigned char f_cac2[], f_cafb[];
extern int g_e2f4;
extern char *g_748c;
extern unsigned char g_7486, g_7db3;
extern unsigned int g_75a8, g_75ac;
extern void f_d656(unsigned char *, int);
extern int f_da01(unsigned char *);
extern void f_d7b8(unsigned char *);
extern void f_11485(void), f_11420(void);
extern void __interrupt f_d252(void);
int f_cdee(void) {
    struct Config cfg;
    unsigned char idx;
    struct ChoiceList choices;
    int i;
    int temp;
    char * p;
    cfg = *(struct Config *)f_cac2;
    idx = 0;
    choices = *(struct ChoiceList *)f_cafb;
    temp = 0;
    g_e2f4 = -1;
again:
    if (temp == 0) {
        temp = -1;
        g_7486 = 0xff;
        p = getenv(g_748c);
        if (p != 0) {
            p = strchr(p, 0x49);
            if (p == 0)
                goto parsed;
            g_7486 = (unsigned char)(p[1] - 0x30);
            if (p[2] >= 0x30 && p[2] <= 0x39)
                g_7486 = (unsigned char)(g_7486 * 10 + p[2] - 0x30);
parsed:
            if (g_7486 == 2)
                g_7486 = 9;
        }
        if (g_7486 == 0xff)
            g_7486 = choices.b[idx++];
    } else {
        g_7486 = choices.b[idx++];
    }
    cfg.b[0x17] = (unsigned char)(g_7486 + (unsigned char)g_75a8);
    cfg.b[0x16] = cfg.b[0x17];
    if (g_7486 >= 8)
        cfg.b[0x16] += (unsigned char)g_75ac - 8 - (unsigned char)g_75a8;
    *(unsigned int *)(cfg.b + 0x19) = 4;
    f_d656(cfg.b, 0);
    *(unsigned int *)(cfg.b + 0x1d) = (unsigned int)f_d252;
    f_da01(cfg.b);
    f_11485();
    g_7db3 = 0xf2;
    f_11420();
    for (i = 0; i < 0xc350; i++) {
        if (g_e2f4 == 0)
            break;
    }
    if (g_e2f4 == -1)
        f_11485();
    g_7db3 = 0x80;
    f_11420();
    g_7db3 = 3;
    f_11420();
    g_7db3 = 0;
    f_11420();
    for (i = 0; i < 0xc350; i++) {
        if (g_e2f4 == 0)
            break;
    }
    if (g_e2f4 == -1)
        g_7486 = 0xff;
    f_d7b8(cfg.b);
    if (choices.b[idx] == 0xff)
        goto done;
    if (g_e2f4 == -1)
        goto again;
done:
    return g_e2f4;
}