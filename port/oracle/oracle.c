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
static void *vga_guard;
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

static int lowmem_emulation_on;
/* Whole-program runs: real-mode IVT/BDA addresses (< 64 KiB) reach the virtual PC's shadow
 * array, as KE_LOWMEM() does for the port's historical source. */
static int is_lowmem_address(uint32_t address)
{
    return lowmem_emulation_on && address < 0x10000u;
}

static int is_vga_address(uint32_t address)
{
    uintptr_t base = (uintptr_t)vga_guard;
    return (address >= 0xA0000u && address < 0xC0000u) ||
           (base && address >= base && address < base + 0x20000u);
}

static uint32_t canonical_vga_address(uint32_t address)
{
    uintptr_t base = (uintptr_t)vga_guard;
    if (base && address >= base && address < base + 0x20000u)
        return 0xA0000u + (address - (uint32_t)base);
    return address;
}

static uint32_t relocate_vga_pointer(uint32_t address)
{
    if (vga_guard && address >= 0xA0000u && address < 0xC0000u)
        return (uint32_t)(uintptr_t)vga_guard + (address - 0xA0000u);
    return address;
}

uint32_t oracle_vga_host_address(uint32_t address)
{
    return relocate_vga_pointer(address);
}

uint32_t oracle_vga_guest_address(uint32_t address)
{
    return canonical_vga_address(address);
}

static int virtual_range_is_free(uintptr_t address, SIZE_T size)
{
    uintptr_t end = address + size;
    while (address < end) {
        MEMORY_BASIC_INFORMATION info;
        uintptr_t region_end;
        if (VirtualQuery((const void *)address, &info, sizeof info) != sizeof info ||
            info.State != MEM_FREE)
            return 0;
        region_end = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (region_end <= address)
            return 0;
        address = region_end < end ? region_end : end;
    }
    return 1;
}

static int reserve_vga_window(void)
{
    const uintptr_t first_host_candidate = 0x000c0000u;
    const uintptr_t search_limit = 0x10000000u;
    const SIZE_T allocation_granularity = 0x10000u;
    const SIZE_T vga_size = 0x20000u;
    void *filler[4096];
    unsigned filler_count = 0;
    uintptr_t address, region_end, candidate;
    DWORD error = ERROR_INVALID_ADDRESS;
    MEMORY_BASIC_INFORMATION info;
    SIZE_T queried;
    if (vga_guard)
        return 0;
    /* Windows may reserve A0000h..BFFFFh for its own stacks or loader data before main().
     * Keep the legacy range unavailable to later VirtualAlloc calls wherever an entire
     * allocation-granularity block is free, then find the first free 128 KiB host window
     * below 10000000h. Reserve isolated free blocks as we scan so later host buffers cannot
     * land below the alias and change the original code's pointer ordering.
     */
    for (address = 0x0a0000u; address < 0x0c0000u; address += allocation_granularity) {
        if (virtual_range_is_free(address, allocation_granularity) &&
            VirtualAlloc((void *)address, allocation_granularity, MEM_RESERVE,
                         PAGE_NOACCESS) != (void *)address) {
            error = GetLastError();
            fprintf(stderr, "oracle: cannot protect free legacy VGA block at %05lxh (%lu)\n",
                    (unsigned long)address, (unsigned long)error);
            return -1;
        }
    }
    address = first_host_candidate;
    while (address + vga_size <= search_limit) {
        queried = VirtualQuery((const void *)address, &info, sizeof info);
        if (queried != sizeof info) {
            error = GetLastError();
            break;
        }
        region_end = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (region_end <= address) {
            error = ERROR_INVALID_ADDRESS;
            break;
        }
        if (info.State != MEM_FREE) {
            address = (region_end + allocation_granularity - 1) &
                      ~((uintptr_t)allocation_granularity - 1);
            continue;
        }
        candidate = (address + allocation_granularity - 1) &
                    ~((uintptr_t)allocation_granularity - 1);
        if (candidate + vga_size <= region_end && candidate + vga_size <= search_limit) {
            void *reserved = VirtualAlloc((void *)candidate, vga_size, MEM_RESERVE, PAGE_NOACCESS);
            if (reserved == (void *)candidate) {
                vga_guard = reserved;
                return 0;
            }
            error = GetLastError();
        }
        while (candidate + allocation_granularity <= region_end &&
               candidate + allocation_granularity <= search_limit) {
            void *reserved;
            if (!virtual_range_is_free(candidate, allocation_granularity)) {
                candidate += allocation_granularity;
                continue;
            }
            reserved = VirtualAlloc((void *)candidate, allocation_granularity, MEM_RESERVE,
                                    PAGE_NOACCESS);
            if (reserved == (void *)candidate)
                filler[filler_count++] = reserved;
            else
                error = GetLastError();
            candidate += allocation_granularity;
        }
        address = (region_end + allocation_granularity - 1) &
                  ~((uintptr_t)allocation_granularity - 1);
    }
    while (filler_count)
        VirtualFree(filler[--filler_count], 0, MEM_RELEASE);
    fprintf(stderr, "oracle: cannot reserve relocated VGA window below %08lxh (%lu)\n",
            (unsigned long)search_limit, (unsigned long)error);
    return -1;
}

