extern int a, b, c, d, i; extern void f(void); extern void g(void); extern void h(void);
void q1(void) { f(); if (a == 1) goto L; L: g(); }
void q2(void) { f(); goto L; L: g(); }
void q3(void) { if (a == 1) { f(); return; } else g(); h(); }
void q4(void) { if (a == 1) { f(); goto X; } else g(); h(); X: f(); }
void q5(void) { f(); goto X; g(); X: h(); }
void q6(void) { while (a == 1) { f(); break; } g(); }
void q7(void) { while (a == 1) { f(); continue; } g(); }
void q8(void) { if (a == 1) goto X; else goto Y; X: f(); Y: g(); }
void q9(void) { if (a == 1) { goto X; } f(); X: g(); }
void q10(void) { if (a == 1) { f(); if (b == 2) goto X; } else g(); X: h(); }
int q11(int x) { if (x == 1) return 3; else if (x == 2) return 4; return 5; }
void q12(void) { if (a == 1) { if (b == 2) return; f(); } g(); }
