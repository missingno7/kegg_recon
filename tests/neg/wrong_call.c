/* calls bound to the wrong function name */
extern int g_8ddc, g_387a, g_e4d0, g_6924;
void f_228f(void);
void f_107b6(int a, int b, int c);
void f_2250(void) { f_228f(); f_228f(); g_387a = g_8ddc; f_107b6(g_6924, g_e4d0, 150); }
