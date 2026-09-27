extern int a, b, c; extern void f(void); extern void g(void);
void p1(void) { if (a == 1 && b == 2) f(); }
void p4(void) { if (a == 1 || b == 2) f(); }
void p5(void) { if (a == 1) f(); else if (b == 2) f(); }
