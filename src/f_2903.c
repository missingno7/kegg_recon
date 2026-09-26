extern short g_7b18;
extern short g_7b20;
extern short g_7b22;
extern int g_7c08;
extern unsigned char *g_dd40;
extern int g_dd44;
extern char *g_dd48;
extern int g_e35a;
extern int g_e4c8;
extern int g_e4d0;
extern int g_6974;
extern int g_6978;
extern int g_697c;
extern int g_6980;

void f_c3ab(void);
void f_e095(int);
void f_ea9f(unsigned char *, int, int, int);
void f_a574(int);
void f_1085a(int);
void f_c14b(int, int, int, int);
void f_a810(char *, int);
void f_12f9c(int, int, int, int, int);
void f_ecdf(unsigned char, unsigned char, unsigned char, unsigned char);
void f_9d40(unsigned char);
void f_13a48(void *, int, int, int);

void f_2903(void)
{
    int i;
    int unused1;
    int unused2;
    char *old1;
    char *old2;

    f_c3ab();
    f_e095(1);
    g_e4d0 = g_7c08;
    f_ea9f(g_dd40, 0, 0x3f, 8);
    g_dd48 = (char *)g_e4d0;
    g_dd40 = g_dd48 + g_e35a;
    f_a574(g_6978);
    old1 = g_e4d0;
    f_a574(g_697c);
    old2 = g_e4d0;
    f_a574(g_6980);
    g_dd44 = g_e4d0;
    f_1085a(g_6974);
    f_c14b(g_dd44, g_e4c8, 0x2ae4, -1);
    f_a810(g_dd48, g_7b18);
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);

    for (i = 0; i < 0x40; i++)
        f_ecdf(1, (unsigned char)i, (unsigned char)i, (unsigned char)i);
    for (i = 0; i < 0x15e; i++)
        f_9d40(1);
    for (i = 0; i < 0x80; i++)
        f_13a48(g_dd40, 0x80, 0x80, 0x3f - i / 2);
    f_ea9f(g_dd40, 0, -0x3f, -1);

    g_dd48 = old1;
    g_dd40 = g_dd48 + g_e35a;
    f_a810(g_dd48, g_7b18);
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    f_ea9f(g_dd40, -0x3f, 0, 1);
    for (i = 0; i < 0x8c; i++)
        f_9d40(1);
    f_ea9f(g_dd40, 0, -0x3f, -3);

    g_dd48 = old2;
    g_dd40 = g_dd48 + g_e35a;
    f_a810(g_dd48, g_7b18);
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    f_ea9f(g_dd40, -0x3f, 0, 1);
    for (i = 0; i < 0x8c; i++)
        f_9d40(1);
    f_ea9f(g_dd40, 0, -0x3f, -3);
}
