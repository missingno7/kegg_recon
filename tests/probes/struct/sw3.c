extern int a, b, c, d, i; extern unsigned char uc; extern void f(void); extern void g(void); extern void h(void);
void x1(void) { switch (a) { case 1: case 3: case 4: case 9: case 11: case 14: case 18: case 22: f(); break; default: g(); } h(); }
void x2(void) { switch (a) { case 1: f(); break; case 2: case 3: case 4: g(); break; } h(); }
void x3(void) { switch (uc) { case 0: f(); break; case 1: g(); break; case 2: h(); break; } h(); }
void x4(void) { switch (a) { case 1: f(); break; case 2: g(); break; case 3: h(); break; } h(); }
void x5(void) { switch (a) { case 1: f(); break; case 2: g(); break; case 3: h(); break; case 4: f(); break; } h(); }
void x6(void) { switch (a) { case 5: f(); break; case 1: g(); break; } h(); }
void x7(void) { switch (a) { case -1: f(); break; case 1: g(); break; } h(); }
