/* ke32_shims.c - the 64-bit side of the ILP32 game world boundary (Android).
 *
 * The historical game and the assembly translations are compiled with 32-bit pointers
 * (port/android/tools/ilp32_world.py). Every C-library call they make is renamed to one of
 * the ke32_* entry points below. Each takes its pointer arguments as uint32_t and returns
 * pointers as uint32_t, so no register bit above 31 is ever interpreted, whatever the
 * 32-bit code generator leaves there. The functions keep the Watcom 10.0 semantics the
 * Windows build gets from msvcrt (lower-case itoa digits, 32-bit strtoul, _rotl, ...).
 *
 * Everything the game can point at is below 2 GB (the game library, its heap arena, the
 * game thread's stack, DOS memory): docs/android/architecture.md, "64-bit".
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../../include/ke_port.h"
#include "../../include/watcom/i86.h"
#include "../../platform/ke_platform.h"

#define P(v) ((void *)(uintptr_t)(uint32_t)(v))
#define CP(v) ((const char *)(uintptr_t)(uint32_t)(v))
#define U32(p) ((uint32_t)(uintptr_t)(p))

FILE *ke_fopen(const char *path, const char *mode);
char *ke_getenv(const char *name);

/* ---- low heap arena ---------------------------------------------------------------------
 * First fit with boundary headers and coalescing. The arena lives in the low game region
 * the loader reserved (ke32_set_heap); the game allocates a few large blocks (its 350 KiB
 * work area, GIF decode workspaces, packed images). */
typedef struct Block {
    uint32_t size;          /* payload bytes, multiple of 16 */
    uint32_t used;
    uint32_t prev_size;     /* payload bytes of the previous block, 0 for the first */
    uint32_t magic;
} Block;
#define BLOCK_MAGIC 0x4b453332u      /* "KE32" */

static uint8_t *heap_base, *heap_end;
static KeMutex heap_lock;
static int heap_ready;
static size_t heap_in_use, heap_peak;

void ke32_set_heap(void *base, size_t size)
{
    Block *first;
    heap_base = (uint8_t *)(((uintptr_t)base + 15) & ~(uintptr_t)15);
    heap_end = (uint8_t *)base + size;
    first = (Block *)heap_base;
    first->size = (uint32_t)((heap_end - heap_base - sizeof(Block)) & ~(size_t)15);
    first->used = 0;
    first->prev_size = 0;
    first->magic = BLOCK_MAGIC;
    ke_mutex_init(&heap_lock);
    heap_ready = 1;
    ke_log(KE_LOG_INFO, "ke32", "game heap %p..%p (%zu KiB)", (void *)heap_base, (void *)heap_end,
           (size_t)(heap_end - heap_base) >> 10);
}

static Block *next_block(Block *b)
{
    uint8_t *next = (uint8_t *)(b + 1) + b->size;
    return next + sizeof(Block) <= heap_end ? (Block *)next : NULL;
}

uint32_t ke32_malloc(uint32_t size)
{
    Block *b;
    uint32_t need;
    void *result = NULL;
    if (!heap_ready || size == 0 || size > 0x7fff0000u)
        return 0;
    need = (size + 15u) & ~15u;
    ke_mutex_lock(&heap_lock);
    for (b = (Block *)heap_base; b; b = next_block(b)) {
        if (b->used || b->size < need)
            continue;
        if (b->size >= need + sizeof(Block) + 16) {
            Block *rest = (Block *)((uint8_t *)(b + 1) + need), *after;
            rest->size = b->size - need - (uint32_t)sizeof(Block);
            rest->used = 0;
            rest->prev_size = need;
            rest->magic = BLOCK_MAGIC;
            after = next_block(rest);
            if (after)
                after->prev_size = rest->size;
            b->size = need;
        }
        b->used = 1;
        heap_in_use += b->size;
        if (heap_in_use > heap_peak)
            heap_peak = heap_in_use;
        result = b + 1;
        break;
    }
    ke_mutex_unlock(&heap_lock);
    if (!result)
        ke_log(KE_LOG_WARN, "ke32", "game heap exhausted: malloc(%u) failed", size);
    return U32(result);
}

