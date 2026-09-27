extern int a; extern void f(void); extern void g(void); extern void h(void);
void y1(void) { switch (a) { case 1: f(); break; case 5: g(); break; case 9: h(); break; case 13: f(); break; } }
void y2(void) { switch (a) { case 1: f(); break; case 2: g(); break; case 3: h(); break; case 5: f(); break; } }
void y3(void) { switch (a) { case 1: f(); break; case 3: g(); break; case 5: h(); break; case 7: f(); break; } }
void y4(void) { switch (a) { case 1: case 2: case 3: case 4: f(); break; default: g(); } }
void y5(void) { switch (a) { case 1: f(); break; case 2: g(); break; case 3: h(); break; case 4: f(); break; case 5: g(); break; case 6: h(); break; case 20: f(); break; } }
