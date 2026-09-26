#include <i86.h>
extern short g_7bfc;
extern unsigned int g_7c02;
extern int f_1004e(void);
int f_ffbe(void) { unsigned int *p=(unsigned int *)0xcc; union REGS r; if (*p == 0) goto zero; r.w.ax=0; int386(0x33,&r,&r); if (r.w.ax != -1) goto zero; g_7bfc=-1; r.w.ax=0x24; int386(0x33,&r,&r); g_7c02=r.w.bx; f_1004e(); return g_7bfc; zero: g_7bfc=0; return g_7bfc; }