int oracle_vga_window_reserved(void) { return vga_guard != NULL; }

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static DWORD *context_reg(CONTEXT *c, int reg)
{
    switch (reg & 7) {
    case 0: return &c->Eax;
    case 1: return &c->Ecx;
    case 2: return &c->Edx;
    case 3: return &c->Ebx;
    case 4: return &c->Esp;
    case 5: return &c->Ebp;
    case 6: return &c->Esi;
    default: return &c->Edi;
    }
}

static uint32_t get_reg_part(CONTEXT *c, int reg, int size)
{
    DWORD *p;
    if (size == 1) {
        p = context_reg(c, reg & 3);
        return (reg & 4) ? ((*p >> 8) & 0xffu) : (*p & 0xffu);
    }
    p = context_reg(c, reg);
    return size == 2 ? (*p & 0xffffu) : *p;
}

static void set_reg_part(CONTEXT *c, int reg, uint32_t value, int size)
{
    DWORD *p;
    if (size == 1) {
        p = context_reg(c, reg & 3);
        if (reg & 4)
            *p = (*p & ~0xff00u) | ((value & 0xffu) << 8);
        else
            *p = (*p & ~0xffu) | (value & 0xffu);
    } else {
        p = context_reg(c, reg);
        if (size == 2)
            *p = (*p & ~0xffffu) | (value & 0xffffu);
        else
            *p = value;
    }
}

typedef struct OracleModrm {
    int mod, reg, rm, memory, length;
    uint32_t address;
} OracleModrm;

static int decode_modrm(const uint8_t *p, CONTEXT *c, int address16, OracleModrm *m)
{
    uint8_t b = p[0];
    int mod = b >> 6, reg = (b >> 3) & 7, rm = b & 7, n = 1;
    uint32_t base = 0;
    m->mod = mod; m->reg = reg; m->rm = rm; m->memory = mod != 3;
    if (!m->memory) {
        m->length = n;
        return 0;
    }
    if (address16) {
        static const uint8_t base_regs[8][2] = {
            {3, 6}, {3, 7}, {5, 6}, {5, 7}, {6, 0xff}, {7, 0xff}, {5, 0xff}, {3, 0xff}
        };
        if (mod == 0 && rm == 6) {
            base = read_u16(p + n);
            n += 2;
        } else {
            base = get_reg_part(c, base_regs[rm][0], 2);
            if (base_regs[rm][1] != 0xff)
                base += get_reg_part(c, base_regs[rm][1], 2);
        }
        if (mod == 1) {
            base += (uint32_t)(int32_t)(int8_t)p[n++];
        } else if (mod == 2) {
            base += (uint32_t)(int32_t)(int16_t)read_u16(p + n);
            n += 2;
        }
        m->address = base & 0xffffu;
    } else {
        if (rm == 4) {
            uint8_t sib = p[n++];
            int scale = sib >> 6, index = (sib >> 3) & 7, breg = sib & 7;
            if (index != 4)
                base += get_reg_part(c, index, 4) << scale;
            if (mod == 0 && breg == 5) {
                base += read_u32(p + n);
                n += 4;
            } else {
                base += get_reg_part(c, breg, 4);
            }
        } else if (mod == 0 && rm == 5) {
            base = read_u32(p + n);
            n += 4;
        } else {
            base = get_reg_part(c, rm, 4);
        }
        if (mod == 1) {
            base += (uint32_t)(int32_t)(int8_t)p[n++];
        } else if (mod == 2) {
            base += read_u32(p + n);
            n += 4;
        }
        m->address = base;
    }
    m->length = n;
    return 0;
}

static uint32_t read_memory_width(uint32_t address, int size)
{
    uint32_t value = 0;
    int i;
    for (i = 0; i < size; i++) {
        uint32_t a = address + (uint32_t)i;
        uint8_t b = is_lowmem_address(a) ? ke_lowmem_shadow[a] :
                    is_vga_address(a) ? vga_mem_read8(canonical_vga_address(a))
                                      : *(volatile uint8_t *)(uintptr_t)a;
        value |= (uint32_t)b << (i * 8);
    }
    return value;
}

static void write_memory_width(uint32_t address, uint32_t value, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        uint32_t a = address + (uint32_t)i;
        uint8_t b = (uint8_t)(value >> (i * 8));
        if (is_lowmem_address(a))
            ke_lowmem_shadow[a] = b;
        else if (is_vga_address(a))
            vga_mem_write8(canonical_vga_address(a), b);
        else
            *(volatile uint8_t *)(uintptr_t)a = b;
    }
}

static uint32_t width_mask(int size)
{
    return size == 1 ? 0xffu : size == 2 ? 0xffffu : 0xffffffffu;
}

static int even_parity(uint8_t value)
{
    value ^= value >> 4;
    value &= 0x0f;
    return ((0x9669u >> value) & 1u) == 0;
}

static void set_logic_flags(CONTEXT *c, uint32_t result, int size)
{
    const DWORD flags = 0x000008d5u; /* CF, PF, AF, ZF, SF, OF */
    uint32_t mask = width_mask(size), sign = size == 1 ? 0x80u : size == 2 ? 0x8000u : 0x80000000u;
    DWORD f = c->EFlags & ~flags;
    result &= mask;
    if (even_parity((uint8_t)result)) f |= 0x04u;
    if (!result) f |= 0x40u;
    if (result & sign) f |= 0x80u;
    c->EFlags = f;
}

