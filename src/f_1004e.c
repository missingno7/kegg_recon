#include <stdlib.h>
extern short g_7c06, g_e4a8, g_e4c2, g_e4a6, g_e494, g_e492, g_e490;
extern void f_10369(void);
extern void f_100ab(void);
void f_1004e(void) { if (g_7c06 != -1) { f_10369(); g_e494=g_e4a8; g_e492=g_e4c2; g_e490=g_e4a6; atexit(f_100ab); g_7c06=-1; } }
