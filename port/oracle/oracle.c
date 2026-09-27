/* oracle.c - LE loader, fixups, native calls and privileged-instruction emulation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"

#define MAX_EVENTS 8192
static OracleEvent events[MAX_EVENTS];
static int event_count;
static uint8_t *object_base[4];
static uint32_t object_size[4];
static struct { char name[64]; int obj; uint32_t off; } *symbols;
static int symbol_count;
static oracle_in_fn hook_in = vhw_port_in;
static oracle_out_fn hook_out = vhw_port_out;

void oracle_set_port_hooks(oracle_in_fn in, oracle_out_fn out)
{
    hook_in = in ? in : vhw_port_in;
    hook_out = out ? out : vhw_port_out;
}
void oracle_trace_reset(void) { event_count = 0; }
int oracle_trace_count(void) { return event_count; }
const OracleEvent *oracle_trace(void) { return events; }
void oracle_trace_add(uint8_t kind, uint16_t port, uint32_t value, int size)
{
    if (event_count < MAX_EVENTS) {
        events[event_count].kind = kind;
        events[event_count].size = (uint8_t)size;
        events[event_count].port = port;
        events[event_count].value = value;
        event_count++;
    }
}

/* ---- privileged instruction emulation ----------------------------------------------- */
static int modrm_length(const uint8_t *p)
{
    uint8_t modrm = p[0], mod = modrm >> 6, rm = modrm & 7;
    int len = 1;
    if (mod == 3)
        return 1;
    if (rm == 4) {                      /* SIB */
        len++;
        if (mod == 0 && (p[1] & 7) == 5)
            len += 4;
    }
    if (mod == 0 && rm == 5)
        len += 4;
    else if (mod == 1)
        len += 1;
    else if (mod == 2)
        len += 4;
    return len;
}

static void set_reg_part(DWORD *reg, uint32_t v, int size)
{
    if (size == 1) *reg = (*reg & ~0xffu) | (v & 0xff);
    else if (size == 2) *reg = (*reg & ~0xffffu) | (v & 0xffff);
    else *reg = v;
}

