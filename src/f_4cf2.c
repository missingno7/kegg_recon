/* Draft lifted from original instructions; verify with tools/check.py. */
extern unsigned char *g_388a;
extern unsigned char *g_38fe;
typedef struct { int value, pad; } Pair8;
extern Pair8 g_63dc[];
extern int g_7c08;
extern int g_8e1c;
extern int g_8e20;
extern int g_dda4;
extern int g_dda8;
extern int g_ddac, g_dda8, g_ddb8;
extern int g_ddb4;
extern int g_ddbc;
extern int g_ddc0;
extern char g_a990[];
extern char g_df78[];
#define DDA4 ((unsigned char *)g_dda4)
#define DDB4 ((unsigned char *)g_ddb4)
extern int g_ddd4;
extern unsigned char *g_de5c;
typedef struct { unsigned b0:1, b1:1, b2:1; } Flags;
typedef struct {
    int at_00, at_04, at_08, at_0c, at_10, at_14, at_18, at_1c;
    int at_20, at_24, at_28, at_2c, at_30, at_34, at_38, at_3c;
    int at_40, at_44, at_48, at_4c, at_50, at_54, at_58, at_5c;
    int at_60, at_64, at_68, at_6c, at_70, at_74, at_78;
} State;
extern State *g_dee4;
extern char g_e13a;
extern unsigned char g_e13b, g_e46b;
extern short g_e4c4;
extern short g_e4c6;
extern int f_10502();
extern int f_13a48();
extern int f_5381();
extern int f_57fa();
void f_4cf2(void)
{
    int *v_4;
    int *v_8;
    int v_c;
    v_4 = (int *)(g_388a + 4);
    v_8 = (int *)g_38fe;
    v_c = (*(v_4 - 1));
    g_dee4->at_08 = g_dee4->at_00;
    if (g_de5c[0] & 2) {
    if (--g_dee4->at_50 <= 0) {
    g_dee4->at_50 = 4;
    if ((g_dee4->at_1c + 8) <= ++g_dee4->at_54) {
    if (v_c) {
L_4d74:;
    v_c = (v_c ^ (*v_4++));
    if (v_4 < v_8) goto L_4d74;
    if (v_c) {
    v_c = 0;
    g_7c08 = 0;
    g_dda8 = 5000;
    }
    }
L_4da9:;
    g_de5c[0] &= 0xfd;
    f_10502(g_dee4->at_00, g_dee4->at_04);
    }
    }
    } else {
    if (g_de5c[0] & 1) {
    if (--g_dee4->at_48 <= 0) {
    g_dee4->at_48 = 4;
    g_dee4->at_4c++;
    g_8e20 = (g_dee4->at_4c - 12);
    if (g_8e20 >= 0) {
    if (g_63dc[g_8e20].value < 0) {
    g_dee4->at_4c--;
    g_e13a = 255;
    } else {
    g_de5c[1] &= 0xfe;
    g_de5c[1] &= 0xfb;
    g_de5c[0] &= 0xbf;
    }
    }
    }
    } else {
    if (g_de5c[0] & 0x20) {
    if (--g_dee4->at_58 == 0) {
    g_dee4->at_58 = 3;
    if (g_dee4->at_1c > g_dee4->at_5c) {
    g_dee4->at_1c--;
    f_57fa();
    } else {
    if (g_dee4->at_1c < g_dee4->at_5c) {
    g_dee4->at_1c++;
    f_57fa();
    } else {
    g_de5c[0] &= 0xdf;
    }
    }
    }
    }
L_4ec7:;
    if (g_de5c[1] & 8) {
    g_8e20 = --g_dee4->at_30;
    if (g_8e20 < 32) {
    f_13a48(g_ddd4, 0, 256, g_8e20);
    }
L_4f06:;
    if (g_8e20 <= 0) {
    g_de5c[1] &= 0xf7;
    }
    }
L_4f18:;
    if (g_de5c[1] & 0x20) {
    if (--g_dee4->at_2c <= 0) {
    g_de5c[1] &= 0xdf;
    g_ddb4 = (int)g_df78;
    g_ddac = 0;
L_4f4e:;
    for (; g_ddac < g_dda8; g_ddac++) {
    if ((*(int *)(DDB4 + 4)) > 2432) {
    DDB4[0x11] &= 0xfd;
    }
L_4f7c:;
    g_ddb4 += 18;
    }
    }
    }
L_4f85:;
    if (g_e13b) {
    if (g_de5c[1] & 1) goto L_5377;
    } else {
    if (!(g_de5c[0] & 8)) goto L_5241;
    if (--g_dee4->at_34 <= 0) {
    g_de5c[0] &= 0xf7;
    g_de5c[0] &= 0xfb;
    f_10502(g_dee4->at_00, g_dee4->at_04);
    } else {
    if (g_dee4->at_34 < 140) {
    ((Flags *)g_de5c)->b2 = (g_dee4->at_34 >> 3) & 1;
    }
    }
    }
L_501a:;
    g_8e20 = 0;
    g_ddb4 = (int)g_df78;
    g_ddac = 0;
L_5038:;
    for (; g_ddac < g_dda8; g_ddac++) {
    if (!(DDB4[0x11] & 1)) {
    if ((*(int *)(DDB4 + 4)) > g_8e20) goto L_506c;
    }
    goto L_508b;
L_506c:;
    g_8e20 = ((int)(*(int *)(DDB4 + 4)) >> (int)4);
    g_8e1c = ((int)(*(int *)DDB4) >> (int)4);
L_508b:;
    g_ddb4 += 18;
    }
L_5094:;
    g_ddb4 = (int)g_df78;
    if ((*(int *)(DDB4 + 0xc)) < 0) {
    g_dda4 = (int)g_a990;
    g_ddbc = 0;
L_50c1:;
    for (; g_ddbc < g_ddc0; g_ddbc++) {
    unsigned char v_10;
    v_10 = DDA4[0x11];
    if (v_10 >= 11) {
    if (v_10 <= 11) goto L_5128;
    if (v_10 >= 18) {
    if (v_10 <= 18) goto L_5128;
    if (v_10 == 22) goto L_5128;
    goto L_512a;
    }
L_5106:;
    if (v_10 == 14) goto L_5128;
    goto L_512a;
    } else {
    if (v_10 >= 3) {
    if (v_10 <= 4) goto L_5128;
    if (v_10 == 9) goto L_5128;
    goto L_512a;
    } else {
    if (v_10 != 1) goto L_512a;
    }
    }
L_5128:;
    goto L_5153;
L_512a:;
    if ((*(int *)(DDA4 + 4)) > g_8e20) {
    g_8e20 = (*(int *)(DDA4 + 4));
    g_8e1c = (*(int *)DDA4);
    }
L_5153:;
    g_dda4 += 18;
    }
    }
L_515f:;
    if (g_dee4->at_04 < 188) {
    g_dee4->at_04 += 5;
    if (g_dee4->at_04 > 188) {
    g_dee4->at_04 = 188;
    }
    }
L_5190:;
    if (g_dee4->at_00 < g_8e1c) {
    g_dee4->at_00 += 8;
    if (g_dee4->at_00 > g_8e1c) {
    g_dee4->at_00 = g_8e1c;
    }
    } else {
    if (g_dee4->at_00 > g_8e1c) {
    g_dee4->at_00 -= 8;
    if (g_dee4->at_00 < g_8e1c) {
    g_dee4->at_00 = g_8e1c;
    }
    }
    }
L_51f8:;
    if (g_dee4->at_00 < g_dee4->at_0c) {
    g_dee4->at_00 = g_dee4->at_0c;
    }
L_521a:;
    if (g_dee4->at_00 > g_dee4->at_10) {
    g_dee4->at_00 = g_dee4->at_10;
    }
    goto L_5377;
L_5241:;
    if (g_de5c[1] & 1) {
    if (--g_dee4->at_3c <= 0) {
    g_de5c[1] &= 0xfe;
    f_10502(g_dee4->at_00, g_dee4->at_04);
    }
    } else {
    if (g_de5c[0] & 0x10) {
    if (--g_dee4->at_38 <= 0) {
    g_de5c[0] &= 0xef;
    g_de5c[0] &= 0xfb;
    g_dee4->at_00 = ((g_dee4->at_10 + g_dee4->at_0c) - (short)g_e4c6);
    g_dee4->at_04 = (short)g_e4c4;
    f_10502(g_dee4->at_00, g_dee4->at_04);
    } else {
    g_dee4->at_00 = ((g_dee4->at_10 + g_dee4->at_0c) - (short)g_e4c6);
    g_dee4->at_04 = (short)g_e4c4;
    if (g_dee4->at_38 < 32) {
    ((Flags *)g_de5c)->b2 = (g_dee4->at_38 >> 2) & 1;
    }
    }
    } else {
    g_dee4->at_00 = (short)g_e4c6;
    g_dee4->at_04 = (short)g_e4c4;
    }
    }
    }
    }
L_5377:;
    f_5381();
}
