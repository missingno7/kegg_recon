extern short g_7b18;
extern short g_7b16;
extern short g_7b20;
extern short g_7b22;
extern short g_7b34;
extern short g_7b3d;
extern short g_7bfe;
extern short g_7c00;
extern short g_746c;
extern unsigned char g_e48e;
extern int g_e14c;
extern int g_e35a;
extern char g_d340[];
extern char g_ab40[];
extern char g_bf40[];

void f_339f(void);
void f_c8c0(char *, int, unsigned char, char *, char *);
void f_10137(void);
void f_8345(void);
void f_83b1(void);
void f_c9ce(int, int);
void f_111de(int, int);
void f_ed38(void);
void f_9d40(unsigned char);
void f_13b72(void);
void f_12f9c(int, int, int, int, int);

int f_3203(void)
{
    int result;

    f_339f();
    g_7b34 = ((*(volatile short *)&g_7b34) & 0xfffe) & 0xfffd;
    g_7b3d = 0;
    f_c8c0(g_d340, 0x100, 4, g_ab40, g_bf40);

retry:
        f_10137();
        f_8345();
        f_83b1();
        f_c9ce(0, 0);
        if (g_746c != 0)
            f_111de(g_746c, 0);
        f_ed38();
        f_9d40(1);
        f_13b72();
        if (g_7b3d != 0) goto path_c6;
        if (g_7bfe == g_7c00) goto path_c4;
        if (g_7bfe != 0) goto path_c6;
path_c4:
        goto path_c8;
path_c6:
        goto path_d1;
path_c8:
        if (g_e48e != 1) goto path_d3;
path_d1:
        goto path_dc;
path_d3:
        if (g_e14c < 0x2e) goto path_de;
path_dc:
        goto loop_done;
path_de:
        goto retry;
loop_done:

    if (g_e14c == 0x2e)
        result = -1;
    else
        result = 0;
    g_7b16 = g_7b18;
    g_e14c = 0x2e;
    f_8345();
    f_83b1();
    f_c9ce(0, 0);
    f_12f9c(g_7b18, 0, g_7b20, 0, g_e35a);
    f_12f9c(g_7b18, 0, g_7b22, 0, g_e35a);
    f_c8c0(g_d340, 0x100, 4, g_ab40, g_bf40);
    return result;
}
