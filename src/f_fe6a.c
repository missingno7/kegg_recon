extern unsigned char g_e48b,g_e48e,g_e48f,g_7b36;
extern unsigned int g_7b4b[],g_7b75;
void f_fe6a(void) { volatile int i; if (g_e48e != g_e48b && g_e48e < 0x7f) { if (g_e48e == 0x1c) { for (i=0;(short)i<9;i++) { if (g_7b4b[(short)i] == g_7b75) { if ((short)i==8) g_7b36 ^= 0xff; else g_7b36 ^= 1<<(short)i; } } g_7b75=0; } else g_7b75 += (unsigned short)g_e48f + 0x10000U; } }
