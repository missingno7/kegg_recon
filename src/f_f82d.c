extern short g_7b6f;
extern unsigned char g_e48c;
void f_f82d(void) {
    unsigned char *p;
    p = (unsigned char *)0x417;
    if (g_7b6f == -1) {
        *p = (*p & 0x8f) | ((g_e48c << 4) & 0x70);
        g_7b6f = 1;
    }
}
