#include <i86.h>
extern short g_7bfc, g_e4ae, g_e4b8, g_e4ba, g_e4c6, g_e4b0, g_e4b4, g_e4bc, g_e4c4;
void f_10502(int a,int b) { if (g_7bfc == -1) { union REGS r; r.w.ax=4; r.w.cx=a*2; r.w.dx=b*2; int386(0x33,&r,&r); g_e4ae=a; g_e4b8=g_e4ae; g_e4ba=g_e4b8; g_e4c6=g_e4ba; g_e4b0=b; g_e4b4=g_e4b0; g_e4bc=g_e4b4; g_e4c4=g_e4bc; } }
