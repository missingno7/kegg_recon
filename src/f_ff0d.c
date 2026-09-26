#include <conio.h>
int f_ff0d(void) { int n=0x1388; poll: if ((inp(0x64)&2)==0) goto poll_exit; if (n>0) goto decrement; poll_exit: goto finish; decrement: n--; goto poll; finish: if(n>0) return 0; return -1; }
