/* Lifted by tools/lift.py (first draft); verify with tools/check.py. */
#include <stdlib.h>
#include <string.h>
extern int g_748c;
extern short g_e2fc;
extern int f_1144d(void);
int f_ccd5(void)
{
    int v_4;
    int v_8;
    unsigned char * v_c;
    v_8 = 0;
    if (v_8 != 0) goto L_cd89;
    v_8 = -1;
    g_e2fc = -1;
    v_c = getenv((char *)g_748c);
    if (v_c == 0) goto L_cd89;
    v_c = strchr((char *)v_c, 0x41);
    if (v_c == 0) goto L_cd89;
    g_e2fc = (unsigned short)((((unsigned short)*(unsigned char *)(v_c + 1) - 0x30) << 8) + (((unsigned short)*(unsigned char *)(v_c + 2) - 0x30) << 4));
    v_4 = 0;
L_cd62:;
    if (v_4 < 5) goto L_cd72;
    goto L_cd89;
L_cd6a:;
    v_4++;
    goto L_cd62;
L_cd72:;
    if (f_1144d() != 0) goto L_cd87;
    return 0;
L_cd87:;
    goto L_cd6a;
L_cd89:;
    g_e2fc = 0x210;
L_cd92:;
    if (g_e2fc < 0x260) goto L_cda9;
    goto L_cdd6;
L_cd9f:;
    g_e2fc += 0x10;
    goto L_cd92;
L_cda9:;
    v_4 = 0;
L_cdb0:;
    if (v_4 < 5) goto L_cdc0;
    goto L_cdd4;
L_cdb8:;
    v_4++;
    goto L_cdb0;
L_cdc0:;
    if (f_1144d() != 0) goto L_cdd2;
    return 0;
L_cdd2:;
    goto L_cdb8;
L_cdd4:;
    goto L_cd9f;
L_cdd6:;
    g_e2fc = -1;
    return -1;
}
