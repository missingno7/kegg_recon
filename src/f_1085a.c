extern unsigned int g_7c14,g_e4c8,g_e4d0;
extern int f_1065b(char *,void *);
int f_1085a(char * name) { int result; --g_7c14; result=f_1065b(name,(void *)g_e4d0); g_e4d0+=g_e4c8; g_e4d0=(g_e4d0+3)&0xfffffffcU; return result; }
