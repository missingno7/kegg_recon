extern int *g_dee4;
extern unsigned char *g_de5c;
void f_c20d(int);
void f_5771(void);
void f_4413(void)
{
    int b;
    int a;
    if (g_dee4[10] > 0) a = 1; else a = 0;
    if ((*g_de5c & 8) == 0) b = 1; else b = 0;
    if (b | a) {
        f_c20d(0x27);
        if (--g_dee4[10] < 0)
            f_5771();
    }
}

