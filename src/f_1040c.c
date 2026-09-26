#include <i86.h>
extern short g_7bfc;
void f_1040c(int a,int b) { if (g_7bfc == -1) { union REGS r; int t; if (a>b) { t=a; a=b; b=t; } if (a<0) a=0; if (b<0) b=0; r.w.ax=7; r.w.cx=a*2; r.w.dx=b*2; int386(0x33,&r,&r); } }