/* CMP/SUB flags: CF borrow, OF signed overflow, AF, SF, ZF, PF. */
static void set_sub_flags(CONTEXT *c, uint32_t left, uint32_t right, int size)
{
    uint32_t mask = width_mask(size), sign = size == 1 ? 0x80u : size == 2 ? 0x8000u : 0x80000000u;
    uint32_t result;
    left &= mask;
    right &= mask;
    result = (left - right) & mask;
    set_logic_flags(c, result, size);
    if (left < right) c->EFlags |= 0x01u;
    if (((left ^ right) & (left ^ result)) & sign) c->EFlags |= 0x800u;
    if (((left ^ right ^ result) & 0x10u)) c->EFlags |= 0x10u;
}

static uint32_t logic_result(int operation, uint32_t left, uint32_t right)
{
    switch (operation) {
    case 1: return left & right;
    case 4: return left | right;
    default: return left ^ right;
    }
}

static uint32_t string_index(CONTEXT *c, int reg, int address16)
{
    return get_reg_part(c, reg, address16 ? 2 : 4);
}

static void advance_string_index(CONTEXT *c, int reg, int address16, int step)
{
    int size = address16 ? 2 : 4;
    uint32_t value = string_index(c, reg, address16) + (uint32_t)step;
    set_reg_part(c, reg, address16 ? value & 0xffffu : value, size);
}

/* Emulate a single instruction that faulted on the reserved VGA aperture.  String
 * instructions are completed here as a unit so REP copies need only one Windows fault.
 */
