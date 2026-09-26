/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <i86.h>
extern unsigned char g_756f[];
extern short g_75ca;
extern unsigned char g_75ce[];
extern unsigned char g_75d2[];
extern unsigned char g_75d6[];
extern unsigned char g_75da[];
extern unsigned char g_75db[];
extern unsigned char g_75dc[];
extern unsigned char g_75e0[];
extern unsigned char g_75e4[];
extern unsigned char g_75e8[];
extern unsigned char g_75ec[];
extern unsigned char g_77ee[];
extern unsigned char g_7822[];
extern short g_e324;
extern unsigned char g_e326[];
extern unsigned char g_e336[];
extern unsigned char g_e346[];
extern unsigned char g_e356[];
extern int g_e35a;
extern int g_e35e;
extern int g_e362;
extern int g_e366;
extern int g_e36a;
extern unsigned char *g_e36e;
extern unsigned char *g_e372;
extern int g_e376;
extern int g_e37a;
extern unsigned char g_e37e;
extern unsigned char g_e381;
extern int a_13824(int, int);
extern void f_9afc(void);
extern int f_9b44(int);
extern void f_dfc3(void);
extern void f_e473(int);
extern void f_e6b3(void *);
extern void f_e813(void);
extern void f_e855(short);
extern void f_ec9c(void);
int f_e095(int a0)
{
    int v_4;
    int v_8;
    int v_c;
    int v_10;
    int v_14;
    int v_18;
    int v_1c;
    unsigned char v_3c[28];
    v_8 = *(short *)g_756f;
    v_c = 0;
    if (g_75ca == -1) goto L_e0c3;
    f_dfc3();
L_e0c3:;
    if (*(int *)(g_75ce + (v_c * 0x22)) == a0) goto L_e0df;
    if (*(int *)(g_75ce + (v_c * 0x22)) != -1) goto L_e0e1;
L_e0df:;
    goto L_e0e9;
L_e0e1:;
    v_c++;
    goto L_e0c3;
L_e0e9:;
    if (*(int *)(g_75ce + (v_c * 0x22)) == -1) goto L_e3c7;
    f_9afc();
    f_ec9c();
    a_13824(0xa0000, 0x10000);
    if (g_e381 == *(int *)(g_75d6 + (v_c * 0x22))) goto L_e168;
    g_e381 = *(unsigned char *)(g_75d6 + (v_c * 0x22));
    *(short *)v_3c = *(short *)(g_75d6 + (v_c * 0x22));
    *(short *)(v_3c + 4) = *(short *)(g_75d2 + (v_c * 0x22));
    int386(0x10, (void *)v_3c, (void *)v_3c);
L_e168:;
    a_13824(0xa0000, 0x10000);
    g_e366 = *(int *)(g_75dc + (v_c * 0x22));
    g_e36a = *(int *)(g_75e0 + (v_c * 0x22));
    g_e35e = *(int *)(g_75e4 + (v_c * 0x22));
    g_e362 = g_e36a;
    g_e36e = 0;
    g_e372 = 0;
    g_e376 = g_e366 - 1;
    g_e37a = g_e36a - 1;
    g_e35a = *(int *)(g_75ec + (v_c * 0x22));
    switch (*(unsigned char *)(g_75db + (v_c * 0x22))) {
        case 0:
L_e211:;
        v_4 = 0xffff;
        v_18 = 0xa0000;
        f_e473(0);
        goto L_e266;
        case 1:
L_e22b:;
        v_4 = 0x3ffff;
        v_18 = 0x280000;
        f_e473(1);
        goto L_e266;
        case 8:
L_e245:;
        v_4 = 0xffff;
        v_18 = 0xa0000;
        f_e473(1);
        g_e324 = 0;
    }
L_e266:;
    g_e37e = *(unsigned char *)(g_75da + (v_c * 0x22));
    v_1c = 0;
    v_10 = 0;
L_e283:;
    if (v_10 < 4) goto L_e296;
    goto L_e30e;
L_e28e:;
    v_10++;
    goto L_e283;
L_e296:;
    *(int *)(g_e326 + (v_10 << 2)) = v_18;
    *(int *)(g_e336 + (v_10 << 2)) = v_1c;
    *(int *)(g_e346 + (v_10 << 2)) = 0;
    *(unsigned char *)(g_e356 + v_10) = *(unsigned char *)(g_75db + (v_c * 0x22));
    v_1c += *(int *)(g_75e8 + (v_c * 0x22));
    if ((v_1c + g_e35a) <= v_4) goto L_e2ff;
    v_1c -= *(int *)(g_75e8 + (v_c * 0x22));
L_e2ff:;
    f_e855((short)v_10);
    goto L_e28e;
L_e30e:;
    f_e813();
    v_14 = 0;
L_e31a:;
    if (*(int *)(g_7822 + (v_14 * 0x38)) != *(int *)(g_75ce + (v_c * 0x22))) goto L_e39a;
    f_e6b3((v_14 * 0x38) + g_77ee);
    switch (*(unsigned char *)(g_75db + (v_c * 0x22))) {
        case 0:
L_e36f:;
        f_e473(0);
        goto L_e39a;
        case 1:
L_e37b:;
        f_e473(1);
        goto L_e39a;
        case 8:
L_e387:;
        f_e473(1);
        g_e324 = 0;
    }
L_e39a:;
    ++v_14;
    if (*(int *)(g_7822 + (v_14 * 0x38)) != -1) goto L_e31a;
    if (v_8 != -1) goto L_e3be;
    f_9b44(0);
L_e3be:;
    return 0;
L_e3c7:;
    if (a0 == 0) goto L_e3d6;
    return 0x503;
L_e3d6:;
    return 0;
}
