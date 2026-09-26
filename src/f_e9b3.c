extern void outpw(int, int);
void f_e9b3(int value) {
    outpw(0x3d4, ((value >> 3) << 8) | 0x13);
}
