extern int inp(int), g_7b71;
extern void f_f905(unsigned char);
void f_f8bb(void) {
    f_f905((unsigned char)inp(0x60));
    if (*(unsigned char *)g_7b71) {
        f_f905(*(unsigned char *)g_7b71);
        *(unsigned char *)g_7b71 = 0;
    }
}
