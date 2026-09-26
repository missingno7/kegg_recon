/* Draft lifted from original instructions; verify with tools/check.py. */
void f_bc42(int a0, int a1)
{
    (*(char *)((a0))) = ((a1 >> 24) & 255);
    (*(char *)((a0) + 1)) = ((a1 >> 16) & 255);
    (*(char *)((a0) + 2)) = ((a1 >> 8) & 255);
    (*(char *)((a0) + 3)) = a1;
}
