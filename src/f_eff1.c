extern int g_7b2a, g_7b2e;
extern short g_e3fc, g_e3fe, g_e418, g_e41a;
extern int inp(int);
extern void f_13f9f(void), f_13fa1(void);
void f_eff1(void) {
    unsigned char key;
    unsigned char mask;
    int count;
    mask = 0x0f;
    count = 0;
    f_13f9f();
    do {
        key = (unsigned char)(inp(0x201) & 0x0f);
        if (key != mask) {
            key ^= mask;
            if ((key & 1) == 1) { mask ^= 1; g_e3fc = count; }
            else if (key & 2) { mask ^= 2; g_e3fe = count; }
            else if (key & 4) { mask ^= 4; g_e418 = count; }
            else if (key & 8) { mask ^= 8; g_e41a = count; }
        }
        ++count;
        if (count > g_7b2e) {
            if (g_7b2a != 0) goto loop_exit;
            if ((key & 1) == 1) g_e3fc = count;
            else if (key & 2) g_e3fe = count;
            else if (key & 4) g_e418 = count;
            else if (key & 8) g_e41a = count;
            mask = 0;
        }
    } while (mask != 0);
loop_exit:
    f_13fa1();
}
