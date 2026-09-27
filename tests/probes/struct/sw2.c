extern int a, b, c, d, i; extern void f(void); extern void g(void); extern void h(void);
void w1(void) { switch (a) { goto X; case 1: f(); break; case 5: g(); break; } X: h(); }
void w2(void) { switch (a) { case 1: f(); break; case 5: g(); break; } h(); }
void w3(void) { switch (a) { case 1: f(); break; case 5: g(); break; default: h(); } h(); }
void w4(void) { switch (a) { case 1: f(); break; case 5: g(); break; default: break; } h(); }
