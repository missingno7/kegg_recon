extern void f_ecdf(unsigned char, unsigned char, unsigned char, unsigned char);
void f_ec9c(void) {
    int i;
    for (i = 0; i < 0x100; i++) f_ecdf((unsigned char)i, 0, 0, 0);
}
