#include <stdio.h>
#include <conio.h>
extern int f_137a8(void),f_d288(void),f_ca6a(void),f_d2f0(void),f_d36c(void),f_d408(void),f_d5d0(void),f_d4ba(void),f_df7f(void),f_ddb9(int),f_de21(int),f_cb3f(void),f_ffbe(void),f_eeac(void),f_fa42(void),f_9974(void),f_9f64(void);
extern unsigned int f_dea6(int),f_de3e(void);
extern void f_df49(unsigned int);
extern void f_11209(int),f_100fa(int,int,int,int),f_10502(int,int);
extern int f_e095(int);
extern unsigned long g_8418,g_841c,g_8420;
extern char *g_7c54[],*g_7c60,*g_7c64,*g_7c68,*g_7c6c,*g_7c70,*g_7c74,*g_7c78,*g_7c7c,*g_7c40,*g_7c44,*g_7c9c,*g_7c84,*g_7c80,*g_7c88,*g_7c8c,*g_7c90,*g_7c94,*g_7c98,*g_7c50,*g_7ca0,*g_7ca4;
extern long g_75c0;
extern unsigned int g_75c4,g_75bc,g_e31c,g_e314,g_7c02,g_7c08,g_7c10,g_7ca8,g_e36e,g_e372,g_e376,g_e37a,g_7cac,g_754f,g_7588,g_7b14;
extern unsigned int g_747e;
extern short g_7536;
extern unsigned char g_7486,g_7487,g_e482,g_e48d,g_e48f;
extern unsigned long g_7492,g_7476,g_749e,g_74a6,g_74ae,g_74be,g_74b6,g_7482;
extern short g_e2fc,g_73a6;
int f_108a9(int a,int b,unsigned long flags,int d) { int result=0; if((flags&1)==1) { f_137a8(); printf(g_7c60,g_8418); printf((char*)g_7c54[g_841c],g_8420); } if((flags&2)!=0) if(f_d288()==-1) { printf(g_7c64); f_11209(g_7492); } if((flags&4)!=0) if(f_ca6a()==-1) { printf(g_7c68); f_11209(g_7476); } if((flags&8)!=0) if(f_d2f0()==-1) printf(g_7c6c,g_749e); if((flags&0x10)!=0) if(f_d36c()==-1) { printf(g_7c70); f_11209(g_74a6); } if((flags&0x20)!=0) if(f_d408()==-1) { printf(g_7c74); f_11209(g_74ae); } if((flags&0x40)!=0) if(f_d5d0()==-1) { printf(g_7c78); f_11209(g_74be); } if((flags&0x80)!=0) if(f_d4ba()==-1) { printf(g_7c7c); f_11209(g_74b6); } if((flags&0x100)!=0) { if(f_dea6(a)==0) { result=-1; printf(g_7c40,a/1024); } if(g_75c4==0) f_df49(g_e31c); printf(g_7c9c,a/1024,f_df7f()/1024); } if((flags&0x200)!=0) { if(f_ddb9(b)==0) { result=-1; printf(g_7c44,b/1024); } if(g_75bc==0) f_de21(g_e314); f_dea6(a); printf(g_7ca0,b/1024,(int)f_de3e()/1024,(g_75c0-a)/1024); if(g_75c4==0) f_df49(g_e31c); } if((flags&0x400)!=0) if(f_ffbe()==-1) { printf(g_7c84); f_11209(g_7c02); } if((flags&0x800)!=0) if(f_eeac()==-1) printf(g_7c80,0x201); if((flags&0x2000)!=0) { if(f_cb3f()==-1) { if(g_747e==0) printf(g_7c88,(int)g_e2fc,g_7486,g_7487); else printf(g_7c8c,(int)g_e2fc,g_7486,g_7487); printf(g_7c90); f_11209(g_7482); } else if(g_e2fc>=0x200 && g_e2fc<0x300) { if(g_7486==0xff) printf(g_7c94,(int)g_e2fc,g_7486,g_7487); else if(g_7487==0xff) printf(g_7c98,(int)g_e2fc,g_7486,g_7487); printf(g_7c50); if(g_7536==-1) { do { f_fa42(); } while (g_e48d==g_e48f || g_e48f!=0x20); } else while(getch()!=0x20) {} } } if(flags&0x1000) if(f_9974()==-1) printf(g_7ca4,f_9f64()); return result; }
