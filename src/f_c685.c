#include <string.h>
extern unsigned char g_123c[];
extern int g_e2a4;
extern int g_e2a8;


struct Pic {
    unsigned char *buf;
    unsigned char *pixels;
    int w;
    int h;
    int bpp;
    int ncolors;
    int pal_end;
    int palsize;
    int size;
};

int f_c685(int data, unsigned char *buf, struct Pic *pic)
{
    int unused;
    unsigned char *hdr;
    unsigned char *src;
    unsigned char *dst;
    int i;
    hdr = (unsigned char *)data;
    if (strcmp((char *)g_123c, (char *)hdr) != 0)
        return 0x301;
    pic->buf = buf;
    pic->w = ((*(short *)(hdr + 8) >> 8) & 0xff) | ((*(short *)(hdr + 8) << 8) & 0xff00);
    pic->h = ((*(short *)(hdr + 0xa) >> 8) & 0xff) | ((*(short *)(hdr + 0xa) << 8) & 0xff00);
    pic->ncolors = ((*(short *)(hdr + 0xc) >> 8) & 0xff) | ((*(short *)(hdr + 0xc) << 8) & 0xff00);
    pic->bpp = 5;
    pic->palsize = pic->ncolors * 3;
    pic->size = pic->w * pic->h;
    pic->pixels = pic->buf + pic->size;
    pic->pal_end = pic->size + pic->palsize;
    g_e2a8 = pic->size + 0x20 + pic->palsize;
    g_e2a4 = pic->size + pic->palsize;
    memcpy(buf, (void *)(data + 0x20 + pic->palsize), pic->size);
    dst = pic->pixels;
    src = (unsigned char *)(data + 0x20);
    for (i = 0; i < pic->palsize; i++)
        *dst++ = *src++ >> 2;
    return 0;
}
