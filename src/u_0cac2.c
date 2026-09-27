int g_e2f4;
int g_e2f8;
short g_e2fe;
/* TU [0xcac2, 0xd2f0): _TEXT tables, f_cb3f..f_d288 (sound-card detection); from worker u12 T12.c */
#include <conio.h>
int g_e2f0;
short g_e2fc;

#include <stdlib.h>
#include <string.h>
#include <i86.h>
struct Config { unsigned char b[57]; };
struct ChoiceList { unsigned char b[7]; };
struct Defaults { unsigned char b[4]; };
struct MapConfig { unsigned char opaque[0x2d]; char *allocation; unsigned int base; char *mapped; };
extern unsigned char g_7db2;
extern unsigned char g_7db3;
extern int f_ccd5(void);
extern int f_cdee(void);
extern int f_d010(void);
extern int f_1144d(void);
extern int f_11420(void);
extern int f_113f8(void);
extern int f_11485(void);
extern void f_d656(unsigned char *, int);
extern int f_da01(unsigned char *);
extern void f_d7b8(unsigned char *);
extern void __interrupt f_d252(void);
extern unsigned int g_e31c;
extern unsigned int g_7db4;
extern short g_7db8;
extern unsigned char g_7dba;
extern unsigned char g_7dbb;
extern unsigned int f_dea6(int);
extern void f_df49(unsigned int);
extern void f_11494(void);
extern void f_114a0(void);
extern void f_11377(unsigned int);
extern void f_13a88(void);
extern void f_113bd(void);
extern unsigned int g_e300;
extern unsigned int g_e304;
extern unsigned int g_1258;
int f_d2f0(void);
int f_d36c(void);
int f_d408(void);
extern int g_e308;
extern int g_e30c;
extern int g_e310;
extern void f_14197(void *);
extern int g_75c4;
extern void f_13889(int, int, int);

extern short g_7498;
extern unsigned short u_749a;
extern unsigned short u_749c;
extern unsigned long g_749e;
extern unsigned short u_74a2;
extern short g_74a4;
extern unsigned long g_74a6;
extern unsigned short u_74aa;
extern short g_74ac;
extern unsigned long g_74ae;
extern unsigned short u_74b2;
extern short g_74b4;
extern unsigned long g_74b6;
extern unsigned short u_74ba;
extern short g_74bc;
extern unsigned long g_74be;
extern unsigned short u_74c2;
extern unsigned char g_74c4[57];
extern unsigned char g_74fd[57];
extern unsigned char g_7536[57];
extern unsigned char g_756f[57];
extern unsigned int g_75a8;
extern unsigned int g_75ac;
/* Watcom emits const aggregates in _TEXT before the function bodies. */
const unsigned char f_cac2[57] = { 0 };
const unsigned char f_cafb[7] = { 7, 5, 3, 10, 9, 2, 0xff };
const unsigned char f_cb02[57] = { 0 };
const unsigned char f_cb3b[4] = { 1, 3, 0, 0xff };


/* _DATA [0x747c,0x7498) */
short g_747c = 0;
unsigned int g_747e = 0xffffffffU;
unsigned int g_7482 = 0xffffffffU;
unsigned char g_7486 = 0xff;
unsigned char g_7487 = 0xff;
unsigned int u_7488 = 0;
char *g_748c = "BLASTER";
short g_7490 = 0;
unsigned long g_7492 = 0xffffffffUL;
unsigned short u_7496 = 0;

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

