extern int g_7b2e;
extern short g_e400, g_e402, g_e404, g_e406;
extern short g_e408, g_e40a, g_e40c, g_e40e, g_e410, g_e412;
extern short g_e41c, g_e41e, g_e420, g_e422;
extern short g_e424, g_e426, g_e428, g_e42a, g_e42c, g_e42e;
void f_f46c(void) {
    g_e400 = (g_e408 + g_e40a) >> 1;
    g_e41c = (g_e424 + g_e426) >> 1;
    g_e402 = (g_e40a + g_e40c) >> 1;
    g_e41e = (g_e426 + g_e428) >> 1;
    g_e404 = (g_e40e + g_e410) >> 1;
    g_e420 = (g_e42a + g_e42c) >> 1;
    g_e406 = (g_e410 + g_e412) >> 1;
    g_e422 = (g_e42c + g_e42e) >> 1;
    g_7b2e = g_e402;
    if (g_e406 > g_7b2e) g_7b2e = g_e406;
    if (g_e41e > g_7b2e) g_7b2e = g_e41e;
    if (g_e422 > g_7b2e) g_7b2e = g_e422;
}
