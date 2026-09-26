#include <stdio.h>
extern char g_1264[];
extern unsigned int g_e4cc,g_e4d4;
int f_105c2(char * volatile name) { FILE * volatile fp; int result; g_e4d4=(unsigned int)name; fp=fopen(name,g_1264); if(fp==0) result=0x201; else { if(fseek(fp,0,2)!=0) result=0x205; else { g_e4cc=(unsigned int)ftell(fp); if(g_e4cc==0xffffffffU) result=0x206; else result=0; } fclose(fp); } return result; }
