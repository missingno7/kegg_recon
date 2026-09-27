/* TU T16: f_f690..f_ff9d [0xf690, 0xffbe), code and owned data. */
#include <stdlib.h>
#include <i86.h>
#include <conio.h>

union KeyWord { unsigned short word; struct { unsigned char lo, hi; } bytes; };
union FlagWord { unsigned short word; unsigned char bytes[2]; };
extern union KeyWord zz_1483[8], zz_1482[8];
extern unsigned char zz_1481, zz_1480, zz_1487, zz_1486, zz_1485, zz_1484, zz_1489, zz_1488;
/* Watcom 10.0 orders _BSS by identifier hash; these storage names map
   the key-word arrays to e468/e478 and state bytes to e488..e48f. */
#define kbd_primary zz_1483
#define kbd_secondary zz_1482
#define kbd_read zz_1480
#define kbd_previous zz_1487
#define kbd_scan zz_1486
#define kbd_last zz_1485
#define kbd_shift zz_1484
#define kbd_current_previous zz_1489
#define kbd_current zz_1488
#define kbd_last_read zz_1481
void f_f690(void);
extern unsigned char g_754d;
extern unsigned char g_754c;
extern unsigned char g_75a8;
struct InterruptRecord { short status; unsigned char opaque[0x33]; };
extern struct InterruptRecord g_7536;
extern void f_f6d9(void);
extern void f_d656(unsigned char *, int);
void f_f6a1(void);
extern void f_d7b8(unsigned char *);
extern int g_754f;
extern int g_756b;
extern int g_75c4;
extern void (*g_7553)(void);
extern void (*g_7557)(void);
extern void (*g_755b)(void);
extern void f_f875(void);
extern void f_f7e4(void);
extern void f_da01(int *);
void __interrupt f_ff5c(void);
extern void __far irq_110(void);
extern void f_f82d(void);
extern void f_f905(unsigned char);
void f_f8bb(void);
extern int kbhit(void);
extern int getch(void);
void f_fa42(void);
extern void f_fe6a(void);
extern void f_fbc0(void);
extern void f_fc1e(void);
extern void f_fb92(void);
extern void f_fd3b(void);
extern void f_fd7b(void);
extern void f_fcab(void);
extern void (*g_7b3f)(void);
extern void (*g_7b43)(void);
extern void (*g_7b47)(int, int);
int f_ff0d(void);

extern void f_f690(void);
union FlagWord g_7b34 = {0x003c};
unsigned char g_7b36 = 0;
unsigned char g_7b37_reserved[2] = {0, 0};
short g_7b39 = 0;
short g_7b3b = 0;
short g_7b3d = 0;
void (*g_7b3f)(void) = f_f690;
void (*g_7b43)(void) = f_f690;
void (*g_7b47)(int, int) = (void (*)(int, int))f_f690;
unsigned int g_7b4b[9] = {
    0x000b033d, 0x000a0315, 0x000902af, 0x00110512,
    0xffffffff, 0xffffffff, 0xffffffff, 0x0004012f, 0x0005017a
};
short g_7b6f = 0;
char *g_7b71 = "";
unsigned int g_7b75 = 0;
unsigned char g_7b79[128] = {
    0x3f, 0x3f, 0x26, 0x82, 0x22, 0x27, 0x28, 0xf5, 0x8a, 0x21, 0x87, 0x85, 0x29, 0x2d, 0x3f, 0x3f,
    0x41, 0x5a, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4f, 0x50, 0x3f, 0x3f, 0x3f, 0x3f, 0x51, 0x53,
    0x44, 0x46, 0x47, 0x48, 0x4a, 0x4b, 0x4c, 0x4d, 0x25, 0x3f, 0x3f, 0x5c, 0x57, 0x58, 0x43, 0x56,
    0x42, 0x4e, 0x3f, 0x3b, 0x3a, 0x2f, 0x3f, 0x2a, 0x3f, 0x20, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x37, 0x38, 0x39, 0x2d, 0x34, 0x35, 0x36, 0x2b, 0x31,
    0x32, 0x33, 0x30, 0x2e, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f
};
unsigned char g_7bf9_reserved[3] = {0, 0, 0};

void f_f690(void) { }

