#include <i86.h>
#include <conio.h>
extern unsigned char g_e48b,g_e48c,g_e48e;
extern void f_ff0d(void);
void f_fd7b(void) { unsigned char old=g_e48c; int k46,k45,k3a; if (g_e48b!=0x46 && g_e48e==0x46) k46=1; else k46=0; if(k46) g_e48c^=1; if (g_e48b!=0x45 && g_e48e==0x45) k45=1; else k45=0; if(k45) g_e48c^=2; if (g_e48b!=0x3a && g_e48e==0x3a) k3a=1; else k3a=0; if(k3a) g_e48c^=4; if(old!=g_e48c) { _disable(); f_ff0d(); outp(0x60,0xed); f_ff0d(); outp(0x60,g_e48c&7); _enable(); } }
