#include <i86.h>
extern short g_7bfc, g_e4a8, g_e4c2, g_e4a6;
void f_10369(void) { if (g_7bfc == -1) { union REGS r; r.w.ax=0x1b; int386(0x33,&r,&r); g_e4a8=r.x.ebx; g_e4c2=r.x.ecx; g_e4a6=r.x.edx; } }
