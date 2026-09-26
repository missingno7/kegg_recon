extern short g_e468;
extern unsigned char g_e46b,g_e48b,g_e48e;
extern void (*g_7b47)(int,int);
void f_fce4(void) { if (((unsigned char *)&g_e468)[0]&2) goto test_high; goto low_skip; test_high: if (((int)g_e468&0x8000) != 0) goto test_mask; low_skip: goto mask_skip; test_mask: if (g_e46b&0x20) goto test_b; mask_skip: goto b_skip; test_b: if (g_e48b==0x1c) goto b_skip; test_e: if (g_e48e==0x1c) goto call_key; goto b_skip; b_skip: goto end; call_key: g_7b47(0x101,0); end: ; }
