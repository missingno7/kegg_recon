extern int a, b, c, d, i; extern void f(void); extern void g(void); extern void h(void);
void v1(void) { for (i = 0; i < 10; i++) { f(); if (a == 1) break; } g(); }
void v2(void) { while (b == 1) { f(); if (a == 1) break; } g(); }
void v3(void) { f(); if (a == 1) goto X; goto Y; X: g(); Y: h(); }
void v4(void) { f(); if (a == 1) goto X; L: goto Y; X: g(); Y: h(); }
void v5(void) { if (a == 1) { f(); if (b == 1) goto X; } g(); X: h(); }
