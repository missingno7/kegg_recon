/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <conio.h>
#include <i86.h>
#include <string.h>
extern void f_df49(unsigned);
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
