extern short g_7b10;
extern void f_ec76(void *);
void f_ec44(void * value) {
    if (g_7b10 == -1) {
        f_ec76(value);
        g_7b10 = 1;
    }
}
