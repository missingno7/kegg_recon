extern int *g_dddc;
extern int g_ddb8;
void f_c20d(int);
void f_45a6(void)
{
    f_c20d(0x3c);
    g_dddc[1] += g_ddb8 * 0x71;
}
