extern unsigned char g_e46b, g_e46f, g_e472;
extern void (*g_7b47)(int, int);
void f_fd3b(void) {
    if ((g_e46b & 0x20) && (g_e46f & 1) && (g_e472 & 8)) g_7b47(0x101, 0);
}
