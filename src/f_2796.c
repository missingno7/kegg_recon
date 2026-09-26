extern short g_7b18;
extern int g_dd48;
extern int g_e35a;
extern unsigned short g_746e;

void f_a810(int, int);
void f_3beb(int);

void f_2796(void)
{
    f_a810(g_dd48, g_7b18);
    f_3beb(g_dd48 + g_e35a + 0x900);
    g_746e = 8;
}
