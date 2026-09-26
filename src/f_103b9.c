#include <i86.h>
extern short g_7bfc, g_e4a8, g_e4c2, g_e4a6;
void f_103b9(void) { if (g_7bfc == -1) { union REGS r; r.w.bx=g_e4a8; r.w.cx=g_e4c2; r.w.dx=g_e4a6; r.w.ax=0x1a; int386(0x33,&r,&r); } }
