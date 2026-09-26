extern int g_75b0, g_75b4;
extern int _rotl(unsigned int, unsigned int);
int f_dd01(void) {
    unsigned int value = g_75b0 * g_75b4++;
    g_75b0 = _rotl(g_75b0, value & 0xff);
    return value & 0xffff;
}
