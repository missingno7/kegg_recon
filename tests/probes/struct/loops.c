extern int a, b, c, d, i; extern void f(void); extern void g(void); extern void h(void);
void wh(void) { while (a == 1) f(); g(); }
void whand(void) { while (a == 1 && b == 2) f(); g(); }
void whor(void) { while (a == 1 || b == 2) f(); g(); }
void dw(void) { do f(); while (a == 1); g(); }
void dwand(void) { do f(); while (a == 1 && b == 2); g(); }
void dwor(void) { do f(); while (a == 1 || b == 2); g(); }
void fr(void) { for (i = 0; i < 10; i++) f(); g(); }
void frand(void) { for (i = 0; i < 10 && a == 1; i++) f(); g(); }
void fror(void) { for (i = 0; i < 10 || a == 1; i++) f(); g(); }
void frnocond(void) { for (i = 0; ; i++) { if (a == 1) break; f(); } g(); }
void frnostep(void) { for (i = 0; i < 10; ) f(); g(); }
void forever(void) { for (;;) { if (a == 1) break; f(); } g(); }
void whone(void) { while (1) { if (a == 1) break; f(); } g(); }
void brk(void) { while (a == 1) { if (b == 2) break; f(); if (c == 3) continue; g(); } h(); }
void dwbrk(void) { do { if (b == 2) break; f(); if (c == 3) continue; g(); } while (a == 1); h(); }
void frbrk(void) { for (i = 0; i < 10; i++) { if (b == 2) break; f(); if (c == 3) continue; g(); } h(); }