static LONG CALLBACK oracle_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
    int opsize = 4, len = 0;
    if (code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    if (IsBadReadPtr(p, 4))
        return EXCEPTION_CONTINUE_SEARCH;
    if (*p == 0x66) {
        opsize = 2;
        p++;
        len = 1;
    }
    switch (p[0]) {
    case 0xe4: case 0xe5: case 0xec: case 0xed: {           /* IN */
        int size = (p[0] & 1) ? opsize : 1;
        uint16_t port = (p[0] & 8) ? (uint16_t)c->Edx : p[1];
        uint32_t v = 0;
        if (size == 1)
            v = hook_in(port, 1) & 0xff;
        else
            v = (hook_in(port, 1) & 0xff) | ((hook_in((uint16_t)(port + 1), 1) & 0xff) << 8);
        set_reg_part(&c->Eax, v, size);
        oracle_trace_add('I', port, v, size);
        len += (p[0] & 8) ? 1 : 2;
        break;
    }
    case 0xe6: case 0xe7: case 0xee: case 0xef: {           /* OUT */
        int size = (p[0] & 1) ? opsize : 1;
        uint16_t port = (p[0] & 8) ? (uint16_t)c->Edx : p[1];
        uint32_t v = c->Eax & (size == 1 ? 0xff : 0xffff);
        hook_out(port, v & 0xff, 1);
        if (size != 1)
            hook_out((uint16_t)(port + 1), (v >> 8) & 0xff, 1);
        oracle_trace_add('O', port, v, size);
        len += (p[0] & 8) ? 1 : 2;
        break;
    }
    case 0xfa:
        oracle_trace_add('C', 0, 0, 0);
        len += 1;
        break;
    case 0xfb:
        oracle_trace_add('S', 0, 0, 0);
        len += 1;
        break;
    case 0xcd: {                                             /* INT nn */
        union REGS r;
        struct SREGS s;
        memset(&s, 0, sizeof s);
        r.x.eax = c->Eax; r.x.ebx = c->Ebx; r.x.ecx = c->Ecx;
        r.x.edx = c->Edx; r.x.esi = c->Esi; r.x.edi = c->Edi;
        r.x.cflag = c->EFlags & 1;
        oracle_trace_add('N', p[1], c->Eax, 4);
        vhw_int(p[1], &r, &r, &s);
        c->Eax = r.x.eax; c->Ebx = r.x.ebx; c->Ecx = r.x.ecx;
        c->Edx = r.x.edx; c->Esi = r.x.esi; c->Edi = r.x.edi;
        c->EFlags = (c->EFlags & ~1u) | (r.x.cflag ? 1u : 0u);
        len += 2;
        break;
    }
    case 0x8e:                                               /* MOV Sreg, r/m16 */
        len += 1 + modrm_length(p + 1);
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
    c->Eip += (DWORD)len;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* ---- loader ---------------------------------------------------------------------------- */
static int read_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *data = (uint8_t *)malloc((size_t)n);
    *size = fread(*data, 1, (size_t)n, f);
    fclose(f);
    return *size == (size_t)n ? 0 : -1;
}

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

int oracle_load(const char *image_path, const char *symbols_path)
{
    uint8_t *img;
    size_t size, pos = 12;
    uint32_t nobj, nfix, i;
    uint16_t ds_selector;
    FILE *sf;
    char line[256];
    if (read_file(image_path, &img, &size) != 0 || memcmp(img, "KEIM", 4) != 0) {
        fprintf(stderr, "oracle: cannot read %s (run python port/tools/le_export.py)\n", image_path);
        return -1;
    }
    nobj = rd32(img + 8);
    for (i = 0; i < nobj; i++) {
        uint32_t n = rd32(img + pos), vsize = rd32(img + pos + 8), init = rd32(img + pos + 12);
        uint8_t *mem = (uint8_t *)VirtualAlloc(NULL, vsize + 4096, MEM_RESERVE | MEM_COMMIT,
                                               PAGE_EXECUTE_READWRITE);
        memcpy(mem, img + pos + 16, init);
        object_base[n] = mem;
        object_size[n] = vsize;
        pos += 16 + init;
    }
    __asm__ volatile("mov %%ds, %0" : "=r"(ds_selector));
    nfix = rd32(img + pos);
    pos += 4;
    for (i = 0; i < nfix; i++, pos += 16) {
        const uint8_t *f = img + pos;
        uint8_t src = f[0], kind = f[1], tgt = f[8];
        uint32_t off = rd32(f + 4), toff = rd32(f + 12);
        uint8_t *site = object_base[src] + off;
        if (kind == 7) {
            uint32_t v = (uint32_t)(uintptr_t)(object_base[tgt] + toff);
            memcpy(site, &v, 4);
        } else if (kind == 2) {
            memcpy(site, &ds_selector, 2);
        }
    }
    free(img);
    sf = fopen(symbols_path, "r");
    if (sf) {
        symbols = calloc(2048, sizeof *symbols);
        while (fgets(line, sizeof line, sf) && symbol_count < 2048) {
            unsigned obj, off;
            if (sscanf(line, "%63s %u %x", symbols[symbol_count].name, &obj, &off) == 3) {
                symbols[symbol_count].obj = (int)obj;
                symbols[symbol_count].off = off;
                symbol_count++;
            }
        }
        fclose(sf);
    }
    AddVectoredExceptionHandler(1, oracle_veh);
    printf("oracle: KE.EXE objects at %p %p %p, %u fixups applied, %d symbols\n",
           (void *)object_base[1], (void *)object_base[2], (void *)object_base[3], nfix,
           symbol_count);
    return 0;
}

void *oracle_object_base(int object) { return object_base[object]; }

void *oracle_sym(const char *name)
{
    int i;
    for (i = 0; i < symbol_count; i++)
        if (strcmp(symbols[i].name, name) == 0)
            return object_base[symbols[i].obj] + symbols[i].off;
    return NULL;
}

uint32_t oracle_call(void *fn, int argc, const uint32_t *a)
{
    typedef uint32_t (*F0)(void);
    typedef uint32_t (*F1)(uint32_t);
    typedef uint32_t (*F2)(uint32_t, uint32_t);
    typedef uint32_t (*F3)(uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*F4)(uint32_t, uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*F5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*F6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    if (!fn) {
        fprintf(stderr, "oracle_call: NULL function (symbol not in ke_symbols.txt?)\n");
        exit(3);
    }
    switch (argc) {
    case 0: return ((F0)fn)();
    case 1: return ((F1)fn)(a[0]);
    case 2: return ((F2)fn)(a[0], a[1]);
    case 3: return ((F3)fn)(a[0], a[1], a[2]);
    case 4: return ((F4)fn)(a[0], a[1], a[2], a[3]);
    case 5: return ((F5)fn)(a[0], a[1], a[2], a[3], a[4]);
    default: return ((F6)fn)(a[0], a[1], a[2], a[3], a[4], a[5]);
    }
}