void ke32_free(uint32_t p)
{
    Block *b, *next, *prev;
    if (!p || !heap_ready)
        return;
    b = (Block *)P(p) - 1;
    if ((uint8_t *)b < heap_base || (uint8_t *)b >= heap_end || b->magic != BLOCK_MAGIC || !b->used) {
        ke_log(KE_LOG_WARN, "ke32", "free(%08X): not a live game heap block", p);
        return;
    }
    ke_mutex_lock(&heap_lock);
    b->used = 0;
    heap_in_use -= b->size;
    next = next_block(b);
    if (next && !next->used) {
        b->size += (uint32_t)sizeof(Block) + next->size;
        next->magic = 0;
        next = next_block(b);
        if (next)
            next->prev_size = b->size;
    }
    if ((uint8_t *)b > heap_base) {
        prev = (Block *)((uint8_t *)b - b->prev_size - sizeof(Block));
        if (!prev->used) {
            prev->size += (uint32_t)sizeof(Block) + b->size;
            b->magic = 0;
            if (next)
                next->prev_size = prev->size;
        }
    }
    ke_mutex_unlock(&heap_lock);
}

/* ---- memory and strings ---------------------------------------------------------------- */
uint32_t ke32_memcpy(uint32_t d, uint32_t s, uint32_t n) { return U32(memcpy(P(d), P(s), n)); }
uint32_t ke32_memmove(uint32_t d, uint32_t s, uint32_t n) { return U32(memmove(P(d), P(s), n)); }
uint32_t ke32_memset(uint32_t d, int c, uint32_t n) { return U32(memset(P(d), c, n)); }
int ke32_memcmp(uint32_t a, uint32_t b, uint32_t n) { return memcmp(P(a), P(b), n); }
uint32_t ke32_strcpy(uint32_t d, uint32_t s) { return U32(strcpy((char *)P(d), CP(s))); }
uint32_t ke32_strcat(uint32_t d, uint32_t s) { return U32(strcat((char *)P(d), CP(s))); }
uint32_t ke32_strlen(uint32_t s) { return (uint32_t)strlen(CP(s)); }
int ke32_strcmp(uint32_t a, uint32_t b) { return strcmp(CP(a), CP(b)); }
uint32_t ke32_strchr(uint32_t s, int c) { return U32(strchr(CP(s), c)); }
int ke32_stricmp(uint32_t a, uint32_t b) { return strcasecmp(CP(a), CP(b)); }
int ke32_abs(int v) { return v < 0 ? -v : v; }

