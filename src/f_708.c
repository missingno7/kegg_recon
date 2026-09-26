extern int g_df38, g_df50, g_df5c, g_dedc;
extern unsigned char *g_dddc;
extern unsigned char g_e13b, g_e13c, g_e13f, g_e140, g_e143, g_e144;
extern void f_10(void), f_19b9(void), f_2250(void), f_240f(void), f_2689(void), f_7e9(void), f_aa2(void), f_e70(void);
void f_708(void) {
    unsigned int index;
    g_e13f = 0;
    g_df38 = -1;
    g_e13c = 4;
    g_e140 = 0;
    g_e144 = 0;
    g_e13b = 0;
    g_df50 = -1;
    f_7e9();
    do {
        f_e70();
        index = (unsigned int)g_df5c - 1;
        switch (index) {
        case 0:
            f_10();
            if (g_e143 >= 60 && *(int *)(g_dddc + 4) >= 0)
                f_2689();
            g_dedc = *(int *)(g_dddc + 0x14);
            f_19b9();
            break;
        case 1:
            g_dedc = 0;
            f_19b9();
            break;
        case 2:
            f_240f();
            break;
        case 3:
            f_aa2();
            break;
        default:
            break;
        }
    } while (g_e13f == 0);
    f_2250();
}
