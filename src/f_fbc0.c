extern unsigned char g_e46e, g_e469;
extern short g_7b39;
extern void (*g_7b3f)(void), (*g_7b43)(void);
extern void f_fa42(void);
void f_fbc0(void) { while ((g_e46e & 0x40) && (g_e469 & 0x40)) { f_fa42(); if (g_7b39 == 0) { g_7b3f(); g_7b39 = -1; } } if (g_7b39 != 0) { g_7b43(); g_7b39 = 0; } }
