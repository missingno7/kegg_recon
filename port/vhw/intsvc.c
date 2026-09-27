/* intsvc.c - INT n services reached through Watcom int386()/int386x().
 *
 * Implemented as DOS/4GW + DOS 6.22 + VGA BIOS + MS mouse driver would answer, limited to
 * the services the game calls (inventory in docs/port/architecture.md):
 *   10h  00h set mode, 0Fh get mode, 1A00h display combination
 *   21h  25h/35h set/get vector (DOS/4GW: protected-mode vector), 30h version, 48h
 *        allocate (BX=FFFFh size query)
 *   2Fh  1600h Windows check, 1687h DPMI installation, 4300h/4310h XMS
 *   31h  0100h/0101h DOS memory, 0200h/0201h real-mode vectors, 0204h/0205h PM vectors,
 *        0400h version (+PIC bases), 0500h free memory info, 0600h/0601h lock/unlock,
 *        0800h physical mapping (identity)
 *   33h  mouse (mouse.c)
 *   67h  EMS: not present (AH=84h)
 * Anything else returns with CF set and is logged once.
 */
#include <stdio.h>
#include <string.h>
#include "vhw.h"
#include "../include/ke_port.h"

#define CF_SET(r) ((r)->x.cflag = 1)
#define CF_CLEAR(r) ((r)->x.cflag = 0)

static void unknown(int intno, union REGS *r)
{
    char key[32];
    snprintf(key, sizeof key, "int%02x.%04x", intno, r->w.ax);
    ke_log_once(key, KE_LOG_WARN, "int", "INT %02Xh AX=%04Xh not implemented (CF=1)", intno,
                r->w.ax);
    CF_SET(r);
}

static void int10(union REGS *r)
{
    switch (r->h.ah) {
    case 0x00:
        vga_bios_set_mode(r->h.al);
        break;
    case 0x0f:
        r->h.al = (uint8_t)vga_bios_mode();
        r->h.ah = vga_bios_mode() == 0x13 ? 40 : 80;
        r->h.bh = 0;
        break;
    case 0x1a:
        if (r->h.al == 0x00) {
            r->h.al = 0x1a;
            r->h.bl = 0x08;     /* VGA with analog color display */
            r->h.bh = 0x00;
        }
        break;
    default:
        unknown(0x10, r);
        return;
    }
    CF_CLEAR(r);
}

static void int21(union REGS *r, struct SREGS *s)
{
    uint16_t sel, seg, largest;
    uint32_t off;
    CF_CLEAR(r);
    switch (r->h.ah) {
    case 0x25:
        vpic_set_pm_vector(r->h.al, s ? s->ds : KE_FLAT_SELECTOR, r->x.edx);
        break;
    case 0x35:
        vpic_get_pm_vector(r->h.al, &sel, &off);
        if (!off) {             /* not hooked: report the real-mode BIOS vector */
            uint16_t o16;
            vpic_get_rm_vector(r->h.al, &seg, &o16);
            sel = seg;
            off = o16;
        }
        if (s)
            s->es = sel;
        r->x.ebx = off;
        break;
    case 0x30:
        r->h.al = 6;            /* DOS 6.22 */
        r->h.ah = 22;
        r->h.bh = 0;
        r->x.ebx &= 0xffff00ffu;
        r->w.cx = 0;
        break;
    case 0x48:
        if (lowmem_dos_alloc(r->w.bx, &seg, &largest) == 0) {
            r->w.ax = seg;
        } else {
            r->w.ax = 8;
            r->w.bx = largest;
            CF_SET(r);
        }
        break;
    case 0x49:
        if (s && lowmem_dos_free(s->es) != 0) {
            r->w.ax = 9;
            CF_SET(r);
        }
        break;
    default:
        unknown(0x21, r);
    }
}

