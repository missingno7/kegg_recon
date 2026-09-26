extern int outp(int, int);
void f_ecdf(unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
    outp(0x3c8, a);
    outp(0x3c9, b);
    outp(0x3c9, c);
    outp(0x3c9, d);
}
