extern short g_7536;
extern int g_754f, g_756b, g_75c4, g_7b71;
extern void (*g_7553)(void), (*g_7557)(void), (*g_755b)(void);
extern void f_f875(void), f_f7e4(void), f_f6a1(void), f_da01(int *);
extern void __far f_ff5c(void);
extern void __far irq_110(void);
int f_f6f8(int mode) {
    int p;
    if (mode == 0) {
        mode = g_754f;
        if (mode == 0) return 0;
    }
    if (g_7536 != -1) {
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
            g_7b71 = p;
        }
        if (g_75c4) return 0x501;
    }
    return 0;
}
