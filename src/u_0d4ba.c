int g_e310;
/* TU [0xd4ba, 0xdce0): f_d4ba..f_da01 (DPMI / interrupt records); from worker u12 T12.c */
#include <conio.h>
int g_e308;
int g_e30c;

#include <stdlib.h>
#include <string.h>
#include <i86.h>
struct SoundBlasterConfig { unsigned char b[57]; };
struct SoundBlasterIrqChoices { unsigned char b[7]; };
struct SoundBlasterDmaChoices { unsigned char b[4]; };
struct DPMIMapRecord { unsigned char opaque[0x2d]; char *allocation; unsigned int base; char *mapped; };
extern short sound_blaster_base_port;
extern unsigned char g_7db2;
extern unsigned char g_7db3;
extern int saved_sound_mixer_value;
extern int select_sound_blaster_port(void);
extern int detect_sound_blaster_irq(void);
extern int detect_sound_blaster_dma(void);
extern int f_1144d(void);
extern int f_11420(void);
extern int f_113f8(void);
extern int f_11485(void);
extern int sound_irq_test_complete_l;
extern void f_d656(unsigned char *, int);
extern int f_da01(unsigned char *);
extern void f_d7b8(unsigned char *);
extern void __interrupt sound_test_irq_handler(void);
extern int sound_dma_test_result;
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
extern void copy_ds_to_es(void);
extern void f_113bd(void);
extern unsigned int g_e300;
extern unsigned int g_e304;
extern unsigned int g_1258;
int f_d2f0(void);
int f_d36c(void);
int f_d408(void);
extern void f_14197(void *);
extern int g_75c4;
extern void f_13889(int, int, int);

extern short sound_blaster_detected;
extern unsigned int sound_blaster_mixer_test;
extern unsigned int sound_blaster_dsp_version;
extern unsigned char sound_blaster_irq;
extern unsigned char sound_blaster_dma_channel;
extern unsigned int u_7488;
extern char *sound_blaster_env_name;
extern short dos_version_query_succeeded;
extern unsigned long dos_version_packed;
extern unsigned short u_7496;
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

/* _DATA [0x74b4,0x75b0): DPMI handles and four 57-byte interrupt records (split at the field names
   other objects import) */
short g_74b4 = 0;
unsigned long g_74b6 = 0xffffffffUL;
unsigned short u_74ba = 0;
short g_74bc = 0;
unsigned long g_74be = 0xffffffffUL;
unsigned short u_74c2 = 0;
unsigned char g_74c4[22] = {0};
unsigned char g_74da = 0;
short g_74db = 0;
int g_74dd = 0;
int g_74e1 = 0;
int g_74e5 = 0;
unsigned char g_74e9[16] = {0};
int g_74f9 = 0;
unsigned char g_74fd[22] = {0};
unsigned char g_7513 = 0;
short g_7514 = 0;
int g_7516 = 0;
int g_751a = 0;
int g_751e = 0;
unsigned char g_7522[16] = {0};
int g_7532 = 0;
unsigned char g_7536[22] = {0};
unsigned char g_754c = 0;
short g_754d = 0;
int g_754f = 0;
int g_7553 = 0;
int g_7557 = 0;
unsigned char g_755b[16] = {0};
int g_756b = 0;
unsigned char g_756f[22] = {0};
unsigned char g_7585 = 0;
short g_7586 = 0;
int g_7588 = 0;
int g_758c = 0;
int g_7590 = 0;
unsigned char g_7594[16] = {0};
int g_75a4 = 0;
unsigned int g_75a8 = 8;
unsigned int g_75ac = 0x70;

int f_d4ba(void)
{
    unsigned char v_10[12];
    unsigned char v_2c[28];
    memset((void *)v_10, 0, 0xc);
    *(short *)v_2c = 0x1687;
    int386x(0x2f, (void *)v_2c, (void *)v_2c, (void *)v_10);
    if (*(short *)v_2c != 0) goto L_d5b5;
    g_74b6 = ((((*(unsigned short *)(v_2c + 12) % 0x3e8) / 0x64) << 8) | (((*(unsigned short *)(v_2c + 12) % 0x64) / 0xa) << 4)) | (*(unsigned short *)(v_2c + 12) % 0xa);
    g_e30c = *(unsigned short *)(v_2c + 16);
    g_e310 = *(unsigned short *)(v_2c + 20);
    g_e308 = *(unsigned short *)v_10;
    *(short *)v_2c = 0x400;
    int386x(0x31, (void *)v_2c, (void *)v_2c, (void *)v_10);
    *(int *)&g_75a8 = *(unsigned char *)(v_2c + 13);
    *(int *)&g_75ac = *(unsigned char *)(v_2c + 12);
    g_74b4 = -1;
    return g_74b4;
L_d5b5:;
    g_74b4 = 0;
    return g_74b4;
}

