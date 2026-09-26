extern int g_7404, g_7408;
extern signed short g_7b16;
extern float g_e1fc;
extern int f_dd53(int, int);
extern void f_135e8();
extern void f_133f6();
extern void f_ae56();

void f_ad63(float factor, int x1, int y1, int x2, int y2)
{
    g_e1fc = factor;
    f_135e8(g_7b16, x1, y1, x2, y2, g_7404);
    f_133f6(x1, y1, f_dd53(g_7404 + 1, g_7408 - 1));
    f_133f6(x2, y1, f_dd53(g_7404 + 1, g_7408 - 1));
    f_133f6(x2, y2, f_dd53(g_7404 + 1, g_7408 - 1));
    f_133f6(x1, y2, f_dd53(g_7404 + 1, g_7408 - 1));
    f_ae56(x1, y1, x2, y2);
}
