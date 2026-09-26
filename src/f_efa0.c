extern int outp(int, int);
extern void f_eff1(void), f_f0f6(void);
void f_efa0(void) {
    outp(0x201, 0xff);
    f_eff1();
    outp(0x201, 0xff);
    f_f0f6();
    outp(0x201, 0xff);
}
