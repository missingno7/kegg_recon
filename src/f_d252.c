extern void f_13a88(void), f_11485(void), f_113bd(void);
extern int g_e2f4;
void __interrupt f_d252(void) {
    f_13a88();
    f_11485();
    f_113bd();
    g_e2f4 = 0;
}
