extern int inp(int);
extern unsigned int g_e3c0, g_e3dc, g_e388, g_e3a4, g_e3f8, g_e414;
extern unsigned int g_e44c, g_e430;
extern short g_e3fc, g_e3fe, g_e400, g_e402, g_e404, g_e406;
extern short g_e418, g_e41a, g_e41c, g_e41e, g_e420, g_e422;
void f_f0f6(void) {
    unsigned char buttons;
    buttons = (unsigned char)inp(0x201);
    g_e44c = g_e3c0;
    g_e430 = g_e3dc;
    g_e3c0 = g_e388;
    g_e3dc = g_e3a4;
    g_e388 = g_e3f8;
    g_e3a4 = g_e414;
    g_e414 = 0;
    g_e3f8 = g_e414;
    if (g_e3fc < g_e400) *(unsigned char *)&g_e3f8 |= 1;
    else if (g_e3fc > g_e402) *(unsigned char *)&g_e3f8 |= 2;
    if (g_e3fe < g_e404) *(unsigned char *)&g_e3f8 |= 4;
    else if (g_e3fe > g_e406) *(unsigned char *)&g_e3f8 |= 8;
    if (g_e418 < g_e41c) *(unsigned char *)&g_e414 |= 1;
    else if (g_e418 > g_e41e) *(unsigned char *)&g_e414 |= 2;
    if (g_e41a < g_e420) *(unsigned char *)&g_e414 |= 4;
    else if (g_e41a > g_e422) *(unsigned char *)&g_e414 |= 8;
    if (!(buttons & 0x10)) *(unsigned char *)&g_e3f8 |= 0x10;
    if (!(buttons & 0x20)) *(unsigned char *)&g_e3f8 |= 0x20;
    if (!(buttons & 0x40)) *(unsigned char *)&g_e414 |= 0x10;
    if (!(buttons & 0x80)) *(unsigned char *)&g_e414 |= 0x20;
}
