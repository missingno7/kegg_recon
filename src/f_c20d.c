extern char *g_e270;
extern int g_e26c;
extern int g_7420, g_7430, g_7450, g_7458, g_7460, g_7464, g_745c, g_744c;
extern short g_7db8;
extern int g_7db4;
extern int f_dd53(int, int);
extern void f_c14b(int, int, int, int);
extern void f_c20d(int);
extern void f_c3fb(void);
extern void f_c621(void);
extern void f_1133f(void);
extern void f_11377(int);
extern void *f_ddb9(int);
extern void f_de21(void *);
extern int f_11df8();

void f_c20d(int index)
{
    f_c14b(*(int *)(g_e270 + index * 16 - 16),
           *(int *)(g_e270 + index * 16 - 12),
           *(int *)(g_e270 + index * 16 - 8),
           *(int *)(g_e270 + index * 16 - 4));
}
