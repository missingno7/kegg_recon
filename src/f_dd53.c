extern int g_75b0, g_75b4;
extern int _rotl(unsigned int, unsigned int);
int f_dd53(int start, int end) {
    unsigned int value = g_75b0 * g_75b4++;
    g_75b0 = _rotl(g_75b0, value & 0xff);
    return start + ((((value & 0xffff) + 1) * (unsigned int)(end - start + 1)) >> 16);
}

