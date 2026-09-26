extern short g_7b6f;
extern unsigned char g_e48c;
extern void f_14197(void (*)(void)), f_f82d(void);
void f_f7e4(void) {
    int p;
    p = 0x417;
    if (g_7b6f != -1) {
        g_e48c = *(unsigned char *)p >> 4;
        f_14197(f_f82d);
        g_7b6f = -1;
    }
}
