#include <conio.h>
extern short g_747c, g_e2fc;
extern unsigned char g_7486, g_7487, g_7db2, g_7db3;
extern unsigned int g_747e, g_7482;
extern int g_e2f8;
extern int f_ccd5(void), f_cdee(void), f_d010(void);
extern int f_1144d(void), f_11420(void), f_113f8(void), f_11485(void);
int f_cb3f(void) {
    g_747c = 0;
    g_7486 = 0xff;
    g_7487 = 0xff;
    if (f_ccd5() != 0) goto done;
    if (f_cdee() != 0) goto done;
    if (f_d010() != 0) goto done;
    g_747c = -1;
    f_1144d();
    g_7db3 = 0xd1;
    f_11420();
    outp(g_e2fc + 4, 0x22);
    g_e2f8 = inp(g_e2fc + 5);
    outp(g_e2fc + 4, 0x22);
    outp(g_e2fc + 5, 0x55);
    outp(g_e2fc + 4, 0x22);
    if (inp(g_e2fc + 5) == 0x55)
        g_747e = 1;
    else
        g_747e = 0;
    outp(g_e2fc + 4, 0x22);
    outp(g_e2fc + 5, g_e2f8);
    g_7db3 = 0xe1;
    f_11420();
    f_113f8();
    g_7482 = g_7db2 << 8;
    f_113f8();
    g_7482 += ((g_7db2 / 10) << 4) | (g_7db2 % 10);
done:
    return g_747c;
}
