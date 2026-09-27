/*
 * TIMER.C  -  timer, retrace and clock routines
 */
#include <i86.h>
#include <conio.h>
extern short windows_environment_detected;
extern int f_9f64(void);
extern unsigned char g_756f[];
extern unsigned char g_7585;
extern unsigned char g_7586;
extern unsigned char g_75a8;
extern void f_9afc(void);
extern void f_d656(void *, int);
void f_9ac4(void);
extern int a_a03f(int);
extern void f_d7b8(void *);
extern unsigned g_7588;
extern int g_758c;
extern int g_7590;
extern int g_7594;
extern int g_75a4;
extern unsigned g_75c4;
extern void __far a_a067(void);
void f_9d40(short a0);
extern void f_9f48(void);
extern int f_da01(void *);
extern void __far o2_103(void);
extern void __far o2_e0(void);
extern void (*kbd_irq_hook)(void);
extern void (*kbd_poll_hook)(void);
extern void (*mouse_update_hook)(void);
extern void (*sprite_update_hook)(void);
extern void f_9e10(void);
extern void f_9e54(void);

struct T06Event { void (*callback)(void); int period; int elapsed; };
int g_e164_6;
struct T06Event g_e168_j[5];
int g_e1a4_g;
int g_e1a8_jn;
int g_e1ac_j;
int g_e1b0_2;
int g_e1b4_35;
int g_e1b8;
short g_e1bc;
short g_e1be_9;
short g_e1c0;

void f_9960(void);
short g_73a4 = 0;
short g_73a6 = 0;
int g_73a8 = 0xffff;
unsigned g_73ac = 0xffff;
unsigned g_73b0 = 0x445f0000UL;
short *kbd_state_ptr = &g_73a4;
void (*kbd_irq_hook)(void) = f_9960;
void (*kbd_poll_hook)(void) = f_9960;
void (*mouse_update_hook)(void) = f_9960;
void (*sprite_update_hook)(void) = f_9960;
unsigned char *g_73c8 = "\0";
short g_73cc = 0;
short g_73ce = 0x6d20;
char *g_73d0 = "Please Wait, I am updating your clock...";