/* msvcrt/Watcom: radix 10 signed, other radixes the unsigned 32-bit value, lower-case. */
static char *convert(uint32_t value, int negative, char *buffer, int radix)
{
    char tmp[40];
    int n = 0, i = 0;
    if (radix < 2 || radix > 36) {
        buffer[0] = 0;
        return buffer;
    }
    do {
        unsigned digit = value % (unsigned)radix;
        tmp[n++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= (unsigned)radix;
    } while (value);
    if (negative)
        buffer[i++] = '-';
    while (n)
        buffer[i++] = tmp[--n];
    buffer[i] = 0;
    return buffer;
}

uint32_t ke32_itoa(int value, uint32_t buffer, int radix)
{
    if (radix == 10 && value < 0)
        return U32(convert((uint32_t)-(int64_t)value, 1, (char *)P(buffer), radix));
    return U32(convert((uint32_t)value, 0, (char *)P(buffer), radix));
}

uint32_t ke32_ltoa(int value, uint32_t buffer, int radix)
{
    return ke32_itoa(value, buffer, radix);  /* Watcom 386: long is 32-bit */
}

uint32_t ke32_strtoul(uint32_t s, uint32_t endp, int base)
{
    char *end;
    unsigned long long value;
    const char *text = CP(s);
    const char *p = text;
    errno = 0;
    value = strtoull(text, &end, base);
    while (*p == ' ' || (*p >= '\t' && *p <= '\r'))
        p++;
    if (*p != '-' && value > 0xffffffffull) {     /* 32-bit unsigned long saturates */
        value = 0xffffffffull;
        errno = ERANGE;
    }
    if (endp)
        *(uint32_t *)P(endp) = U32(end);
    return (uint32_t)value;
}

uint32_t ke32_rotl(uint32_t value, int shift)
{
    shift &= 31;
    return shift ? (value << shift) | (value >> (32 - shift)) : value;
}

/* _splitpath: "C:\DIR\NAME.EXT" -> "C:", "\DIR\", "NAME", ".EXT" (any part may be NULL). */
void ke32_splitpath(uint32_t path32, uint32_t drive32, uint32_t dir32, uint32_t name32,
                    uint32_t ext32)
{
    const char *path = CP(path32), *p = path, *last_sep = NULL, *dot = NULL, *end;
    char *drive = (char *)P(drive32), *dir = (char *)P(dir32);
    char *name = (char *)P(name32), *ext = (char *)P(ext32);
    if (path[0] && path[1] == ':') {
        if (drive) {
            drive[0] = path[0];
            drive[1] = ':';
            drive[2] = 0;
        }
        p += 2;
    } else if (drive) {
        drive[0] = 0;
    }
    for (end = p; *end; end++) {
        if (*end == '\\' || *end == '/')
            last_sep = end, dot = NULL;
        else if (*end == '.')
            dot = end;
    }
    if (dir) {
        size_t n = last_sep ? (size_t)(last_sep + 1 - p) : 0;
        memcpy(dir, p, n);
        dir[n] = 0;
    }
    p = last_sep ? last_sep + 1 : p;
    if (!dot)
        dot = end;
    if (name) {
        memcpy(name, p, (size_t)(dot - p));
        name[dot - p] = 0;
    }
    if (ext) {
        memcpy(ext, dot, (size_t)(end - dot));
        ext[end - dot] = 0;
    }
}

/* ---- files: FILE* of the game world are addresses of low slots ------------------------- */
#define KE32_FILES 16
static FILE *file_slots[KE32_FILES];

static FILE *slot_file(uint32_t handle)
{
    FILE **slot = (FILE **)P(handle);
    if (slot < file_slots || slot >= file_slots + KE32_FILES)
        return NULL;
    return *slot;
}

uint32_t ke32_fopen(uint32_t path, uint32_t mode)
{
    int i;
    FILE *file;
    for (i = 0; i < KE32_FILES && file_slots[i]; i++)
        ;
    if (i == KE32_FILES) {
        errno = EMFILE;
        return 0;
    }
    file = ke_fopen(CP(path), CP(mode));
    if (!file)
        return 0;
    file_slots[i] = file;
    return U32(&file_slots[i]);
}

int ke32_fclose(uint32_t handle)
{
    FILE *file = slot_file(handle);
    if (!file)
        return EOF;
    *(FILE **)P(handle) = NULL;
    return fclose(file);
}

uint32_t ke32_fread(uint32_t p, uint32_t size, uint32_t n, uint32_t handle)
{
    FILE *file = slot_file(handle);
    return file ? (uint32_t)fread(P(p), size, n, file) : 0;
}

uint32_t ke32_fwrite(uint32_t p, uint32_t size, uint32_t n, uint32_t handle)
{
    FILE *file = slot_file(handle);
    return file ? (uint32_t)fwrite(P(p), size, n, file) : 0;
}

int ke32_fseek(uint32_t handle, int offset, int whence)
{
    FILE *file = slot_file(handle);
    return file ? fseek(file, (long)offset, whence) : -1;
}

int ke32_ftell(uint32_t handle)
{
    FILE *file = slot_file(handle);
    long position = file ? ftell(file) : -1L;
    return position > 0x7fffffffL ? -1 : (int)position;
}

int ke32_fflush(uint32_t handle)
{
    FILE *file = handle ? slot_file(handle) : NULL;
    return fflush(file);
}

/* ---- virtual PC and runtime entry points with pointer arguments ------------------------ */
int ke32_int386(int intno, uint32_t in, uint32_t out) { return int386(intno, P(in), P(out)); }

int ke32_int386x(int intno, uint32_t in, uint32_t out, uint32_t sregs)
{
    return int386x(intno, P(in), P(out), P(sregs));
}

void ke32_segread(uint32_t sregs) { segread((struct SREGS *)P(sregs)); }
int ke32_atexit(uint32_t fn) { return ke_atexit((void (*)(void))P(fn)); }
uint32_t ke32_getenv(uint32_t name) { return U32(ke_getenv(CP(name))); }
void ke32_stub_hit(uint32_t name, uint32_t owner) { ke_stub_hit(CP(name), CP(owner)); }

/* printf output of the game world (its startup report, fatal messages): the DOS console. */
void ke32_host_write(uint32_t text, int length)
{
    char line[512];
    const char *s = CP(text);
    int start = 0, i;
    fwrite(s, 1, (size_t)length, stdout);
    for (i = 0; i <= length; i++) {
        if (i == length || s[i] == '\n') {
            int n = i - start;
            if (n > (int)sizeof line - 1)
                n = (int)sizeof line - 1;
            if (n > 0 && s[start + n - 1] == '\r')
                n--;
            if (n > 0) {
                memcpy(line, s + start, (size_t)n);
                line[n] = 0;
                ke_log(KE_LOG_INFO, "console", "%s", line);
            }
            start = i + 1;
        }
    }
}

void ke32_spawn_refused(uint32_t path)
{
    ke_log(KE_LOG_WARN, "clib", "spawnlp(\"%s\") refused: the port does not run DOS programs",
           path ? CP(path) : "");
}

/* arm64_32 code calls ___chkstk_darwin before a frame larger than a page; the game stack
 * is fully committed memory, so there is nothing to probe. */
#if defined(__aarch64__)
__asm__(".text\n.globl ke32_chkstk_darwin\n.hidden ke32_chkstk_darwin\n"
        ".type ke32_chkstk_darwin,%function\nke32_chkstk_darwin:\n\tret\n");
#endif

/* ---- self-relocation --------------------------------------------------------------------
 * 32-bit absolute pointers in the game world's initialized data (string tables, sprite
 * frame tables, hooks). The build left the slots zero and listed them in section
 * ke32_relocs as {slot address, target address} pairs (64-bit dynamic relocations the
 * Android linker applies); store each target as a 32-bit value once, before the game runs. */
extern const uint64_t __start_ke32_relocs[] __attribute__((weak, visibility("hidden")));
extern const uint64_t __stop_ke32_relocs[] __attribute__((weak, visibility("hidden")));

int ke32_apply_relocs(void)
{
    const uint64_t *r;
    size_t count = 0;
    for (r = __start_ke32_relocs; r && r + 1 < __stop_ke32_relocs + 1 && r < __stop_ke32_relocs; r += 2) {
        uint64_t slot = r[0], target = r[1];
        if (target > 0x7fffffffull || slot > 0x7fffffffull) {
            ke_log(KE_LOG_ERROR, "ke32", "relocation target %llx for slot %llx is not below 2 GB",
                   (unsigned long long)target, (unsigned long long)slot);
            return -1;
        }
        *(uint32_t *)(uintptr_t)slot = (uint32_t)target;
        count++;
    }
    ke_log(KE_LOG_INFO, "ke32", "%zu 32-bit data pointers relocated", count);
    return 0;
}