void f_f6a1(void) {
    g_754d = 9;
    g_754c = g_75a8 + 1;
    f_d656((unsigned char *)&g_7536, (int)f_f6d9);
}

void f_f6d9(void) { f_d7b8((unsigned char *)&g_7536); }

int f_f6f8(int mode) {
    int p;
    if (mode == 0) {
        mode = g_754f;
        if (mode == 0) return 0;
    }
    if (g_7536.status != -1) {
        f_f875();
        f_f7e4();
        g_754f = mode;
        f_f6a1();
        g_7553 = (void (*)(void))f_ff5c;
        g_7557 = (void (*)(void))irq_110;
        g_755b = (void (*)(void))((unsigned char __far *)irq_110 + 0x39);
        f_da01((int *)&g_7536);
        if ((mode & 1) == 1) {
            int addr1;
            int addr2;
            p = g_756b;
            addr1 = 0x41a;
            *(unsigned short *)(p + 4) = *(unsigned short *)addr1;
            addr2 = 0x41c;
            *(unsigned short *)(p + 6) = *(unsigned short *)addr2;
            g_7b71 = (char *)p;
        }
        if (g_75c4) return 0x501;
    }
    return 0;
}

void f_f7e4(void) {
    int p;
    p = 0x417;
    if (g_7b6f != -1) {
        kbd_shift = *(unsigned char *)p >> 4;
        atexit(f_f82d);
        g_7b6f = -1;
    }
}

void f_f82d(void) {
    unsigned char *p;
    p = (unsigned char *)0x417;
    if (g_7b6f == -1) {
        *p = (*p & 0x8f) | ((kbd_shift << 4) & 0x70);
        g_7b6f = 1;
    }
}

void f_f875(void) {
    int i;
    for (i = 0; i < 8; i++) {
        kbd_primary[i].word = 0;
        kbd_secondary[i].word = 0;
    }
}

void f_f8bb(void) {
    f_f905((unsigned char)inp(0x60));
    if (*(unsigned char *)g_7b71) {
        f_f905(*(unsigned char *)g_7b71);
        *(unsigned char *)g_7b71 = 0;
    }
}

void f_f905(unsigned char key) {
    if ((key & 0x6f) > 0x60) return;
    kbd_scan = key;
    key &= 0x7f;
    kbd_read = g_7b79[key];
    key = (unsigned char)((int)key >> 4);
    if (kbd_scan & 0x80) {
        kbd_primary[key].word = kbd_primary[key].word & (unsigned short)~(1 << (kbd_scan & 0x0f));
        key = (unsigned char)((int)kbd_read >> 4);
        kbd_secondary[key].word = kbd_secondary[key].word & (unsigned short)~(1 << (kbd_read & 0x0f));
        kbd_read |= kbd_scan & 0x80;
    } else {
        kbd_primary[key].word = kbd_primary[key].word | (unsigned short)(1 << (kbd_scan & 0x0f));
        key = (unsigned char)((int)kbd_read >> 4);
        kbd_secondary[key].word = kbd_secondary[key].word | (unsigned short)(1 << (kbd_read & 0x0f));
    }
}

void f_fa42(void) {
    if (g_7536.status != -1) {
        f_f8bb();
        if (kbhit()) {
            kbd_read = (unsigned char)getch();
            for (kbd_scan = 0; kbd_scan < 0x7f; ++kbd_scan) {
                if (kbd_read == g_7b79[kbd_scan]) break;
            }
        }
    } else if (*(unsigned char *)g_7b71) {
        f_f905(*(unsigned char *)g_7b71);
        *(unsigned char *)g_7b71 = 0;
    }
    if (kbd_previous == kbd_current) goto update;
    if (kbd_current != kbd_scan) goto update;
    goto finish;
update:
        kbd_current_previous = kbd_last_read;
        kbd_last = kbd_current;
finish:
    kbd_current = kbd_scan;
    kbd_last_read = kbd_read;
    kbd_previous = kbd_current;
}

void f_fb17(void) {
    if (g_7b34.word & 0x80) f_fe6a();
    if ((g_7b34.word & 1) == 1) f_fbc0();
    if (g_7b34.word & 2) f_fc1e();
    if (g_7b34.word & 4) f_fb92();
    if (g_7b34.word & 0x10) f_fd3b();
    if (g_7b34.word & 0x20) f_fd7b();
    if (g_7b34.word & 0x40) f_fcab();
}

