extern int a; extern unsigned char *p; extern unsigned char fl; extern void f(void); extern int g(void);
struct S { int x; unsigned char flags; }; extern struct S *sp;
void c1(int l) { if ((a & 2) != 0) f(); if ((sp->flags & 2) == 0) f(); if ((l & 4) != 0) f(); if (g() != 0) f(); if (p != 0) f(); if ((*p & 8) == 0) f(); }
void c2(int l) { if (a & 2) f(); if (!(sp->flags & 2)) f(); if (l & 4) f(); if (g()) f(); if (p) f(); if (!(*p & 8)) f(); }
