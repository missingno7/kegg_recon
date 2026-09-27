extern int a, b, c, d; extern void f(void); extern void g(void); extern void h(void);
void and3(void) { if (a == 1 && b == 2 && c == 3) f(); }
void or3(void) { if (a == 1 || b == 2 || c == 3) f(); }
void andor(void) { if ((a == 1 && b == 2) || c == 3) f(); }
void orand(void) { if (a == 1 || (b == 2 && c == 3)) f(); }
void andorp(void) { if (a == 1 && (b == 2 || c == 3)) f(); }
void orandp(void) { if ((a == 1 || b == 2) && c == 3) f(); }
void notand(void) { if (!(a == 1 && b == 2)) f(); }
void notor(void) { if (!(a == 1 || b == 2)) f(); }
void and_else(void) { if (a == 1 && b == 2) f(); else g(); }
void or_else(void) { if (a == 1 || b == 2) f(); else g(); }
void truth(void) { if (a) f(); if (!b) g(); }
void and4(void) { if ((a == 1 && b == 2) || (c == 3 && d == 4)) f(); }
