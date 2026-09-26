extern int g_e4c8;
extern int g_dd44;
extern int g_df30;
extern short g_7b14;

void f_c3ab(void);
void f_2e4a(void);
void f_c14b(int, int, int, int);
void f_c9f4(int, int, int, int, int);

void f_2c72(void)
{
    f_c3ab();
    f_2e4a();
    f_c14b(g_dd44, g_e4c8, 0x1f40, -1);
    f_c9f4(g_7b14, 8, g_df30 + 0x7102, 0xa0, 100);
}