int f_d5d0(void) {
    union REGS regs;
    struct SREGS sregs;
    if (*(unsigned int *)0x19c != 0) {
        memset(&sregs, 0, 12);
        regs.w.ax = 0xde00;
        int386x(0x67, &regs, &regs, &sregs);
        if (regs.h.ah == 0) {
            g_74be = regs.w.bx;
            return g_74bc = -1;
        }
    }
    return g_74bc = 0;
}

void f_d656(unsigned char *p, int release) {
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    if (*(short *)(p + 2) == -1)
        return;
    if (p[0x19] & 2) {
        regs.x.eax = 0x204;
        regs.h.bl = p[0x16];
        int386x(0x31, &regs, &regs, &sregs);
        *(unsigned short *)(p + 0x0a) = regs.x.ecx;
        *(unsigned int *)(p + 6) = regs.x.edx;
    }
    if (p[0x19] & 4) {
        regs.x.eax = p[0x16];
        regs.h.ah = 0x35;
        int386x(0x21, &regs, &regs, &sregs);
        *(unsigned short *)(p + 0x10) = sregs.es;
        *(unsigned int *)(p + 0x0c) = regs.x.ebx;
    }
    if ((*(unsigned int *)(p + 0x19) & 1) == 1) {
        regs.x.eax = 0x200;
        regs.h.bl = p[0x16];
        int386(0x31, &regs, &regs);
        *(unsigned short *)(p + 0x14) = regs.w.cx;
        *(unsigned short *)(p + 0x12) = regs.w.dx;
    }
    if (p[0x17] < 0x10)
        p[0x18] = inp(0x21);
    else if (p[0x17] < 0x18)
        p[0x18] = inp(0xa1);
    if (*(short *)(p + 4) != -1) {
        if (release)
            atexit((void (*)(void))release);
        *(short *)(p + 4) = -1;
    }
    *(short *)(p + 2) = -1;
}

void f_d7b8(unsigned char * a0)
{
    unsigned char v_c[12];
    unsigned char v_28[28];
    memset((void *)v_c, 0, 0xc);
    if (*(short *)(a0 + 2) != -1) goto L_d9fc;
    if (*(unsigned char *)(a0 + 0x17) >= 0x10) goto L_d817;
    outp(0x21, inp(0x21) | (1 << (*(unsigned char *)(a0 + 0x17) - 8)));
    goto L_d84e;
L_d817:;
    if (*(unsigned char *)(a0 + 0x17) >= 0x18) goto L_d84e;
    outp(0xa1, inp(0xa1) | (1 << (*(unsigned char *)(a0 + 0x17) - 0x10)));
L_d84e:;
    if ((*(unsigned char *)(a0 + 0x19) & 2) == 0) goto L_d891;
    *(int *)v_28 = 0x205;
    *(unsigned char *)(v_28 + 4) = *(unsigned char *)(a0 + 0x16);
    *(short *)(v_28 + 8) = *(short *)(a0 + 0xa);
    *(int *)(v_28 + 12) = *(int *)(a0 + 6);
    int386x(0x31, (void *)v_28, (void *)v_28, (void *)v_c);
L_d891:;
    if ((*(unsigned char *)(a0 + 0x19) & 4) == 0) goto L_d8d2;
    *(int *)v_28 = *(unsigned char *)(a0 + 0x16);
    *(unsigned char *)(v_28 + 1) = 0x25;
    *(short *)(v_c + 6) = *(short *)(a0 + 0x10);
    *(int *)(v_28 + 12) = *(int *)(a0 + 0xc);
    int386x(0x21, (void *)v_28, (void *)v_28, (void *)v_c);
L_d8d2:;
    if ((*(int *)(a0 + 0x19) & 1) != 1) goto L_d916;
    *(int *)v_28 = 0x201;
    *(unsigned char *)(v_28 + 4) = *(unsigned char *)(a0 + 0x16);
    *(int *)(v_28 + 8) = *(unsigned short *)(a0 + 0x14);
    *(int *)(v_28 + 12) = *(unsigned short *)(a0 + 0x12);
    int386(0x31, (void *)v_28, (void *)v_28);
L_d916:;
    if (*(unsigned char *)(a0 + 0x17) >= 0x10) goto L_d96d;
    outp(0x21, (inp(0x21) & ~(1 << (*(unsigned char *)(a0 + 0x17) - 8))) | (*(unsigned char *)(a0 + 0x18) & (1 << (*(unsigned char *)(a0 + 0x17) - 8))));
    goto L_d9c8;
L_d96d:;
    if (*(unsigned char *)(a0 + 0x17) >= 0x18) goto L_d9c8;
    outp(0xa1, (inp(0xa1) & ~(1 << (*(unsigned char *)(a0 + 0x17) - 0x10))) | (*(unsigned char *)(a0 + 0x18) & (1 << (*(unsigned char *)(a0 + 0x17) - 0x10))));
L_d9c8:;
    f_df49(*(int *)(a0 + 0x31));
    *(int *)(a0 + 0x31) = 0;
    if (*(short *)a0 != -1) goto L_d9f3;
    *(short *)a0 = 1;
L_d9f3:;
    *(short *)(a0 + 2) = 1;
L_d9fc:;
}