/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9960  empty default hook                                     лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9960(void)
{
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9974  measure the timer                                      лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int f_9974(void)
{
    int v_4;
    int v_8;
    int v_c;
    int v_10;
    v_c = 0;
    v_10 = 0;
    if (windows_environment_detected != -1) goto L_99b2;
    g_73a6 = 0;
    return g_73a6;
L_99b2:;
    v_4 = 0;
L_99b9:;
    if (v_4 < 4) goto L_99cc;
    goto L_9a51;
L_99c4:;
    v_4++;
    goto L_99b9;
L_99cc:;
    v_8 = f_9f64();
    v_10 += v_8;
    if (v_8 > 0x61a8) goto L_99ec;
    if (v_8 >= 0x2710) goto L_9a04;
L_99ec:;
    g_73a6 = 0;
    return g_73a6;
L_9a04:;
    if (v_c == 0) goto L_9a46;
    v_c -= v_8;
    if (v_c >= 0) goto L_9a19;
    v_c = -v_c;
L_9a19:;
    if ((v_c / 0x200) <= 0) goto L_9a46;
    g_73a6 = 0;
    return g_73a6;
L_9a46:;
    v_c = v_8;
    goto L_99c4;
L_9a51:;
    v_10 = (v_10 / 4) - f_9f64();
    if (v_10 >= 0) goto L_9a77;
    v_10 = -v_10;
L_9a77:;
    if (((v_10 * 4) / 0x200) <= 0) goto L_9aa7;
    g_73a6 = 0;
    return g_73a6;
L_9aa7:;
    g_73a6 = -1;
    return g_73a6;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9ac4                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9ac4(void)
{
    g_7586 = 8;
    g_7585 = g_75a8;
    f_d656(g_756f, (int)f_9afc);
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9afc                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9afc(void)
{
    _disable();
    f_d7b8(g_756f);
    a_a03f(0);
    _enable();
    g_73ac = 0xffff;
    g_73a8 = g_73ac;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9b44                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

int f_9b44(int a0)
{
    if (a0 != 0) goto L_9b70;
    a0 = g_7588;
    if (a0 != 0) goto L_9b70;
    return 0;
L_9b70:;
    if (g_73a6 != -1) goto L_9b88;
    if (*(short *)g_756f != -1) goto L_9b8d;
L_9b88:;
    goto L_9c81;
L_9b8d:;
    g_73ac = f_9f64();
    if (g_73ac > 0x61a8) goto L_9baf;
    if (g_73ac >= 0x2710) goto L_9bc4;
L_9baf:;
    g_73a6 = 0;
    return 0;
L_9bc4:;
    g_7588 = a0;
    f_9ac4();
    g_73a8 = g_73ac - 0x100;
    f_9f48();
    g_e1b4_35 = 1;
    g_e1be_9 = 1;
    _disable();
    g_758c = (int)a_a067;
    g_7590 = (int)o2_e0;
    g_7594 = (int)o2_103;
    f_da01(g_756f);
    a_a03f(0xffff);
    _enable();
    if ((a0 & 1) != 1) goto L_9c59;
    g_73c8 = (unsigned char *)g_75a4;
L_9c59:;
    if (g_75c4 == 0) goto L_9c6b;
    return 0x502;
L_9c6b:;
    f_9d40(0);
    f_9d40(0);
L_9c81:;
    return 0;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9c90                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9c90(void)
{
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9ca4                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

short f_9ca4(void)
{
    unsigned short lo, hi;
    outp(0x43, 0);
    lo = inp(0x40);
    hi = inp(0x40);
    g_e164_6 = g_e1ac_j;
    g_e1ac_j = ((unsigned short)hi << 8) + (unsigned short)lo;
    if (g_e1ac_j < g_e164_6) {
    } else {
        g_e164_6 += g_73a8;
    }
    g_e1b0_2 = (g_e164_6 - g_e1ac_j) * 100 / 0x4280;
    return g_e1b0_2;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9d40                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9d40(short a0)
{
    int v_4;
    v_4 = g_e1be_9;
    if ((unsigned short)(a0 & 1) != 1) goto L_9d6a;
    (*kbd_irq_hook)();
L_9d6a:;
    if ((unsigned short)(a0 & 1) != 1) goto L_9d7e;
    (*kbd_poll_hook)();
L_9d7e:;
    if ((a0 & 2) == 0) goto L_9d8c;
    (*mouse_update_hook)();
L_9d8c:;
    if ((a0 & 4) == 0) goto L_9d9a;
    (*sprite_update_hook)();
L_9d9a:;
    if (*(short *)g_756f != -1) goto L_9ddf;
    g_e1a8_jn = 0;
L_9db0:;
    if (g_e1be_9 != v_4) goto L_9ddd;
    ++g_e1a8_jn;
    if (*(unsigned char *)g_73c8 == 0) goto L_9ddb;
    ++g_e1be_9;
    *(unsigned char *)g_73c8 = 0;
L_9ddb:;
    goto L_9db0;
L_9ddd:;
    goto L_9df3;
L_9ddf:;
    g_e1a8_jn = 0;
    f_9e10();
    f_9e54();
L_9df3:;
    g_e1c0 = g_e1be_9;
    g_e1be_9 = 0;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9e10  wait for vertical retrace                              лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9e10(void)
{
L_9e1c:;
    if (((unsigned char)inp(0x3da) & 8) == 0) goto L_9e35;
    ++g_e1a8_jn;
    goto L_9e1c;
L_9e35:;
    if (((unsigned char)inp(0x3da) & 8) != 0) goto L_9e4e;
    ++g_e1a8_jn;
    goto L_9e35;
L_9e4e:;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9e54                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9e54(void)
{
    unsigned short v_4;
    if (*(short *)kbd_state_ptr != 0) goto L_9e72;
    ++g_e1be_9;
L_9e72:;
    v_4 = 0;
L_9e79:;
    if ((unsigned short)v_4 < (unsigned short)g_73cc) goto L_9e8f;
    return;
L_9e87:;
    v_4++;
    goto L_9e79;
L_9e8f:;
    *(int *)(((unsigned char *)&g_e168_j[0].elapsed) + (v_4 * 0xc)) += g_73ac;
    if ((unsigned)*(int *)(((unsigned char *)&g_e168_j[0].elapsed) + (v_4 * 0xc)) < *(int *)(((unsigned char *)&g_e168_j[0].period) + (v_4 * 0xc))) goto L_9ee5;
    *(int *)(((unsigned char *)&g_e168_j[0].elapsed) + (v_4 * 0xc)) -= *(int *)(((unsigned char *)&g_e168_j[0].period) + (v_4 * 0xc));
    (*(int (**)())(((unsigned char *)g_e168_j) + (v_4 * 0xc)))();
L_9ee5:;
    goto L_9e87;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9eec                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9eec(int a0, int a1)
{
    if ((unsigned short)g_73cc >= 5) goto L_9f43;
    *(int *)(((unsigned char *)g_e168_j) + ((unsigned short)g_73cc * 0xc)) = a0;
    *(int *)(((unsigned char *)&g_e168_j[0].period) + ((unsigned short)g_73cc * 0xc)) = a1;
    *(int *)(((unsigned char *)&g_e168_j[0].elapsed) + ((unsigned short)g_73cc * 0xc)) = 0;
    ++g_73cc;
L_9f43:;
}

/*лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лл f_9f48                                                         лл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл
  лллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллллл*/

void f_9f48(void)
{
    g_73cc = 0;
}