void f_fb92(void) { if (kbd_current_previous != 0x20 && kbd_last_read == 0x20) g_7b3d = -1; }

void f_fbc0(void) { while ((kbd_primary[3].bytes.lo & 0x40) && (kbd_primary[0].bytes.hi & 0x40)) { f_fa42(); if (g_7b39 == 0) { g_7b3f(); g_7b39 = -1; } } if (g_7b39 != 0) { g_7b43(); g_7b39 = 0; } }

void f_fc1e(void)
{
    if (kbd_current != 25) goto L_fc81;
    g_7b3f();
    g_7b39 = -1;
L_fc42:;
    while (kbd_current == 25) {
    f_fa42();
    }
L_fc52:;
    while (kbd_current != 25) {
    f_fa42();
    }
L_fc62:;
    while (kbd_current == 25) {
    f_fa42();
    }
L_fc72:;
    g_7b43();
    g_7b39 = 0;
L_fc81:;
}

void f_fc86(void) { g_7b3f = f_f690; g_7b43 = f_f690; }

void f_fcab(void) { if ((kbd_primary[2].bytes.hi & 4) && (kbd_primary[2].bytes.hi & 2)) g_7b3b = 1; else g_7b3b = 0; }

void f_fce4(void) { if (kbd_primary[0].bytes.lo&2) goto test_high; goto low_skip; test_high: if (((int)(short)kbd_primary[0].word&0x8000) != 0) goto test_mask; low_skip: goto mask_skip; test_mask: if (kbd_primary[1].bytes.hi&0x20) goto test_b; mask_skip: goto b_skip; test_b: if (kbd_last==0x1c) goto b_skip; test_e: if (kbd_current==0x1c) goto call_key; goto b_skip; b_skip: goto end; call_key: g_7b47(0x101,0); end: ; }

void f_fd3b(void) {
    if ((kbd_primary[1].bytes.hi & 0x20) && (kbd_primary[3].bytes.hi & 1) && (kbd_primary[5].bytes.lo & 8)) g_7b47(0x101, 0);
}

void f_fd7b(void) { unsigned char old=kbd_shift; int k46,k45,k3a; if (kbd_last!=0x46 && kbd_current==0x46) k46=1; else k46=0; if(k46) kbd_shift^=1; if (kbd_last!=0x45 && kbd_current==0x45) k45=1; else k45=0; if(k45) kbd_shift^=2; if (kbd_last!=0x3a && kbd_current==0x3a) k3a=1; else k3a=0; if(k3a) kbd_shift^=4; if(old!=kbd_shift) { _disable(); f_ff0d(); outp(0x60,0xed); f_ff0d(); outp(0x60,kbd_shift&7); _enable(); } }

void f_fe6a(void) { int i; if (kbd_current != kbd_last && kbd_current < 0x7f) { if (kbd_current == 0x1c) { for (i=0;(short)i<9;i++) { if (g_7b4b[(short)i] == g_7b75) { if ((short)i==8) g_7b36 ^= 0xff; else g_7b36 ^= 1<<(short)i; } } g_7b75=0; } else g_7b75 += (unsigned short)kbd_last_read + 0x10000U; } }

int f_ff0d(void) { int n=0x1388; poll: if ((inp(0x64)&2)==0) goto poll_exit; if (n>0) goto decrement; poll_exit: goto finish; decrement: n--; goto poll; finish: if(n>0) return 0; return -1; }

void __interrupt f_ff5c(void) {
    f_13a88();
    f_f8bb();
    outp(0x20, 0x20);
    if (g_7b34.bytes[0] & 8)
        f_fce4();
}

void f_ff9d(void) { g_7b47(0x101, 0); }


union KeyWord zz_1482[8];
union KeyWord zz_1483[8];
unsigned char zz_1481;
unsigned char zz_1480;
unsigned char zz_1487;
unsigned char zz_1486;
unsigned char zz_1485;
unsigned char zz_1484;
unsigned char zz_1489;
unsigned char zz_1488;
