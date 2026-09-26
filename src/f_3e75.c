extern unsigned char g_e48e;
extern unsigned char g_e48f;
extern unsigned char g_e143;
extern int g_dd78;
extern int g_ddc0;
extern unsigned char *g_de5c;

void f_42e1(unsigned char, unsigned char, int, int);
void f_58b0(void);

void f_3e75(void)
{
    if (g_e48f == 0x2e && (g_e48e & 0x80) == 0)
        g_e48f = 0;
    else
        goto key_2a;
key_2a:
    if (g_e48f == 0x2a && (g_e48e & 0x80) == 0) {
        g_e143 = (g_e143 / 10) * 10 + 9;
        g_dd78 = 0;
        g_ddc0 = 0;
        g_e48f = 0;
    } else goto key_2f;
key_2f:
    if (g_e48f == 0x2f && (g_e48e & 0x80) == 0) {
        g_dd78 = 0;
        g_ddc0 = 0;
        g_e48f = 0;
    } else goto key_alpha;
key_alpha:
    if (g_e48f >= 0x41 && g_e48f <= 0x5a && (g_e48e & 0x80) == 0) {
        f_42e1((unsigned char)(g_e48f - 0x41), 0, 0xa0, 0x64);
        g_e48f = 0;
    } else goto key_slash;
key_slash:
    if (g_e48f == 0x2f && (g_e48e & 0x80) == 0) {
        f_42e1(0x1a, 0, 0xa0, 0x64);
        g_e48f = 0;
    } else goto key_37;
key_37:
    if (g_e48f == 0x37 && (g_e48e & 0x80) == 0) {
        f_42e1(6, 0, 0xa0, 0x64);
        g_e48f = 0;
    } else goto key_39;
key_39:
    if (g_e48f == 0x39 && (g_e48e & 0x80) == 0) {
        f_42e1(0xf, 0, 0xa0, 0x64);
        g_e48f = 0;
    } else goto key_dot;
key_dot:
    if (g_e48f == 0x2e && (g_e48e & 0x80) == 0) {
        g_de5c[1] &= 0xfb;
        f_58b0();
        g_de5c[0] &= 0xbf;
        g_de5c[0] &= 0x7f;
        g_e48f = 0;
    }
key_g:
    if (g_e48f == 0x47) {
        g_e48f = 0;
        g_dd78 = 0;
    }
    if (g_e48e == 0x1b) {
        g_e143 = 0x3c;
        g_dd78 = 0;
    }
}
