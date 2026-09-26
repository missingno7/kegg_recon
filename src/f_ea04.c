extern int f_13964(unsigned char, unsigned char, unsigned char);
void f_ea04(int value) {
    f_13964(0x18, 0, (unsigned char)value);
    f_13964(7, 0xef, (value & 0x100) >> 4);
    f_13964(9, 0xbf, (value & 0x200) >> 3);
}
