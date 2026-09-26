extern short g_7b10;
extern int outp(int, int), inp(int);
void f_ebcd(unsigned char *p) {
    int i;
    int j;
    for (i = 0; i < 0x100; i++) {
        outp(0x3c7, i);
        for (j = 0; j < 3; j++) *p++ = (unsigned char)inp(0x3c9);
    }
    g_7b10 = -1;
}
