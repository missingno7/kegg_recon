extern void outpw(int, int);
void f_ee65(int value) {
    outpw(0x3d4, ((value >> 2) & 0xff00) | 0x0c);
    outpw(0x3d4, ((value << 6) & 0xff00) | 0x0d);
}
