extern int a, b, c, d, i; extern void f(void); extern void g(void); extern void h(void);
void u1(void) { f(); goto X; L: g(); X: h(); }
void u2(void) { if (a == 1) { goto X; } L:; f(); X: g(); }
void u3(void) { if (a == 1) { f(); return; } L:; g(); }
void u4(void) { while (a == 1) { f(); if (b == 2) continue; g(); } }
void u5(void) { do { f(); if (b == 2) continue; g(); } while (a == 1); }
void u6(void) { for (;;) { f(); if (b == 2) continue; g(); } }
void u7(void) { while (1) { f(); if (b == 2) continue; if (c == 3) break; g(); } h(); }
void u8(void) { for (i = 0; ; i++) { f(); if (b == 2) continue; g(); } }
