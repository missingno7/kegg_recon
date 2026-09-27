/* i86.h - Watcom 386 register/interrupt interface, layout compatible with Watcom 10.0 H/i86.h
 * (flat 386, not __WINDOWS_386__). int386/int386x are dispatched to the virtual PC
 * (port/vhw/intsvc.c) instead of executing INT n. */
#ifndef KE_I86_H
#define KE_I86_H

#pragma pack(push, 1)
struct DWORDREGS {
    unsigned int eax, ebx, ecx, edx, esi, edi;
    unsigned int cflag;
};
struct WORDREGS {
    unsigned short ax, _1, bx, _2, cx, _3, dx, _4, si, _5, di, _6;
    unsigned int cflag;
};
struct BYTEREGS {
    unsigned char al, ah; unsigned short _1;
    unsigned char bl, bh; unsigned short _2;
    unsigned char cl, ch; unsigned short _3;
    unsigned char dl, dh; unsigned short _4;
};
union REGS {
    struct DWORDREGS x;
    struct WORDREGS w;
    struct BYTEREGS h;
};
#define _REGS REGS
struct SREGS {
    unsigned short es, cs, ss, ds, fs, gs;
};
#define _SREGS SREGS
#pragma pack(pop)

/* Declared with void* parameters: some units pass a private register-block struct
 * (e.g. BiosVideoModeRequest) whose layout matches REGS, as the original did. */
int int386(int intno, void *in, void *out);
int int386x(int intno, void *in, void *out, void *sregs);

/* Interrupt flag (IF) of the virtual CPU: see port/vhw/pic.c. Real functions (not
 * macros) because some units redeclare them as `extern void _disable(void);`. */
void _disable(void);
void _enable(void);

/* Every far pointer is flat; the selector reported is the virtual flat selector. */
#define KE_FLAT_SELECTOR 0x0170u
#define FP_SEG(p) ((unsigned short)KE_FLAT_SELECTOR)
#define FP_OFF(p) ((unsigned int)(p))
#define MK_FP(s, o) ((void *)(o))

void segread(struct SREGS *);
void delay(unsigned int milliseconds);
void sound(unsigned int hz);
void nosound(void);

#endif