int f_da01(unsigned char *p) {
    int temp;
    union REGS regs;
    struct SREGS sregs;
    memset(&sregs, 0, 12);
    if (*(short *)p != -1) {
    if (p[0x17] < 0x10)
        outp(0x21, inp(0x21) | (1 << (p[0x17] - 8)));
    else if (p[0x17] < 0x18)
        outp(0xa1, inp(0xa1) | (1 << (p[0x17] - 0x10)));
    if (p[0x19] & 2) {
        regs.x.eax = 0x205;
        regs.h.bl = p[0x16];
        regs.w.cx = FP_SEG((void (__far *)(void))(void (__near *)(void))*(unsigned int *)(p + 0x1d));
        regs.x.edx = *(unsigned int *)(p + 0x1d);
        int386x(0x31, &regs, &regs, &sregs);
        *(short *)p = -1;
    }
    if (p[0x19] & 4) {
        regs.x.eax = p[0x16];
        regs.h.ah = 0x25;
        sregs.ds = FP_SEG((void (__far *)(void))(void (__near *)(void))*(unsigned int *)(p + 0x1d));
        regs.x.edx = *(unsigned int *)(p + 0x1d);
        int386x(0x21, &regs, &regs, &sregs);
        *(short *)p = -1;
    }
    if ((*(unsigned int *)(p + 0x19) & 1) == 1) {
        *(unsigned int *)(p + 0x29) = *(unsigned int *)(p + 0x25) - *(unsigned int *)(p + 0x21);
        if (*(unsigned int *)(p + 0x2d) = f_dea6(*(int *)(p + 0x29) + 0x10)) {
            *(unsigned int *)(p + 0x31) = g_e31c;
            ((struct DPMIMapRecord *)p)->mapped =
                ((struct DPMIMapRecord *)p)->allocation + 0xd;
            f_13889(*(unsigned int *)(p + 0x21), *(unsigned int *)(p + 0x2d), *(unsigned int *)(p + 0x29));
            temp = *(int *)(p + 0x2d) + 7;
            *(unsigned short *)temp = *(int *)(p + 0x2d) >> 4;
            temp = *(int *)(p + 0x2d) + *(int *)(p + 0x29) - 5;
            *(unsigned short *)temp = *(unsigned short *)(p + 0x12);
            *(unsigned short *)(temp + 2) = *(unsigned short *)(p + 0x14);
            regs.x.eax = 0x201;
            regs.h.bl = p[0x16];
            regs.x.ecx = (*(int *)(p + 0x2d) >> 4) & 0xffff;
            regs.x.edx = *(int *)(p + 0x2d) & 0xf;
            int386(0x31, &regs, &regs);
        }
        *(short *)p = -1;
    } else {
        *(unsigned int *)(p + 0x31) = 0;
        g_75c4 = 0;
    }
    if (p[0x17] < 0x10)
        outp(0x21, inp(0x21) & ~(1 << (p[0x17] - 8)));
    else if (p[0x17] < 0x18)
        outp(0xa1, inp(0xa1) & ~(1 << (p[0x17] - 0x10)));
        return 0;
    } else {
        return -1;
    }
}
