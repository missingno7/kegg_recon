extern void f_14771(int, int);
void f_ee65(int value) {
    f_14771(0x3d4, ((value >> 2) & 0xff00) | 0x0c);
    f_14771(0x3d4, ((value << 6) & 0xff00) | 0x0d);
}
