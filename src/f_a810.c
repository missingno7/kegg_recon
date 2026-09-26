extern short g_e324;
extern int g_e326[3][4];
extern int g_e35a;
extern void a_12f30(int, int, int);
extern void f_13889(int, int, int);

void f_a810(int a0, int a1)
{
    if (g_e324 == 1) {
        a_12f30(a0, (g_e326[0][a1] + g_e326[1][a1] + g_e326[2][a1]) >> 2, g_e35a);
    } else {
        f_13889(a0, g_e326[0][a1] + g_e326[1][a1] + g_e326[2][a1], g_e35a);
    }
}