int f_ccd5(void)
{
    int v_4;
    int v_8;
    unsigned char * v_c;
    v_8 = 0;
    if (v_8 != 0) goto L_cd89;
    v_8 = -1;
    g_e2fc = -1;
    v_c = getenv((char *)g_748c);
    if (v_c == 0) goto L_cd89;
    v_c = strchr((char *)v_c, 0x41);
    if (v_c == 0) goto L_cd89;
    g_e2fc = (unsigned short)((((unsigned short)*(unsigned char *)(v_c + 1) - 0x30) << 8) + (((unsigned short)*(unsigned char *)(v_c + 2) - 0x30) << 4));
    v_4 = 0;
L_cd62:;
    if (v_4 < 5) goto L_cd72;
    goto L_cd89;
L_cd6a:;
    v_4++;
    goto L_cd62;
L_cd72:;
    if (f_1144d() != 0) goto L_cd87;
    return 0;
L_cd87:;
    goto L_cd6a;
L_cd89:;
    g_e2fc = 0x210;
L_cd92:;
    if (g_e2fc < 0x260) goto L_cda9;
    goto L_cdd6;
L_cd9f:;
    g_e2fc += 0x10;
    goto L_cd92;
L_cda9:;
    v_4 = 0;
L_cdb0:;
    if (v_4 < 5) goto L_cdc0;
    goto L_cdd4;
L_cdb8:;
    v_4++;
    goto L_cdb0;
L_cdc0:;
    if (f_1144d() != 0) goto L_cdd2;
    return 0;
L_cdd2:;
    goto L_cdb8;
L_cdd4:;
    goto L_cd9f;
L_cdd6:;
    g_e2fc = -1;
    return -1;
}

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

int f_d010(void) {
    struct Config cfg;
    unsigned char idx;
    struct Defaults defaults;
    int i;
    int temp;
    char *p;
    int size;
    unsigned int base;
    cfg = *(struct Config *)f_cb02;
    idx = 0;
    defaults = *(struct Defaults *)f_cb3b;
    temp = 0;
    size = 0;
    g_e2f0 = -1;
    base = f_dea6(0x1080);
    if (base != 0) {
        base = (base + 0x3ffc) & 0xffffefff;
        size = g_e31c;
    } else {
        return g_e2f0;
    }
again:
    if (temp == 0) {
        temp = -1;
        g_7487 = 0xff;
        p = getenv(g_748c);
        if (p != 0) {
            p = strchr(p, 0x44);
            if (p != 0)
                g_7487 = (unsigned char)(p[1] - 0x30);
        }
        if (g_7487 == 0xff)
            g_7487 = defaults.b[idx++];
    } else {
        g_7487 = defaults.b[idx++];
    }
    cfg.b[0x17] = (unsigned char)(g_7486 + (unsigned char)g_75a8);
    cfg.b[0x16] = cfg.b[0x17];
    if (g_7486 >= 8)
        cfg.b[0x16] += (unsigned char)g_75ac - 8 - (unsigned char)g_75a8;
    *(unsigned int *)(cfg.b + 0x19) = 4;
    f_d656(cfg.b, 0);
    *(unsigned int *)(cfg.b + 0x1d) = (unsigned int)f_d252;
    f_da01(cfg.b);
    g_7dbb = g_7487;
    f_11494();
    g_7db4 = base;
    g_7db8 = 4;
    g_7dba = 0x44;
    f_114a0();
    *(unsigned int *)base = 0x12345678;
    f_11377(0x3e80);
    g_7db3 = 0x24;
    f_11420();
    g_7db3 = g_7db8 - 1;
    f_11420();
    g_7db3 = (g_7db8 - 1) >> 8;
    f_11420();
    for (i = 0; i < 0xc350; i++) {
        if (*(unsigned int *)base != 0x12345678) {
            g_e2f0 = 0;
        }
    }
    f_11494();
    f_d7b8(cfg.b);
    if (defaults.b[idx] == 0xff)
        goto finish;
    if (g_e2f0 == -1)
        goto again;
finish:
    f_df49(size);
    size = 0;
    return g_e2f0;
}

void __interrupt f_d252(void) {
    f_13a88();
    f_11485();
    f_113bd();
    g_e2f4 = 0;
}

int f_d288(void) {
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    regs.w.ax = 0x3000;
    int386x(0x21, &regs, &regs, &sregs);
    g_7492 = (regs.h.al << 8) + regs.h.ah;
    return g_7490 = -1;
}

