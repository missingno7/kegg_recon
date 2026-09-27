extern int a, b, c, d, i, x; extern void f(void); extern void g(void); extern void h(void);
void s1(void) { switch (a) { case 1: f(); break; case 2: g(); break; default: h(); } f(); }
void s2(void) { switch (a) { case 1: f(); break; case 2: g(); break; case 3: h(); break; case 4: f(); case 5: g(); break; } f(); }
void s3(void) { switch (a) { case 1: f(); break; case 100: g(); break; case 1000: h(); break; } }
void t1(void) { x = a == 1 ? b : c; }
void t2(void) { x = (a == 1 && b == 2); }
void t3(void) { x = !a; }
void t4(void) { if (a == 1 ? b : c) f(); }
