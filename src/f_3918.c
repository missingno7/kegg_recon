extern short g_7b16;
extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int *g_dddc;
extern unsigned char *g_dee4;
extern unsigned g_dd50;
extern int g_dd78;
extern int g_df24;
extern unsigned char g_8e24;
struct Cell { unsigned char lo : 2, hi : 6; unsigned char flag; };
extern struct Cell *g_dd80;
extern char g_2fec[];

void f_3ba4(int, int);
void f_7e62(int, int, int, int, int, int, int);
void f_42e1(unsigned char, unsigned char, int, int);

void f_3918(int a, int b, int c, int d)
{
    unsigned char saved_7b16;
    int x;
    int y;

    saved_7b16 = (unsigned char)g_7b16;
    x = a * 16 + 16;
    y = b / 18 * 8 + 24;
    g_dddc[5] += 1 << *(int *)(g_dee4 + 0x24);
    g_7b16 = g_7b18;
    f_3ba4(x, y);
    g_7b16 = g_7b20;
    f_3ba4(x, y);
    g_7b16 = g_7b22;
    f_3ba4(x, y);

    if (c == 0 && d == 0) {
        c = -1;
        d = -1;
        if ((g_8e24 & 8) != 0)
            c = -c;
    }

    f_7e62(x, y, c, d, g_df24,
           (int)(g_2fec + (g_dd50 - 1) * 8), 0x12);
    if (g_dd50 >= 0x31 && g_dd50 <= 0x60) {
        if (g_dd80[a].hi) {
            f_42e1(g_dd80[a].hi - 1, g_dd80[a].lo, x + 8, y + 4);
        }
    }
    g_dd80[a].flag = 0;
    if (g_dd50 >= 1 && g_dd50 <= 0x90)
        --g_dd78;
    g_7b16 = saved_7b16;
}
