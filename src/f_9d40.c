/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
extern int (* kbd_irq_hook)();
extern int (* kbd_poll_hook)();
extern int (* mouse_update_hook)();
extern int (* sprite_update_hook)();
extern unsigned char *g_73c8;
extern unsigned char g_756f[];
extern int g_e1a8;
extern short g_e1be;
extern short g_e1c0;
extern void f_9e10(void);
extern void f_9e54(void);
void f_9d40(short a0)
{
    int v_4;
    v_4 = g_e1be;
    if ((unsigned short)(a0 & 1) != 1) goto L_9d6a;
    (*kbd_irq_hook)();
L_9d6a:;
    if ((unsigned short)(a0 & 1) != 1) goto L_9d7e;
    (*kbd_poll_hook)();
L_9d7e:;
    if ((a0 & 2) == 0) goto L_9d8c;
    (*mouse_update_hook)();
L_9d8c:;
    if ((a0 & 4) == 0) goto L_9d9a;
    (*sprite_update_hook)();
L_9d9a:;
    if (*(short *)g_756f != -1) goto L_9ddf;
    g_e1a8 = 0;
L_9db0:;
    if (g_e1be != v_4) goto L_9ddd;
    ++g_e1a8;
    if (*(unsigned char *)g_73c8 == 0) goto L_9ddb;
    ++g_e1be;
    *(unsigned char *)g_73c8 = 0;
L_9ddb:;
    goto L_9db0;
L_9ddd:;
    goto L_9df3;
L_9ddf:;
    g_e1a8 = 0;
    f_9e10();
    f_9e54();
L_9df3:;
    g_e1c0 = g_e1be;
    g_e1be = 0;
}
