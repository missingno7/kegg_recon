extern short g_7536;
extern int g_7b71;
extern unsigned char g_7b79[], g_e488, g_e489, g_e48a, g_e48b, g_e48d, g_e48e, g_e48f;
extern int f_1477c(void), f_14793(void);
extern void f_f8bb(void), f_f905(unsigned char);
void f_fa42(void) {
    if (g_7536 != -1) {
        f_f8bb();
        if (f_1477c()) {
            g_e488 = (unsigned char)f_14793();
            for (g_e48a = 0; g_e48a < 0x7f; ++g_e48a) {
                if (g_e488 == g_7b79[g_e48a]) break;
            }
        }
    } else if (*(unsigned char *)g_7b71) {
        f_f905(*(unsigned char *)g_7b71);
        *(unsigned char *)g_7b71 = 0;
    }
    if (g_e489 == g_e48e) goto update;
    if (g_e48e != g_e48a) goto update;
    goto finish;
update:
        g_e48d = g_e48f;
        g_e48b = g_e48e;
finish:
    g_e48e = g_e48a;
    g_e48f = g_e488;
    g_e489 = g_e48e;
}
