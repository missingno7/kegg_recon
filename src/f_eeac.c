extern short g_e3fc, g_e3fe;
extern short g_7b28;
extern int outp(int, int), inp(int);
extern void f_f59a(void);
int f_eeac(void) {
    unsigned char a;
    volatile unsigned char b;
    volatile int i;
    outp(0x201, 0xff);
    for (i = 0; i < 1000; i++) {
        a = (unsigned char)inp(0x201);
        if (a == 0xff) break;
    }
    for (i = 0; i < 1000; i++) {
        b = (unsigned char)inp(0x201);
        if (b != a) break;
    }
    if (a == b) {
        if (a == 0xff) goto check_last;
    }
    goto changed;
check_last:
    if (b == 0xff) goto no_input;
changed:
    f_f59a();
    if (g_e3fc < 0x20) goto low;
    if (g_e3fe >= 0x20) goto high;
low:
    g_7b28 = 0;
    return g_7b28;
high:
    g_7b28 = -1;
    return g_7b28;
no_input:
    g_7b28 = 0;
    return g_7b28;
}
