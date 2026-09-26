extern short g_746e;
extern short g_7b16;
extern short g_7b18;
extern unsigned char g_e142;
extern unsigned char g_e143;
extern unsigned int g_8e04;
extern int g_ddb0;
extern int g_dd78;
extern unsigned short *g_dd80;
extern unsigned int g_dd50;
extern int g_dd60;
extern int g_dd64;
extern int g_dd68;
extern int g_dd70;
extern int g_df24;
extern int g_df28;
extern int g_e158;
extern int g_6c64[];
extern int g_6c7c[];
extern unsigned char g_a608;
extern unsigned char g_a988[];
extern unsigned char g_6b9e[];
extern unsigned char g_2fe4[];
typedef struct { unsigned char prefix:2; unsigned char value:6; } IndexBits;

void f_13889(int, int, int);
void f_12cbd(int, int, int);

void f_2f9e(void)
{
    int j;
    int i;

    g_746e = 8;
    g_7b16 = g_7b18;
    g_dd80 = (unsigned short *)&g_a608;
    g_dd78 = 0;
    if (g_e142 != 0) {
        for (g_8e04 = 0; g_8e04 < 0x120; ++g_8e04) {
            *((unsigned char *)g_dd80++ + 1) = 0;
        }
        g_ddb0 = g_6c64[g_e142];
        g_dd78 = g_6c7c[g_e142];
    } else {
        g_df28 += g_e143 * 0x24a;
        f_13889(g_df28, (int)&g_ddb0, 2);
        f_13889(g_df28 + 2, (int)g_a988, 8);
        f_13889(g_df28 + 10, (int)&g_a608, 0x240);
        for (i = 0; i < 0x10; i++) {
            for (j = 0; j < 0x12; j++) {
                g_dd50 = ((unsigned char *)g_dd80)[1];
                if (g_dd50 > 0xff) {
                    g_dd50 = 0xff;
                    ((unsigned char *)g_dd80)[1] = (unsigned char)g_dd50;
                }
                if (g_dd50 >= 1 && g_dd50 <= 0x90)
                    ++g_dd78;
                if (g_dd50 >= 0xfa && g_dd50 <= 0x100) {
                    ((IndexBits *)g_dd80)->value = g_6b9e[g_dd50];
                }
                if (g_dd50 == 0xf8) {
                    g_dd60 = j * 16 + 16;
                    g_dd64 = i * 8 + 24;
                }
                if (g_dd50 == 0xf9) {
                    g_dd68 = j * 16 + 16;
                    g_dd70 = i * 8 + 24;
                }
                if (g_dd50 != 0) {
                    g_e158 = g_df24 + *(int *)(g_2fe4 + g_dd50 * 8);
                    f_12cbd(g_e158, j * 16 + 16, i * 8 + 24);
                }
                ++g_dd80;
            }
        }
    }
}
