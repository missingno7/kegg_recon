extern int g_df30;
extern int g_e154;
extern int g_e148;
extern unsigned char g_e143;
extern int g_e150;
extern struct digit_entry { int value; int other; } g_557c[10];

void f_82cc(void);

void f_34b7(void)
{
    g_e154 = g_df30;
    g_e148 = 6;
    f_82cc();
    *(int *)(g_e150 + 0x44) = g_557c[(((int)g_e143 + 1) / 10) % 10].value;
    *(int *)(g_e150 + 0x74) = g_557c[((int)g_e143 + 1) % 10].value;
}
