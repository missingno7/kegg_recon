#pragma pack(1)
typedef struct { unsigned char bytes[25]; } Record;
#pragma pack()
extern int g_387a;
extern char *g_6924;
extern int g_8428;
extern int g_8ddc;
extern int g_8e20;
extern Record g_37f4[];
extern void *g_e4d0;
extern int f_1065b(char *, void *);
extern void f_228e(void);
void f_2188(void)
{
    Record saved[5];
    for (g_8e20 = 0; g_8e20 < 5; ++g_8e20)
        saved[g_8e20] = g_37f4[g_8e20];
    f_228e();
    g_8428 = f_1065b(g_6924, g_e4d0);
    if (g_8428 == 0) {
        f_228e();
        if (g_387a == g_8ddc)
            return;
    }
    for (g_8e20 = 0; g_8e20 < 5; ++g_8e20)
        g_37f4[g_8e20] = saved[g_8e20];
}
