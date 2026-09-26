#include <stdio.h>
extern char g_1268[];
extern unsigned int g_e4d4,g_e4d8;
int f_107b6(char * name,void * buf,unsigned int length) { FILE * fp; int result; g_e4d4=(unsigned int)name; fp=fopen(name,g_1268); if(fp==0) result=0x201; else { if(fseek(fp,0,0)!=0) result=0x205; else { g_e4d8=(unsigned int)fwrite(buf,1,length,fp); if(g_e4d8!=length) result=0x204; else result=0; } fclose(fp); } return result; }
