extern int a, b, c, d, i; extern void f(void); extern void g(void); extern void h(void);
void r1(void) { if (a == 1) return; f(); }
void r2(void) { if (a == 1) { f(); return; } g(); }
void r3(void) { while (b == 2) { if (a == 1) return; f(); } g(); }
int r4(void) { if (a == 1) return 5; f(); return 7; }
int r5(void) { if (a == 1 && b == 2) return 5; return 7; }
void r6(void) { if (a == 1) { f(); } else { g(); return; } }
void r7(void) { f(); return; }
int r8(void) { int x; x = a; return x; }
void r9(void) { if (a == 1 && b == 2) return; f(); }
void r10(void) { if (a == 1) { if (b == 2) f(); else g(); } else h(); }
void r11(void) { if (a == 1) { if (b == 2) f(); } else h(); }
void r12(void) { if (a == 1) { f(); } else if (b == 2) { g(); } else { h(); } }
void r13(void) { if (a == 1) ; else f(); g(); }
void r14(void) { if (a == 1) {} f(); }
void r15(void) { if (a == 1) { f(); } else { if (b == 2) g(); } h(); }
