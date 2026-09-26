typedef struct {
    int at_00, at_04, at_08, at_0c, at_10, at_14, at_18, at_1c;
    int at_20, at_24, at_28, at_2c, at_30, at_34, at_38, at_3c;
    int at_40, at_44;
    int pad_48[6];
    int at_60, at_64, at_68, at_6c, at_70, at_74;
} State;
typedef struct { unsigned char b[18]; } Rec12;
typedef struct { unsigned char b[40]; } Rec28;
extern void f_c20d(int);
extern void f_8fce(int, int);
extern void f_13a48(void *, int, int, int);
extern void f_9053(void);
extern void f_10502(int, int);
extern void f_58b0(void);
extern void f_4394(void);
extern State *g_dee4;
extern unsigned char *g_de5c;
extern int g_ddac, g_dda8, g_ddb8;
extern Rec12 *g_ddb4;
extern Rec12 g_df78[];
extern int g_8e1c, g_8e20, g_8e24;
extern int (* volatile g_8db8)(void);
extern int (*g_6160[])(void);
extern void *g_ddd4;
extern int g_ddc4, g_ddcc;
extern Rec28 *g_dd94;
extern Rec28 g_a848[];
extern short g_e4c6, g_e4c4;

void f_4761(void) {
    f_c20d(0x33);
    g_de5c[1] &= 0xfe;
    g_de5c[0] |= 8;
    g_dee4->at_34 = g_ddb8 << 10;
    g_de5c[1] &= 0xfe;
    if (g_de5c[0] & 0x10) {
        g_de5c[0] &= 0xef;
        g_dee4->at_00 = g_dee4->at_10 + g_dee4->at_0c - (short)g_e4c6;
        g_dee4->at_04 = (short)g_e4c4;
        f_10502(g_dee4->at_00, g_dee4->at_04);
    }
}
