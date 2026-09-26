extern int g_7404, g_7408, g_740c, g_7414, g_7410;

void f_acdd(float factor, int first, int last)
{
    int half;
    g_7404 = first;
    g_7408 = last;
    g_740c = g_7408 - g_7404 + 1;
    if (factor < 0.0f)
        factor = 0.0f;
    if (factor > 1.0f)
        factor = 1.0f;
    half = g_740c / 2;
    g_7414 = (int)(half * factor);
    g_7410 = -g_7414;
}
