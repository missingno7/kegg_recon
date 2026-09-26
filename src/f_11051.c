#include <stdio.h>
extern short g_7bfc,g_7498,g_747c,g_7b28;
extern long g_8418;
extern char *g_7c24,*g_7c1c,*g_7c20,*g_7c2c,*g_7c28;
int f_11051(volatile unsigned long flags) { int result=0; if(flags&2) { if(g_7bfc!=-1) { printf(g_7c24); result=-1; } } if((flags&1)==1) { if(g_7498!=-1) { printf(g_7c1c); result=-1; } } if(flags&4) { if(g_8418<0x386) { printf(g_7c20); result=-1; } } if(flags&0x10) { if(g_747c!=-1) { printf(g_7c2c); result=-1; } } if(flags&0x20) { if(g_7b28!=-1) { printf(g_7c28); result=-1; } } return result; }