static int oracle_emulate_vga_memory(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    const uint8_t *start = (const uint8_t *)(uintptr_t)c->Eip, *p = start;
    ULONG_PTR fault;
    int operand16 = 0, address16 = 0, repeat = 0, prefixes = 0, width, len;
    uint8_t opcode;
    if (ep->ExceptionRecord->NumberParameters < 2 ||
        ep->ExceptionRecord->ExceptionInformation[0] == 8)
        return 0;
    fault = ep->ExceptionRecord->ExceptionInformation[1];
    if ((!is_vga_address((uint32_t)fault) && !is_lowmem_address((uint32_t)fault)) ||
        IsBadReadPtr(p, 15))
        return 0;
    vga_trace_caller = c->Eip;
    for (;;) {
        switch (*p) {
        case 0x66: operand16 = 1; break;
        case 0x67: address16 = 1; break;
        case 0xf2: case 0xf3: repeat = 1; break;
        case 0xf0: case 0x26: case 0x2e: case 0x36: case 0x3e: case 0x64: case 0x65: break;
        default: goto prefixes_done;
        }
        if (++prefixes >= 14)
            return 0;
        p++;
    }
prefixes_done:
    opcode = *p++;
    width = operand16 ? 2 : 4;
    switch (opcode) {
    case 0x88: case 0x89: case 0x8a: case 0x8b: {
        OracleModrm m;
        int byteop = (opcode == 0x88 || opcode == 0x8a);
        if (decode_modrm(p, c, address16, &m) != 0 || !m.memory)
            return 0;
        p += m.length;
        width = byteop ? 1 : width;
        if (opcode == 0x88 || opcode == 0x89)
            write_memory_width(m.address, get_reg_part(c, m.reg, width), width);
        else
            set_reg_part(c, m.reg, read_memory_width(m.address, width), width);
        break;
    }
    case 0xc6: case 0xc7: {
        OracleModrm m;
        int byteop = opcode == 0xc6;
        if (decode_modrm(p, c, address16, &m) != 0 || !m.memory || m.reg != 0)
            return 0;
        p += m.length;
        width = byteop ? 1 : width;
        write_memory_width(m.address, width == 1 ? *p : width == 2 ? read_u16(p) : read_u32(p), width);
        p += width;
        break;
    }
    case 0xa0: case 0xa1: case 0xa2: case 0xa3: {
        uint32_t address;
        width = (opcode == 0xa0 || opcode == 0xa2) ? 1 : width;
        if (address16) { address = read_u16(p); p += 2; }
        else { address = read_u32(p); p += 4; }
        if (opcode == 0xa0 || opcode == 0xa1)
            set_reg_part(c, 0, read_memory_width(address, width), width);
        else
            write_memory_width(address, get_reg_part(c, 0, width), width);
        break;
    }
    case 0x0f: {
        uint8_t second = *p++;
        OracleModrm m;
        int source_size;
        if ((second != 0xb6 && second != 0xb7) ||
            decode_modrm(p, c, address16, &m) != 0 || !m.memory)
            return 0;
        p += m.length;
        source_size = second == 0xb6 ? 1 : 2;
        set_reg_part(c, m.reg, read_memory_width(m.address, source_size), width);
        break;
    }
    case 0xac: case 0xad: {                              /* LODS */
        uint32_t count = repeat ? string_index(c, 1, address16) : 1;
        int index_size = address16 ? 2 : 4;
        int step;
        width = opcode == 0xac ? 1 : width;
        step = (c->EFlags & 0x400u) ? -width : width;
        while (count) {
            uint32_t source = string_index(c, 6, address16);
            set_reg_part(c, 0, read_memory_width(source, width), width);
            advance_string_index(c, 6, address16, step);
            if (repeat) {
                count--;
                set_reg_part(c, 1, count, index_size);
            } else {
                count = 0;
            }
        }
        break;
    }
    case 0xa4: case 0xa5: case 0xaa: case 0xab: {
        int movs = opcode == 0xa4 || opcode == 0xa5;
        int stos = opcode == 0xaa || opcode == 0xab;
        uint32_t count = repeat ? string_index(c, 1, address16) : 1;
        int index_size = address16 ? 2 : 4;
        int step;
        width = (opcode == 0xa4 || opcode == 0xaa) ? 1 : width;
        step = (c->EFlags & 0x400u) ? -width : width;
        while (count) {
            uint32_t source = string_index(c, 6, address16), dest = string_index(c, 7, address16);
            if (movs) {
                uint32_t value = read_memory_width(source, width);
                write_memory_width(dest, value, width);
                advance_string_index(c, 6, address16, step);
            } else if (stos) {
                write_memory_width(dest, get_reg_part(c, 0, width), width);
            }
            advance_string_index(c, 7, address16, step);
            if (repeat) {
                count--;
                set_reg_part(c, 1, count, index_size);
            } else {
                count = 0;
            }
        }
        break;
    }
    case 0x80: case 0x81: case 0x83: {
        OracleModrm m;
        uint32_t immediate, left, result;
        int size = opcode == 0x80 ? 1 : width;
        if (decode_modrm(p, c, address16, &m) != 0 || !m.memory ||
            (m.reg != 1 && m.reg != 4 && m.reg != 6 && m.reg != 7))
            return 0;
        p += m.length;
        if (opcode == 0x80 || opcode == 0x83) {
            immediate = *p++;
            if (opcode == 0x83 && size != 1)
                immediate = (uint32_t)(int32_t)(int8_t)immediate;
        } else if (size == 2) {
            immediate = read_u16(p); p += 2;
        } else {
            immediate = read_u32(p); p += 4;
        }
        left = read_memory_width(m.address, size);
        if (m.reg == 7) {                                /* CMP r/m, imm */
            set_sub_flags(c, left, immediate, size);
            break;
        }
        result = logic_result(m.reg == 1 ? 4 : m.reg == 4 ? 1 : 6, left,
                              immediate & width_mask(size));
        write_memory_width(m.address, result, size);
        set_logic_flags(c, result, size);
        break;
    }
    case 0x38: case 0x39: case 0x3a: case 0x3b:         /* CMP r/m,r and r,r/m */
    case 0x84: case 0x85: {                              /* TEST r/m,r */
        OracleModrm m;
        uint32_t mem, reg;
        int byteop = !(opcode & 1);
        if (decode_modrm(p, c, address16, &m) != 0 || !m.memory)
            return 0;
        p += m.length;
        width = byteop ? 1 : width;
        mem = read_memory_width(m.address, width);
        reg = get_reg_part(c, m.reg, width);
        if (opcode == 0x84 || opcode == 0x85)
            set_logic_flags(c, mem & reg, width);
        else if (opcode & 2)
            set_sub_flags(c, reg, mem, width);
        else
            set_sub_flags(c, mem, reg, width);
        break;
    }
    case 0x08: case 0x09: case 0x0a: case 0x0b:
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x30: case 0x31: case 0x32: case 0x33: {
        OracleModrm m;
        uint32_t left, right, result;
        int operation = opcode & 0xf8, to_memory = !(opcode & 2), byteop = !(opcode & 1);
        if (decode_modrm(p, c, address16, &m) != 0 || !m.memory)
            return 0;
        p += m.length;
        width = byteop ? 1 : width;
        if (to_memory) {
            left = read_memory_width(m.address, width);
            right = get_reg_part(c, m.reg, width);
        } else {
            left = get_reg_part(c, m.reg, width);
            right = read_memory_width(m.address, width);
        }
        result = logic_result(operation == 0x08 ? 4 : operation == 0x20 ? 1 : 6, left, right);
        if (to_memory)
            write_memory_width(m.address, result, width);
        else
            set_reg_part(c, m.reg, result, width);
        set_logic_flags(c, result, width);
        break;
    }
    default:
        return 0;
    }
    len = (int)(p - start);
    c->Eip += (DWORD)len;
    vga_trace_caller = 0;
    return 1;
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

static void set_accumulator_part(DWORD *reg, uint32_t v, int size)
{
    if (size == 1) *reg = (*reg & ~0xffu) | (v & 0xff);
    else if (size == 2) *reg = (*reg & ~0xffffu) | (v & 0xffff);
    else *reg = v;
}

static int oracle_emulate_int(CONTEXT *c, const uint8_t *p, int prefix_len)
{
    union REGS r;
    struct SREGS s;
    memset(&s, 0, sizeof s);
    r.x.eax = c->Eax; r.x.ebx = c->Ebx; r.x.ecx = c->Ecx;
    r.x.edx = c->Edx; r.x.esi = c->Esi; r.x.edi = c->Edi;
    r.x.cflag = c->EFlags & 1;
    oracle_trace_add('N', p[0], c->Eax, 4);
    vhw_int(p[0], &r, &r, &s);
    c->Eax = r.x.eax; c->Ebx = r.x.ebx; c->Ecx = r.x.ecx;
    c->Edx = r.x.edx; c->Esi = r.x.esi; c->Edi = r.x.edi;
    c->EFlags = (c->EFlags & ~1u) | (r.x.cflag ? 1u : 0u);
    c->Eip += (DWORD)(prefix_len + 2);
    return 1;
}

/* ---- code patches for whole-program runs (port/oracle/lockstep.c) -------------------
 * Breakpoints: INT3 at an original instruction boundary. The VEH redirects the thread to
 * oracle_bp_thunk, which calls the registered function as ordinary code (not in exception
 * context) and returns into a trampoline that executes the displaced instruction bytes and
 * jumps back. The same thunk delivers pending virtual interrupts after emulated
 * IN/OUT/STI/INT when IRQ redirection is on (a CPU takes them at that boundary). */
typedef struct OracleBreakpoint {
    uint8_t *site;
    uint8_t *trampoline;
    void (*fn)(void);
    uint8_t emulate;       /* 9Ch PUSHFD / 9Dh POPFD emulated in the VEH, else 0 */
} OracleBreakpoint;
static OracleBreakpoint breakpoints[64];
static int breakpoint_count;
static uint8_t *trampoline_pool;
static int trampoline_used;
static int irq_redirect;
static uint32_t breakpoint_esp;
uint32_t oracle_breakpoint_esp(void) { return breakpoint_esp; }
void oracle_set_lowmem_emulation(int on) { lowmem_emulation_on = on; }

void oracle_bp_dispatch(void (*fn)(void));
void oracle_bp_thunk(void);
__asm__(
    ".text\n"
    ".globl _oracle_bp_thunk\n"
    "_oracle_bp_thunk:\n"
    "    pushfl\n"
    "    pushal\n"
    "    cld\n"
    "    movl 40(%esp), %eax\n"
    "    pushl %eax\n"
    "    call _oracle_bp_dispatch\n"
    "    addl $4, %esp\n"
    "    popal\n"
    "    popfl\n"
    "    ret $4\n"
    /* void oracle_call_iret_handler(uint32_t offset): IRETD frame for original handlers */
    ".globl _oracle_call_iret_handler\n"
    "_oracle_call_iret_handler:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    movl 8(%ebp), %eax\n"
    "    pushfl\n"
    "    pushl %cs\n"
    "    call *%eax\n"
    "    cld\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    popl %ebx\n"
    "    popl %ebp\n"
    "    ret\n");

static void oracle_deliver_irqs(void)
{
    if (vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
        vpic_deliver_pending();
}

void oracle_bp_dispatch(void (*fn)(void))
{
    fn();
}

void oracle_set_irq_redirect(int on) { irq_redirect = on; }

static void redirect_call(CONTEXT *c, void (*fn)(void), uint32_t resume)
{
    uint32_t *sp = (uint32_t *)(uintptr_t)c->Esp;
    *--sp = (uint32_t)(uintptr_t)fn;
    *--sp = resume;
    c->Esp = (DWORD)(uintptr_t)sp;
    c->Eip = (DWORD)(uintptr_t)oracle_bp_thunk;
}

/* After an emulated instruction: take a pending interrupt at this boundary. */
static void maybe_redirect_irq(CONTEXT *c)
{
    if (irq_redirect && vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
        redirect_call(c, oracle_deliver_irqs, c->Eip);
}

int oracle_breakpoint(uint32_t offset, const uint8_t *expect, int expect_len, int len,
                      void (*fn)(void))
{
    uint8_t *site = object_base[1] + offset, *t;
    DWORD old;
    int32_t rel;
    if (!object_base[1] || breakpoint_count >= 64 || len < 1 || len > 15 ||
        expect_len > len || memcmp(site, expect, (size_t)expect_len) != 0) {
        fprintf(stderr, "oracle: breakpoint site obj1+%05lx does not hold the expected bytes\n",
                (unsigned long)offset);
        return -1;
    }
    if (!trampoline_pool)
        trampoline_pool = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT,
                                       PAGE_EXECUTE_READWRITE);
    t = trampoline_pool + trampoline_used;
    trampoline_used += 32;
    memcpy(t, site, (size_t)len);      /* displaced instruction(s): no relative operands */
    t[len] = 0xe9;
    rel = (int32_t)((site + len) - (t + len + 5));
    memcpy(t + len + 1, &rel, 4);
    breakpoints[breakpoint_count].site = site;
    breakpoints[breakpoint_count].trampoline = t;
    breakpoints[breakpoint_count].fn = fn;
    breakpoints[breakpoint_count].emulate = 0;
    breakpoint_count++;
    VirtualProtect(site, 1, PAGE_EXECUTE_READWRITE, &old);
    *site = 0xcc;
    FlushInstructionCache(GetCurrentProcess(), site, 1);
    return 0;
}

/* PUSHFD/POPFD do not fault at CPL 3 and IOPL 0: POPFD silently ignores IF and PUSHFD shows
 * the host's IF/IOPL. Whole-program runs trap them (INT3) and use the virtual IF, with IOPL 3
 * as DOS/4GW runs its clients (the port's probe_cpu_environment reports 3 as well). */
int oracle_emulate_flags_instruction(uint32_t offset)
{
    uint8_t *site = object_base[1] + offset;
    DWORD old;
    if (!object_base[1] || breakpoint_count >= 64 || (*site != 0x9c && *site != 0x9d)) {
        fprintf(stderr, "oracle: obj1+%05lx is not PUSHFD/POPFD\n", (unsigned long)offset);
        return -1;
    }
    breakpoints[breakpoint_count].site = site;
    breakpoints[breakpoint_count].trampoline = NULL;
    breakpoints[breakpoint_count].fn = NULL;
    breakpoints[breakpoint_count].emulate = *site;
    breakpoint_count++;
    VirtualProtect(site, 1, PAGE_EXECUTE_READWRITE, &old);
    *site = 0xcc;
    FlushInstructionCache(GetCurrentProcess(), site, 1);
    return 0;
}

static void emulate_flags_instruction(CONTEXT *c, const OracleBreakpoint *bp)
{
    const DWORD writable = 0x00240dd5u;   /* CF PF AF ZF SF DF OF AC ID */
    uint32_t *sp = (uint32_t *)(uintptr_t)c->Esp;
    if (bp->emulate == 0x9c) {
        uint32_t v = (c->EFlags & ~0x3200u) | (vcpu_if_flag ? 0x200u : 0u) | 0x3000u;
        *--sp = v;
        c->Esp -= 4;
    } else {
        uint32_t v = *sp;
        c->Esp += 4;
        c->EFlags = (c->EFlags & ~writable) | (v & writable);
        if (v & 0x200u)
            vcpu_sti();
        else
            vcpu_cli();
    }
    c->Eip = (DWORD)(uintptr_t)(bp->site + 1);
    if (bp->emulate == 0x9d)
        maybe_redirect_irq(c);
}

/* Replace an original routine's entry with a jump to a host function of the same (cdecl)
 * signature: used for the Watcom C runtime entries that talk to DOS/DOS4GW. */
int oracle_patch_jump(uint32_t offset, void *target)
{
    uint8_t *site = object_base[1] + offset;
    int32_t rel = (int32_t)((uint8_t *)target - (site + 5));
    if (!object_base[1])
        return -1;
    site[0] = 0xe9;
    memcpy(site + 1, &rel, 4);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return 0;
}

/* The INT3 address is ExceptionAddress (the context EIP may already point past it). */
static int handle_breakpoint(CONTEXT *c, const void *address)
{
    int i;
    for (i = 0; i < breakpoint_count; i++)
        if ((const uint8_t *)address == breakpoints[i].site) {
            if (breakpoints[i].emulate) {
                emulate_flags_instruction(c, &breakpoints[i]);
                return 1;
            }
            breakpoint_esp = c->Esp;
            redirect_call(c, breakpoints[i].fn,
                          (uint32_t)(uintptr_t)breakpoints[i].trampoline);
            return 1;
        }
    return 0;
}

static LONG CALLBACK oracle_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
    int opsize = 4, len = 0;
    static int debug_veh = -1;
    if (debug_veh < 0)
        debug_veh = getenv("KE_ORACLE_DEBUG_VEH") != NULL;
    if (debug_veh && code != EXCEPTION_PRIV_INSTRUCTION)
        fprintf(stderr, "veh %08lx eip %08lx esp %08lx addr %08lx\n", (unsigned long)code,
                (unsigned long)c->Eip, (unsigned long)c->Esp,
                (unsigned long)(ep->ExceptionRecord->NumberParameters >= 2 ?
                                ep->ExceptionRecord->ExceptionInformation[1] : 0));
    if (code == EXCEPTION_BREAKPOINT && handle_breakpoint(c, ep->ExceptionRecord->ExceptionAddress))
        return EXCEPTION_CONTINUE_EXECUTION;
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        const uint8_t *opcode = p;
        int prefix_len = 0;
        if (!IsBadReadPtr(opcode, 1) && *opcode == 0x66) {
            opcode++;
            prefix_len = 1;
        }
        /* Some Windows builds report user-mode INT n as an access violation, not #GP. */
        if (!IsBadReadPtr(opcode, 2) && opcode[0] == 0xcd) {
            oracle_emulate_int(c, opcode + 1, prefix_len);
            maybe_redirect_irq(c);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (oracle_emulate_vga_memory(ep))
            return EXCEPTION_CONTINUE_EXECUTION;
        if (ep->ExceptionRecord->NumberParameters >= 2) {
            ULONG_PTR fault = ep->ExceptionRecord->ExceptionInformation[1];
            if (is_vga_address((uint32_t)fault)) {
                fprintf(stderr, "oracle: unhandled VGA access %s at %08lx from %08lx bytes",
                        ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
                        (unsigned long)fault, (unsigned long)c->Eip);
                if (!IsBadReadPtr(p, 8)) {
                    int i;
                    for (i = 0; i < 8; i++)
                        fprintf(stderr, " %02x", p[i]);
                }
                fprintf(stderr, " eax=%08lx ecx=%08lx edx=%08lx ebx=%08lx esi=%08lx edi=%08lx\n",
                        (unsigned long)c->Eax, (unsigned long)c->Ecx,
                        (unsigned long)c->Edx, (unsigned long)c->Ebx,
                        (unsigned long)c->Esi, (unsigned long)c->Edi);
            } else {
                fprintf(stderr, "oracle: unhandled access violation at %08lx from %08lx\n",
                        (unsigned long)fault, (unsigned long)c->Eip);
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code != EXCEPTION_PRIV_INSTRUCTION)
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
        set_accumulator_part(&c->Eax, v, size);
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
        vcpu_cli();
        oracle_trace_add('C', 0, 0, 0);
        len += 1;
        break;
    case 0xfb:
        vcpu_sti();
        oracle_trace_add('S', 0, 0, 0);
        len += 1;
        break;
    case 0xcd: {                                             /* INT nn */
        oracle_emulate_int(c, p + 1, len);
        maybe_redirect_irq(c);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    case 0x8e:                                               /* MOV Sreg, r/m16 */
        len += 1 + modrm_length(p + 1);
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
    c->Eip += (DWORD)len;
    maybe_redirect_irq(c);
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

typedef struct OracleVgaPatch {
    uint8_t object;
    uint32_t offset;
    uint32_t expected;
} OracleVgaPatch;

/* 32-bit absolute operands in the frozen LE object 1. The 0A0000h values point at the
 * aperture; 0B0000h values are its exclusive end in clear_video_bytes/mov_mem. Verify the
 * source bytes before patching so an image-layout change cannot silently corrupt code.
 */
static const OracleVgaPatch vga_patches[] = {
    {1, 0x0e10a, 0x0a0000}, {1, 0x0e16e, 0x0a0000},
    {1, 0x0e21b, 0x0a0000}, {1, 0x0e24f, 0x0a0000},
    {1, 0x13832, 0x0b0000}, {1, 0x1383a, 0x0a0000},
    {1, 0x138a7, 0x0b0000}, {1, 0x138af, 0x0a0000},
    {1, 0x138b7, 0x0b0000}, {1, 0x138bf, 0x0a0000},
    {1, 0x138ed, 0x0b0000}, {1, 0x138f5, 0x0a0000},
    /* set_display_mode: VGA_EXTENDED_MEMORY_BASE 280000h = 0A0000h * 4, the "chunky" page
     * origin of the unchained modes that the renderers shift right by 2. */
    {1, 0x0e235, 0x280000}
};

static int relocate_vga_operands(void)
{
    uint32_t i, base = (uint32_t)(uintptr_t)vga_guard;
    for (i = 0; i < sizeof vga_patches / sizeof vga_patches[0]; i++) {
        const OracleVgaPatch *patch = &vga_patches[i];
        uint32_t value, relocated;
        if (!object_base[patch->object] || patch->offset + 4 > object_size[patch->object]) {
            fprintf(stderr, "oracle: VGA relocation site obj%u+%05lx is outside the image\n",
                    patch->object, (unsigned long)patch->offset);
            return -1;
        }
        value = rd32(object_base[patch->object] + patch->offset);
        if (value != patch->expected) {
            fprintf(stderr, "oracle: VGA relocation site obj%u+%05lx expected %08lx, got %08lx\n",
                    patch->object, (unsigned long)patch->offset,
                    (unsigned long)patch->expected, (unsigned long)value);
            return -1;
        }
        relocated = patch->expected == 0x280000u ? base * 4u
                                                 : base + patch->expected - 0x0a0000u;
        memcpy(object_base[patch->object] + patch->offset, &relocated, sizeof relocated);
    }
    return 0;
}

int oracle_load(const char *image_path, const char *symbols_path)
{
    uint8_t *img;
    size_t size, pos = 12;
    uint32_t nobj, nfix, i;
    uint16_t ds_selector;
    FILE *sf;
    char line[256];
    /* Reserve the relocated VGA alias before allocating the image buffer; early heap
     * allocations can otherwise change which low address is available for the alias. */
    if (reserve_vga_window() != 0)
        return -1;
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
    if (relocate_vga_operands() != 0) {
        free(img);
        return -1;
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
    uint32_t args[6] = {0, 0, 0, 0, 0, 0};
    int i;
    if (!fn) {
        fprintf(stderr, "oracle_call: NULL function (symbol not in ke_symbols.txt?)\n");
        exit(3);
    }
    for (i = 0; a && i < argc && i < 6; i++)
        args[i] = relocate_vga_pointer(a[i]);
    switch (argc) {
    case 0: return ((F0)fn)();
    case 1: return ((F1)fn)(args[0]);
    case 2: return ((F2)fn)(args[0], args[1]);
    case 3: return ((F3)fn)(args[0], args[1], args[2]);
    case 4: return ((F4)fn)(args[0], args[1], args[2], args[3]);
    case 5: return ((F5)fn)(args[0], args[1], args[2], args[3], args[4]);
    default: return ((F6)fn)(args[0], args[1], args[2], args[3], args[4], args[5]);
    }
}

uint32_t oracle_port_call(void *fn, int argc, const uint32_t *a)
{
    typedef uint32_t (*F0)(void);
    typedef uint32_t (*F1)(uint32_t);
    typedef uint32_t (*F2)(uint32_t, uint32_t);
    typedef uint32_t (*F3)(uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*F4)(uint32_t, uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*F5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*F6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    if (!fn || argc < 0 || argc > 6 || (argc && !a)) {
        fprintf(stderr, "oracle_port_call: invalid function or argument count\n");
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

static void read_indexed_registers(uint16_t index_port, uint16_t data_port, unsigned count,
                                  uint8_t *dst, uint8_t *saved_index)
{
    unsigned i;
    *saved_index = (uint8_t)vhw_port_in(index_port, 1);
    for (i = 0; i < count; i++) {
        vhw_port_out(index_port, i, 1);
        dst[i] = (uint8_t)vhw_port_in(data_port, 1);
    }
    vhw_port_out(index_port, *saved_index, 1);
}

int oracle_vga_snapshot(OracleVgaSnapshot *snapshot)
{
    uint8_t rgb[256][3];
    unsigned plane, offset, i;
    if (!snapshot || !vga_guard)
        return -1;

    read_indexed_registers(0x3c4, 0x3c5, 8, snapshot->sequencer,
                           &snapshot->sequencer_index);
    read_indexed_registers(0x3ce, 0x3cf, 16, snapshot->graphics_controller,
                           &snapshot->graphics_index);
    read_indexed_registers(0x3d4, 0x3d5, 32, snapshot->crtc, &snapshot->crtc_index);
    snapshot->attribute_index = (uint8_t)vhw_port_in(0x3c0, 1);
    for (i = 0; i < 32; i++) {
        (void)vhw_port_in(0x3da, 1); /* reset the attribute-controller flip-flop */
        vhw_port_out(0x3c0, i | (snapshot->attribute_index & 0x20u), 1);
        snapshot->attribute[i] = (uint8_t)vhw_port_in(0x3c1, 1);
    }
    (void)vhw_port_in(0x3da, 1);
    vhw_port_out(0x3c0, snapshot->attribute_index, 1);
    (void)vhw_port_in(0x3da, 1); /* restore the index and leave it in index phase */
    snapshot->misc_output = (uint8_t)vhw_port_in(0x3cc, 1);
    vga_palette_rgb888(rgb);
    for (i = 0; i < 256; i++)
        for (plane = 0; plane < 3; plane++)
            snapshot->dac[i][plane] = rgb[i][plane] >> 2;

    /* Disable chain-4 and read mode 1 temporarily so each read-map value exposes one full
     * physical plane.  Restore the original register data and selectors after the scan.
     */
    vhw_port_out(0x3c4, 4, 1);
    vhw_port_out(0x3c5, snapshot->sequencer[4] & (uint8_t)~0x08u, 1);
    vhw_port_out(0x3ce, 5, 1);
    vhw_port_out(0x3cf, snapshot->graphics_controller[5] & (uint8_t)~0x08u, 1);
    for (plane = 0; plane < 4; plane++) {
        vhw_port_out(0x3ce, 4, 1);
        vhw_port_out(0x3cf, plane, 1);
        for (offset = 0; offset < ORACLE_VGA_PLANE_SIZE; offset++)
            snapshot->planes[plane][offset] = vga_mem_read8(0xA0000u + offset);
    }
    vhw_port_out(0x3c4, 4, 1);
    vhw_port_out(0x3c5, snapshot->sequencer[4], 1);
    vhw_port_out(0x3ce, 5, 1);
    vhw_port_out(0x3cf, snapshot->graphics_controller[5], 1);
    vhw_port_out(0x3ce, 4, 1);
    vhw_port_out(0x3cf, snapshot->graphics_controller[4], 1);
    vhw_port_out(0x3c4, snapshot->sequencer_index, 1);
    vhw_port_out(0x3ce, snapshot->graphics_index, 1);
    vhw_port_out(0x3d4, snapshot->crtc_index, 1);
    return 0;
}

static int compare_snapshot_bytes(const char *label, const uint8_t *a, const uint8_t *b,
                                  size_t size)
{
    size_t i;
    if (memcmp(a, b, size) == 0)
        return 0;
    for (i = 0; i < size; i++) {
        if (a[i] != b[i]) {
            printf("    VGA %s differs at +%lx: %02X != %02X\n", label,
                   (unsigned long)i, a[i], b[i]);
            break;
        }
    }
    return 1;
}

int oracle_vga_snapshot_equal(const OracleVgaSnapshot *a, const OracleVgaSnapshot *b,
                              const char *label)
{
    char block[32];
    int failures = 0;
    unsigned i;
    if (!a || !b)
        return 1;
    if (!label)
        label = "fixture";
    for (i = 0; i < 4; i++) {
        snprintf(block, sizeof block, "%s plane %u", label, i);
        failures += compare_snapshot_bytes(block, a->planes[i], b->planes[i],
                                           ORACLE_VGA_PLANE_SIZE);
    }
    failures += compare_snapshot_bytes("sequencer", a->sequencer, b->sequencer,
                                       sizeof a->sequencer);
    failures += compare_snapshot_bytes("graphics controller", a->graphics_controller,
                                       b->graphics_controller, sizeof a->graphics_controller);
    failures += compare_snapshot_bytes("CRTC", a->crtc, b->crtc, sizeof a->crtc);
    failures += compare_snapshot_bytes("attribute controller", a->attribute, b->attribute,
                                       sizeof a->attribute);
    failures += compare_snapshot_bytes("DAC", &a->dac[0][0], &b->dac[0][0], sizeof a->dac);
    if (a->misc_output != b->misc_output || a->sequencer_index != b->sequencer_index ||
        a->graphics_index != b->graphics_index || a->crtc_index != b->crtc_index ||
        a->attribute_index != b->attribute_index) {
        printf("    VGA %s index/misc registers differ\n", label);
        failures++;
    }
    return failures != 0;
}
