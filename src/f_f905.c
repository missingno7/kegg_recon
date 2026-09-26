extern unsigned char g_7b79[], g_e488, g_e48a;
extern unsigned short g_e468[], g_e478[];
void f_f905(unsigned char key) {
    if ((key & 0x6f) > 0x60) return;
    g_e48a = key;
    key &= 0x7f;
    g_e488 = g_7b79[key];
    key = (unsigned char)((int)key >> 4);
    if (g_e48a & 0x80) {
        g_e468[key] = g_e468[key] & (unsigned short)~(1 << (g_e48a & 0x0f));
        key = (unsigned char)((int)g_e488 >> 4);
        g_e478[key] = g_e478[key] & (unsigned short)~(1 << (g_e488 & 0x0f));
        g_e488 |= g_e48a & 0x80;
    } else {
        g_e468[key] = g_e468[key] | (unsigned short)(1 << (g_e48a & 0x0f));
        key = (unsigned char)((int)g_e488 >> 4);
        g_e478[key] = g_e478[key] | (unsigned short)(1 << (g_e488 & 0x0f));
    }
}
