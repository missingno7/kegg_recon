#include <string.h>

extern short g_7b39;
extern int g_7418, g_75bc, g_e366, g_e36a, g_e35e, g_e362;
extern unsigned char f_13324(int, int);
extern int f_107b6(char *, void *, unsigned int);
extern void *f_ddb9(int);
extern void f_de21(void *);
extern void f_bc42(int, int);
extern void f_ebcd(unsigned char *);

struct header9 {
    int first;
    int second;
    unsigned char last;
};
extern struct header9 g_1232;

int f_b804(char *name)
{
    int result;
    unsigned char *p;
    int x;
    int y;
    int saved_mode;
    unsigned char *allocated;
    unsigned char *header;
    unsigned char *map;
    unsigned char *body;
    int four;
    int twenty;
    int palette_bytes;
    int pixel_bytes;
    int total_bytes;
    unsigned char *raw;
    int raw_bytes;

    saved_mode = g_7b39;
    four = 4;
    twenty = 0x14;
    palette_bytes = 0x300;
    pixel_bytes = g_e36a * ((g_e366 + 1) & -2);
    total_bytes = ((((((pixel_bytes + 8) + palette_bytes) + 8) + twenty) + 8) + four) + 8;
    raw_bytes = g_e366 * g_e36a + 0x320;
    {
    unsigned int mode;
    mode = g_7418;

    if (mode < 1)
        goto L_b895;
    if (mode <= 1)
        goto L_b9f9;
    if (mode == 5)
        goto L_b89a;
    goto L_bc34;

L_b895:
    goto L_bc34;

L_b89a:
    raw = (unsigned char *)f_ddb9(raw_bytes);
    if (raw == 0)
        return g_75bc;
    g_7b39 = -1;
    *(struct header9 *)raw = g_1232;
    raw[6] = 0;
    raw[7] = 4;
    f_bc42((int)(raw + 8), (g_e366 << 0x10) | g_e36a);
    f_bc42((int)(raw + 0xc), 0x1000000);
    memset(raw + 0xe, 0, 0x12);
    p = raw + 0x20;
    f_ebcd(p);
    for (x = 0; x < 0x300; x++)
        p[x] = p[x] << 2;

    p = raw + 0x320;
    for (y = 0; y < g_e36a; y++)
        for (x = 0; x < g_e366; x++)
            *p++ = (unsigned char)f_13324(x, y);

    result = f_107b6(name, raw, raw_bytes);
    g_7b39 = (short)saved_mode;
    f_de21(raw);
    goto L_bc34;

L_b9f9:
    allocated = (unsigned char *)f_ddb9(total_bytes);
    if (allocated == 0)
        return g_75bc;
    g_7b39 = -1;
    header = allocated + four + 8;
    map = header + twenty + 8;
    body = map + palette_bytes + 8;
    f_bc42((int)allocated, 0x464f524d);
    f_bc42((int)(allocated + 4), total_bytes - 8);
    f_bc42((int)(allocated + 8), 0x50424d20);
    f_bc42((int)header, 0x424d4844);
    f_bc42((int)(header + 4), twenty);
    f_bc42((int)(header + 8), (g_e366 << 0x10) | g_e36a);
    *(int *)(header + 0xc) = 0;
    header[0x11] = 0;
    header[0x13] = 0;
    header[0x13] = 0;
    header[0x10] = 8;
    header[0x12] = 0;
    *(int *)(header + 0x14) = 0x605ff00;
    f_bc42((int)(header + 0x18), (g_e35e << 0x10) | g_e362);
    f_bc42((int)map, 0x434d4150);
    f_bc42((int)(map + 4), palette_bytes);
    f_ebcd(map + 8);
    for (x = 8; x < palette_bytes + 8; x++)
        map[x] = map[x] << 2;
    f_bc42((int)body, 0x424f4459);
    f_bc42((int)(body + 4), pixel_bytes);
    p = body + 8;
    for (y = 0; y < g_e36a; y++)
        for (x = 0; x < ((g_e366 + 1) & -2); x++)
            *p++ = (unsigned char)f_13324(x, y);

    result = f_107b6(name, allocated, (total_bytes + 3) & -4);
    g_7b39 = (short)saved_mode;
    f_de21(allocated);

L_bc34:
    return result;
    }
}
