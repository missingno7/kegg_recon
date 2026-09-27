/* conio.h - Watcom port I/O and console input, routed to the virtual PC.
 * Return types are int (units redeclare `extern int inp(int);`), outpw is left to
 * the units that declare it (they disagree on its return type; the port defines it
 * with an unsigned result, which is ABI compatible with a void declaration). */
#ifndef KE_CONIO_H
#define KE_CONIO_H
int inp(int port);
int inpw(int port);
int outp(int port, int value);
int kbhit(void);
int getch(void);
int getche(void);
#endif