static void int2f(union REGS *r, struct SREGS *s)
{
    CF_CLEAR(r);
    switch (r->w.ax) {
    case 0x1600:
        if (ke_config.windows_host) {
            r->h.al = 3;        /* Windows 3.10 enhanced mode */
            r->h.ah = 10;
        } else {
            r->h.al = 0;
        }
        break;
    case 0x1687:                /* DPMI 0.90 host, 32-bit programs supported */
        r->w.ax = 0;
        r->w.bx = 1;
        r->h.cl = 3;
        r->w.dx = 0x005a;
        r->w.si = 0;
        r->w.di = 0x0000;
        if (s)
            s->es = KE_BIOS_ROM_SEGMENT;
        break;
    case 0x4300:
        r->h.al = 0x80;         /* XMS driver installed */
        break;
    case 0x4310:
        r->w.bx = KE_BIOS_IRET_OFFSET;
        if (s)
            s->es = KE_BIOS_ROM_SEGMENT;
        break;
    default:
        unknown(0x2f, r);
    }
}

static void int31(union REGS *r, struct SREGS *s)
{
    uint16_t seg, largest, sel, o16;
    uint32_t off;
    CF_CLEAR(r);
    switch (r->w.ax) {
    case 0x0100:
        if (lowmem_dos_alloc(r->w.bx, &seg, &largest) == 0) {
            r->w.ax = seg;
            r->w.dx = seg;      /* selector: the flat model uses the segment as handle */
        } else {
            r->w.ax = 8;
            r->w.bx = largest;
            CF_SET(r);
        }
        break;
    case 0x0101:
        if (lowmem_dos_free(r->w.dx) != 0) {
            r->w.ax = 9;
            CF_SET(r);
        }
        break;
    case 0x0200:
        vpic_get_rm_vector(r->h.bl, &seg, &o16);
        r->w.cx = seg;
        r->w.dx = o16;
        break;
    case 0x0201:
        vpic_set_rm_vector(r->h.bl, r->w.cx, r->w.dx);
        break;
    case 0x0204:
        vpic_get_pm_vector(r->h.bl, &sel, &off);
        r->w.cx = sel;
        r->x.edx = off;
        break;
    case 0x0205:
        vpic_set_pm_vector(r->h.bl, r->w.cx, r->x.edx);
        break;
    case 0x0400:
        r->h.ah = 0;
        r->h.al = 90;           /* DPMI 0.90 */
        r->w.bx = 0x0003;       /* 32-bit, reflects to real mode */
        r->h.cl = 3;
        r->h.dh = (uint8_t)vpic_vector_base(0);
        r->h.dl = (uint8_t)vpic_vector_base(1);
        break;
    case 0x0500:
        if (s) {                /* ES:EDI -> 48-byte info block */
            uint32_t *info = (uint32_t *)(uintptr_t)r->x.edi;
            memset(info, 0xff, 48);
            info[0] = 16u << 20; /* largest free block: 16 MiB */
            info[1] = info[0] / 4096;
            info[2] = info[1];
        }
        break;
    case 0x0600:
    case 0x0601:
        break;
    case 0x0800:                /* BX:CX physical -> BX:CX linear (identity) */
        break;
    default:
        unknown(0x31, r);
    }
}

int vhw_int(int intno, union REGS *in, union REGS *out, struct SREGS *s)
{
    union REGS r = *in;
    ke_log(KE_LOG_TRACE, "int", "INT %02Xh AX=%04X BX=%04X CX=%04X DX=%04X", intno, r.w.ax,
           r.w.bx, r.w.cx, r.w.dx);
    switch (intno) {
    case 0x10: int10(&r); break;
    case 0x21: int21(&r, s); break;
    case 0x2f: int2f(&r, s); break;
    case 0x31: int31(&r, s); break;
    case 0x33: vmouse_int33(&r, s); CF_CLEAR(&r); break;
    case 0x67: r.h.ah = 0x84; CF_CLEAR(&r); break;   /* EMS: function not supported */
    default: unknown(intno, &r); break;
    }
    *out = r;
    return (int)r.x.eax;
}

/* ---- Watcom clib i86.h ----------------------------------------------------------------- */
int int386(int intno, void *in, void *out)
{
    int ax;
    vhw_enter();
    ax = vhw_int(intno, (union REGS *)in, (union REGS *)out, NULL);
    vhw_leave();
    return ax;
}

int int386x(int intno, void *in, void *out, void *sregs)
{
    int ax;
    vhw_enter();
    ax = vhw_int(intno, (union REGS *)in, (union REGS *)out, (struct SREGS *)sregs);
    vhw_leave();
    return ax;
}

void segread(struct SREGS *s)
{
    s->cs = s->ds = s->es = s->ss = s->fs = s->gs = KE_FLAT_SELECTOR;
}
