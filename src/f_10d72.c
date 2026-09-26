#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
extern void f_11146(void),f_111de(unsigned long,unsigned long),f_fb17(void),f_fa42(void),f_10137(void),f_efa0(void);
extern int f_ee65(int),f_f6f8(int),f_9b44(int),f_e095(int);
extern void f_9d40(unsigned char);
extern void f_11209(int),f_100fa(int,int,int,int),f_10502(int,int);
extern short g_7b14,g_7b39,g_73a6;
extern void (*g_7b47)(unsigned long,unsigned long);
extern short *g_73b4;
extern void (*g_73b8)(void),(*g_73bc)(void),(*g_73c0)(void),(*g_73c4)(void);
extern char *g_7cb0,*g_7cac,*g_7ca8;
extern char **g_7d84[];
extern unsigned long g_754f,g_7588;
extern long g_e36e,g_e372,g_e376,g_e37a;
extern unsigned char g_e482;
extern char g_240e[],g_2416[],g_241e[],g_2426[];
void f_10d72(int a,int b,int c,volatile int d) { volatile int x; int y; volatile int i; atexit(f_11146); g_7b47=f_111de; g_73b4=&g_7b39; g_73b8=f_fb17; g_73bc=f_fa42; g_73c0=f_10137; g_73c4=f_efa0; f_ee65(g_7b14); outp(0x21,inp(0x21)|1); if(c==-1) printf(g_7cb0); if(b==-1) { if(c==-1) x=2; else x=4; y=f_f6f8(x); if(y) printf(g_7d84[(y&0xff00)>>8][(y&0xff)-1]); if(g_754f) { printf(g_7cac); if((g_754f&1)==1) printf(g_240e); if(g_754f&2) printf(g_2416); if(g_754f&4) printf(g_241e); printf(g_2426); } } if(a==-1 && g_73a6==-1) { if(c==-1) x=2; else x=4; y=f_9b44(x); if(y) printf(g_7d84[(y&0xff00)>>8][(y&0xff)-1]); if(g_7588) { printf(g_7ca8); if((g_7588&1)==1) printf(g_240e); if(g_7588&2) printf(g_2416); if(g_7588&4) printf(g_241e); printf(g_2426); } } for(i=0;i<50;i++) { f_9d40((volatile int)1); if(g_e482&0x80) i--; } if(d!=-1) { f_e095(d); f_100fa(g_e36e,g_e372,g_e376,g_e37a); f_10502((g_e36e+g_e376)/2,(g_e372+g_e37a)/2); } }
