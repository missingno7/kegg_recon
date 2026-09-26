extern int g_e36e, g_e372, g_e376, g_e37a, g_e366, g_e36a;
void f_e88d(int left, int top, int right, int bottom) {
    int t;
    if (left > right) { t = left; left = right; right = t; }
    if (top > bottom) { t = top; top = bottom; bottom = t; }
    g_e36e = left;
    g_e376 = right;
    g_e372 = top;
    g_e37a = bottom;
    g_e36a = g_e37a - g_e372 + 1;
    g_e366 = g_e376 - g_e36e + 1;
}
