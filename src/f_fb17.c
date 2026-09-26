extern unsigned short g_7b34;
extern void f_fe6a(void), f_fbc0(void), f_fc1e(void), f_fb92(void), f_fd3b(void), f_fd7b(void), f_fcab(void);
void f_fb17(void) {
    if (g_7b34 & 0x80) f_fe6a();
    if ((g_7b34 & 1) == 1) f_fbc0();
    if (g_7b34 & 2) f_fc1e();
    if (g_7b34 & 4) f_fb92();
    if (g_7b34 & 0x10) f_fd3b();
    if (g_7b34 & 0x20) f_fd7b();
    if (g_7b34 & 0x40) f_fcab();
}
