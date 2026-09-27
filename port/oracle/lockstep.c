/* lockstep.c - ke_lockstep.exe: run the whole game - the port or the ORIGINAL KE.EXE machine
 * code - on the deterministic lockstep virtual PC and dump machine state at every frame.
 *
 *   ke_lockstep.exe --mode port|orig --image DIR --data DIR --out FILE.lsd
 *                   [--replay FILE.kereplay] [--frames N] [--portmap FILE] [--full-at a,b,..]
 *                   [--idle-keys 39,b9,...] [--log FILE]
 *
 * Both modes use the same executable, the same vhw devices (vhw_lockstep = 1: time advances
 * only by emulated events, no device threads, synchronous interrupts) and the same input
 * source (port/host/replay.c, keyed by the Nth call of wait_for_tick). Differences between
 * the two dumps are therefore differences between the port's code and the original's.
 *
 * Original mode (docs/port/lockstep.md): oracle_load() maps KE.EXE's LE objects and applies
 * its fixups; the Watcom C runtime entries that talk to DOS/DOS4GW (malloc/free, stdio,
 * exit/atexit, getenv, kbhit/getch, int386/int386x, printf, spawnlp) jump to the port's host
 * implementations, so both runs see the same files, heap and DOS services; everything else
 * - game C code, assembly, pure clib routines - is the original machine code. IN/OUT, CLI/STI,
 * INT n, VGA window and real-mode low-memory accesses are emulated by the oracle VEH on the
 * same vhw devices. Two INT3 breakpoints mirror the port's hooks: wait_for_tick entry (the
 * frame hook) and the timer-installed memory poll (vhw_cpu_poll_yield, a PORT: line in t06.c).
 * Interrupt handlers are called through an IRETD frame; pending IRQs are taken after each
 * emulated instruction as the port takes them at vhw_leave().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <windows.h>
#include "oracle.h"
#include "../vhw/vhw.h"
#include "../include/ke_port.h"
#include "../host/replay.h"

#define DG_SIZE 0xE610u           /* DGROUP up to the STACK segment (runtime data_layout) */
#define PLANE_BLOCK 1024u
#define PLANE_BLOCKS (0x40000u / PLANE_BLOCK)
#define LS_HEAP_BASE 0x20000000u
#define LS_HEAP_SIZE (96u << 20)

/* ---- deterministic heap (identical addresses in both runs) ------------------------------ */
static uint8_t *heap;
static uint32_t heap_top;
static struct { uint32_t off, size; uint8_t freed; } heap_blocks[4096];
static int heap_count;

static int heap_init(void)
{
    /* the parent reserved the range in the suspended child (relaunch below) */
    heap = (uint8_t *)VirtualAlloc((void *)LS_HEAP_BASE, LS_HEAP_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if (heap != (uint8_t *)LS_HEAP_BASE)
        heap = (uint8_t *)VirtualAlloc((void *)LS_HEAP_BASE, LS_HEAP_SIZE,
                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    return heap == (uint8_t *)LS_HEAP_BASE ? 0 : -1;
}

static int in_heap(const void *p)
{
    return heap && (const uint8_t *)p >= heap && (const uint8_t *)p < heap + LS_HEAP_SIZE;
}

void *ls_malloc(size_t n)
{
    uint32_t size = (uint32_t)((n + 15u) & ~15u);
    void *p;
    if (!heap || heap_count >= 4096 || size > LS_HEAP_SIZE - heap_top || n == 0)
        return NULL;
    p = heap + heap_top;
    heap_blocks[heap_count].off = heap_top;
    heap_blocks[heap_count].size = size;
    heap_blocks[heap_count].freed = 0;
    heap_count++;
    heap_top += size;
    memset(p, 0, size);
    return p;
}

void ls_free(void *p)
{
    int i;
    if (!in_heap(p))
        return;
    for (i = heap_count - 1; i >= 0; i--)
        if (heap + heap_blocks[i].off == (uint8_t *)p) {
            heap_blocks[i].freed = 1;
            break;
        }
    while (heap_count > 0 && heap_blocks[heap_count - 1].freed) {
        heap_count--;
        heap_top = heap_blocks[heap_count].off;
    }
}

/* Port mode: -Wl,--wrap=malloc,--wrap=free. Only the game's alloc_heap_block/free_heap_block
 * (u_0ddb9.c, the single malloc/free call sites of the historical code) use the arena. */
void *__real_malloc(size_t n);
void __real_free(void *p);
void *alloc_heap_block(int byte_count);
void free_heap_block(void *p);
static int heap_active;

void *__wrap_malloc(size_t n)
{
    uintptr_t ra = (uintptr_t)__builtin_return_address(0);
    if (heap_active && ra >= (uintptr_t)alloc_heap_block && ra < (uintptr_t)free_heap_block)
        return ls_malloc(n);
    return __real_malloc(n);
}

void __wrap_free(void *p)
{
    if (in_heap(p))
        ls_free(p);
    else
        __real_free(p);
}

static uint64_t lockstep_clock(void) { return vhw_lockstep_ns; }

/* ---- options / dump ----------------------------------------------------------------------- */
static int mode_orig;
static const char *out_path, *portmap_path, *replay_path, *log_path;
static const char *image_dir = "build/port/oracle", *data_dir = "assets";
static unsigned max_frames = 0xffffffffu;
static unsigned full_at[64];
static int full_at_count;
static uint8_t idle_keys[64];
static int idle_key_count, idle_key_next;
static unsigned idle_calls_without_input;
static FILE *dump, *io_trace;
static uint32_t io_trace_frame_limit = 0xffffffffu, io_trace_frame_first;
static unsigned frame_no;
static uint8_t dg[DG_SIZE], dg_mask[DG_SIZE];
static uint8_t planes[0x40000];
static struct { uint32_t orig_off, len; uint32_t port_addr; } spans[4096];
static int span_count;

static int load_portmap(const char *path)
{
    FILE *f = fopen(path, "r");
    char name[128];
    unsigned off, len, addr;
    if (!f)
        return -1;
    while (span_count < 4096 && fscanf(f, "%x %x %x %127s", &off, &len, &addr, name) == 4) {
        if (off + len > DG_SIZE)
            continue;
        spans[span_count].orig_off = off;
        spans[span_count].len = len;
        spans[span_count].port_addr = addr;
        memset(dg_mask + off, 1, len);
        span_count++;
    }
    fclose(f);
    return 0;
}

static uint32_t fnv1a(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

static void capture_dgroup(void)
{
    int i;
    if (mode_orig) {
        memcpy(dg, oracle_object_base(3), DG_SIZE);
        return;
    }
    memset(dg, 0, sizeof dg);
    for (i = 0; i < span_count; i++)
        memcpy(dg + spans[i].orig_off, (const void *)(uintptr_t)spans[i].port_addr, spans[i].len);
}

/* Record layout (little endian), see port/tools/lockstep.py:
 *   "LSF1" u32 frame u64 clock_ns u32 wait_for_tick_count u8 pic[10] u8 pad[2] u32 pit[16]
 *   u8 vga_regs[VGA_DEBUG_REGS] u8 dac[768] u32 plane_hash[256] u32 heap_top u32 heap_hash
 *   u8 lowmem_bda[0x100] (0x400..0x4FF) u8 full u8 pad[3] [planes 4x64K when full]
 *   u8 dgroup[DG_SIZE] */
static void write_frame(void)
{
    uint8_t pic[10], pad[3] = {0, 0, 0}, regs[VGA_DEBUG_REGS], dac[768];
    uint32_t pit[16], hashes[PLANE_BLOCKS], v;
    uint64_t clock = vhw_lockstep_ns;
    unsigned i;
    uint8_t full = 0;
    if (!dump)
        return;
    for (i = 0; i < (unsigned)full_at_count; i++)
        if (full_at[i] == frame_no)
            full = 1;
    vpic_debug_state(pic);
    vpit_debug_state(pit);
    vga_debug_state(planes, regs, dac);
    for (i = 0; i < PLANE_BLOCKS; i++)
        hashes[i] = fnv1a(planes + i * PLANE_BLOCK, PLANE_BLOCK);
    capture_dgroup();
    fwrite("LSF1", 1, 4, dump);
    fwrite(&frame_no, 4, 1, dump);
    fwrite(&clock, 8, 1, dump);
    v = ke_wait_for_tick_count();
    fwrite(&v, 4, 1, dump);
    fwrite(pic, 1, 10, dump);
    fwrite(pad, 1, 2, dump);
    fwrite(pit, 4, 16, dump);
    fwrite(regs, 1, sizeof regs, dump);
    fwrite(dac, 1, sizeof dac, dump);
    fwrite(hashes, 4, PLANE_BLOCKS, dump);
    fwrite(&heap_top, 4, 1, dump);
    v = heap ? fnv1a(heap, heap_top) : 0;
    fwrite(&v, 4, 1, dump);
    fwrite(ke_lowmem_shadow + 0x400, 1, 0x100, dump);
    fwrite(&full, 1, 1, dump);
    fwrite(pad, 1, 3, dump);
    if (full)
        fwrite(planes, 1, sizeof planes, dump);
    fwrite(dg, 1, DG_SIZE, dump);
}

static void finish(const char *why)
{
    if (io_trace) {
        fclose(io_trace);
        io_trace = NULL;
    }
    if (dump) {
        fclose(dump);
        dump = NULL;
    }
    printf("lockstep: %s mode stopped after %u frame entries (%s), clock %.6f s\n",
           mode_orig ? "orig" : "port", frame_no, why, (double)vhw_lockstep_ns / 1e9);
    fflush(stdout);
    ke_log(KE_LOG_INFO, "lockstep", "stopped after %u frame entries (%s)", frame_no, why);
}

static void trace_io(char kind, uint16_t port, uint32_t value)
{
    if (io_trace && frame_no <= io_trace_frame_limit && frame_no >= io_trace_frame_first)
        fprintf(io_trace, "%u %llu %c %04x %x\n", frame_no, (unsigned long long)vhw_lockstep_ns,
                kind, port, value);
}

static void trace_vga_write(uint32_t linear, uint8_t value, void *caller)
{
    uint8_t regs[VGA_DEBUG_REGS];
    if (!io_trace || frame_no > io_trace_frame_limit || frame_no < io_trace_frame_first)
        return;
    vga_debug_state(NULL, regs, NULL);
    /* W frame address value seq_mapmask gc_mode gc_bitmask caller */
    fprintf(io_trace, "%u W %05lx %02x m%x g%02x b%02x @%08lx\n", frame_no, (unsigned long)linear,
            value, regs[2] & 15, regs[8 + 5], regs[8 + 8], (unsigned long)(uintptr_t)caller);
}

/* The frame hook: state at the Nth wait_for_tick entry, then the replay's input for N,
 * then pending interrupts (the input's keyboard IRQ) at this instruction boundary. */
static void lockstep_frame(void)
{
    if (frame_no >= max_frames) {
        finish("frame limit");
        ExitProcess(0);
    }
    write_frame();
    ke_replay_frame_entry();
    frame_no++;
    if (vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
        vpic_deliver_pending();
}

/* Blocking BIOS keyboard wait (getch) with an empty buffer: feed the scripted idle keys. */
static void lockstep_idle(void)
{
    if (vkbd_bios_kbhit()) {
        idle_calls_without_input = 0;
        return;
    }
    if (idle_key_next < idle_key_count) {
        ke_input_push_scancode(idle_keys[idle_key_next++]);
        if (idle_key_next < idle_key_count && (idle_keys[idle_key_next] & 0x80))
            ke_input_push_scancode(idle_keys[idle_key_next++]);
        idle_calls_without_input = 0;
        return;
    }
    if (++idle_calls_without_input > 2000) {
        finish("blocked in a BIOS keyboard wait without scripted input");
        ExitProcess(4);
    }
}

/* ---- --watch ADDR: hardware write watchpoint (debug registers) on the game thread ------- */
static uint32_t watch_addr, watch_from;
static HANDLE watch_target;

static LONG CALLBACK watch_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    uint32_t ebp, n;
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(c->Dr6 & 1))
        return EXCEPTION_CONTINUE_SEARCH;
    c->Dr6 = 0;
    if (frame_no >= watch_from) {
        fprintf(stderr, "watch: frame %u write %08lx = %08lx at EIP %08lx; callers",
                frame_no, (unsigned long)watch_addr,
                (unsigned long)*(volatile uint32_t *)(uintptr_t)watch_addr, (unsigned long)c->Eip);
        for (ebp = c->Ebp, n = 0; n < 6 && ebp && !IsBadReadPtr((void *)(uintptr_t)ebp, 8); n++) {
            fprintf(stderr, " %08lx", (unsigned long)((uint32_t *)(uintptr_t)ebp)[1]);
            if (((uint32_t *)(uintptr_t)ebp)[0] <= ebp)
                break;
            ebp = ((uint32_t *)(uintptr_t)ebp)[0];
        }
        fprintf(stderr, "\n");
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static DWORD WINAPI watch_arm(LPVOID unused)
{
    CONTEXT c;
    (void)unused;
    SuspendThread(watch_target);
    memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(watch_target, &c);
    c.Dr0 = watch_addr;
    c.Dr7 = 1u | (1u << 16) | (3u << 18);   /* L0, break on write, 4 bytes */
    SetThreadContext(watch_target, &c);
    ResumeThread(watch_target);
    return 0;
}

static void watch_install(void)
{
    HANDLE t;
    if (!watch_addr)
        return;
    AddVectoredExceptionHandler(1, watch_veh);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &watch_target,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    t = CreateThread(NULL, 0, watch_arm, NULL, 0, NULL);
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

/* ---- original-mode setup ------------------------------------------------------------------ */
static void orig_isr_invoke(uint32_t offset)
{
    oracle_call_iret_handler(offset);
}

/* The port's frame hook is the linker wrapper of wait_for_tick, which (GNU ld --wrap) does
 * not see start_timer's two calls inside t06.c itself; skip the same calls here so the Nth
 * frame entry means the same call in both runs. */
static void orig_frame(void)
{
    uint32_t ra = *(uint32_t *)(uintptr_t)oracle_breakpoint_esp();
    uint32_t base = (uint32_t)(uintptr_t)oracle_object_base(1);
    if (ra >= base + 0x9b44 && ra < base + 0x9c90)      /* start_timer */
        return;
    lockstep_frame();
}

static void orig_main(void)
{
    ((void (*)(void))((uint8_t *)oracle_object_base(1) + 0x13a95))();
}

extern int int386(int, void *, void *);
extern int int386x(int, void *, void *, void *);
extern int spawnlp(int mode, const char *path, const char *arg0, ...);
char *ke_getenv(const char *name);
FILE *ke_fopen(const char *path, const char *mode);

static int setup_original(void)
{
    /* LE object 1 offsets of the Watcom 10.0 clib entries (manifest.json runtime publics)
     * reached from game code (tools: every call from a manifest function into the runtime). */
    static const struct { uint32_t off; const char *name; } crt[] = {
        {0x14197, "atexit"}, {0x1515a, "exit"}, {0x14996, "fopen"}, {0x14c13, "fclose"},
        {0x14d96, "fread"}, {0x14a8c, "fseek"}, {0x14bdf, "ftell"}, {0x14f47, "fwrite"},
        {0x14705, "free"}, {0x14609, "malloc"}, {0x14793, "getch"}, {0x1477c, "kbhit"},
        {0x1427c, "getenv"}, {0x145d0, "int386"}, {0x145ad, "int386x"}, {0x15136, "printf"},
        {0x13b9c, "spawnlp"},
    };
    void *targets[] = {
        (void *)ke_atexit, (void *)ke_exit, (void *)fopen, (void *)fclose,
        (void *)fread, (void *)fseek, (void *)ftell, (void *)fwrite,
        (void *)ls_free, (void *)ls_malloc, (void *)getch, (void *)kbhit,
        (void *)ke_getenv, (void *)int386, (void *)int386x, (void *)printf,
        (void *)spawnlp,
    };
    static const uint8_t push_ebx[] = {0x53};
    static const uint8_t inc_mem[] = {0xff, 0x05};
    unsigned i;
    uint16_t cs;
    for (i = 0; i < sizeof crt / sizeof crt[0]; i++)
        if (oracle_patch_jump(crt[i].off, targets[i]) != 0)
            return -1;
    /* wait_for_tick (obj1 09D40h): entry `push ebx`; memory poll body `inc [retrace_spin_count8]`
     * at 09DBCh, where t06.c's PORT: line calls vhw_cpu_poll_yield(). */
    if (oracle_breakpoint(0x9d40, push_ebx, 1, 1, orig_frame) != 0 ||
        oracle_breakpoint(0x9dbc, inc_mem, 2, 6, vhw_cpu_poll_yield) != 0)
        return -1;
    /* PUSHFD/POPFD of the assembly modules (measure_pit_channel0, set_pit_channel0_reload,
     * probe_cpu_environment): capstone scan of every manifest function. */
    {
        static const uint32_t flag_sites[] = {
            0x09f69, 0x0a037, 0x0a045, 0x0a062, 0x137b8, 0x137bf, 0x137ca, 0x137cb,
            0x137d1, 0x137fe, 0x13811,
        };
        for (i = 0; i < sizeof flag_sites / sizeof flag_sites[0]; i++)
            if (oracle_emulate_flags_instruction(flag_sites[i]) != 0)
                return -1;
    }
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    vpic_extra_code_selector = cs;
    vpic_isr_invoker = orig_isr_invoke;
    oracle_set_irq_redirect(1);
    oracle_set_lowmem_emulation(1);
    return 0;
}

/* ---- process setup ------------------------------------------------------------------------ */
static int relaunch_with_low_memory_reserved(int *exit_code)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR path[MAX_PATH];
    DWORD code = 1;
    char child[8];
    if (GetEnvironmentVariableA("KE_ORACLE_CHILD", child, sizeof child) != 0)
        return 0;
    SetEnvironmentVariableA("KE_ORACLE_CHILD", "1");
    if (!GetModuleFileNameW(NULL, path, MAX_PATH))
        return -1;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessW(path, GetCommandLineW(), NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL,
                        &si, &pi))
        return -1;
    /* The legacy VGA window must stay unmapped (no-access) so every original access to it
     * faults into the VGA emulation; without this the loader may place data there. */
    VirtualAllocEx(pi.hProcess, (void *)0xA0000, 0x20000, MEM_RESERVE, PAGE_NOACCESS);
    if (VirtualAllocEx(pi.hProcess, (void *)LOWMEM_BASE, LOWMEM_END - LOWMEM_BASE, MEM_RESERVE,
                       PAGE_READWRITE) != (void *)LOWMEM_BASE ||
        VirtualAllocEx(pi.hProcess, (void *)LS_HEAP_BASE, LS_HEAP_SIZE, MEM_RESERVE,
                       PAGE_READWRITE) != (void *)LS_HEAP_BASE) {
        fprintf(stderr, "ke_lockstep: cannot reserve fixed ranges in the child (%lu)\n",
                (unsigned long)GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return -1;
    }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    *exit_code = (int)code;
    return 1;
}

static void parse_list(const char *s, unsigned *out, int *count, int max, int base)
{
    while (s && *s && *count < max) {
        char *end;
        unsigned long v = strtoul(s, &end, base);
        if (end == s)
            break;
        out[(*count)++] = (unsigned)v;
        s = *end ? end + 1 : end;
    }
}

int main(int argc, char **argv)
{
    char image[512], symbols[512];
    int i, child_exit, code, relaunch;
    unsigned keys[64];
    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--mode") && v) { mode_orig = !strcmp(v, "orig"); i++; }
        else if (!strcmp(a, "--image") && v) { image_dir = v; i++; }
        else if (!strcmp(a, "--data") && v) { data_dir = v; i++; }
        else if (!strcmp(a, "--out") && v) { out_path = v; i++; }
        else if (!strcmp(a, "--portmap") && v) { portmap_path = v; i++; }
        else if (!strcmp(a, "--replay") && v) { replay_path = v; i++; }
        else if (!strcmp(a, "--log") && v) { log_path = v; i++; }
        else if (!strcmp(a, "--io-trace") && v) {
            io_trace = fopen(v, "w");
            if (io_trace && getenv("KE_LOCKSTEP_TRACE_UNBUFFERED"))
                setvbuf(io_trace, NULL, _IONBF, 0);
            i++;
        }
        else if (!strcmp(a, "--io-trace-first") && v) {
            io_trace_frame_first = (unsigned)strtoul(v, NULL, 0); i++;
        } else if (!strcmp(a, "--watch") && v) {
            watch_addr = (uint32_t)strtoul(v, NULL, 16); i++;
        } else if (!strcmp(a, "--watch-from") && v) {
            watch_from = (uint32_t)strtoul(v, NULL, 0); i++;
        } else if (!strcmp(a, "--vga-trace")) {
            vga_write_trace_hook = trace_vga_write;
        } else if (!strcmp(a, "--io-trace-frames") && v) {
            io_trace_frame_limit = (unsigned)strtoul(v, NULL, 0); i++;
        }
        else if (!strcmp(a, "--frames") && v) { max_frames = (unsigned)strtoul(v, NULL, 0); i++; }
        else if (!strcmp(a, "--full-at") && v) {
            parse_list(v, full_at, &full_at_count, 64, 10); i++;
        } else if (!strcmp(a, "--idle-keys") && v) {
            int n = 0;
            parse_list(v, keys, &n, 64, 16);
            for (idle_key_count = 0; idle_key_count < n; idle_key_count++)
                idle_keys[idle_key_count] = (uint8_t)keys[idle_key_count];
            i++;
        } else {
            fprintf(stderr, "ke_lockstep: unknown argument %s\n", a);
            return 2;
        }
    }
    relaunch = relaunch_with_low_memory_reserved(&child_exit);
    if (relaunch != 0)
        return relaunch > 0 ? child_exit : 2;

    vhw_lockstep = 1;
    snprintf(image, sizeof image, "%s/ke_image.bin", image_dir);
    snprintf(symbols, sizeof symbols, "%s/ke_symbols.txt", image_dir);
    /* Both modes load the original image so the two processes share one address layout. */
    if (oracle_load(image, symbols) != 0)
        return 2;
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void *)0xA0000, &mbi, sizeof mbi) == sizeof mbi &&
            (mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS))
            fprintf(stderr, "ke_lockstep: WARNING legacy VGA window A0000h is mapped (%08lx +%lx)\n",
                    (unsigned long)(uintptr_t)mbi.AllocationBase, (unsigned long)mbi.RegionSize);
    }
    printf("lockstep: vga alias %08lx\n", (unsigned long)oracle_vga_host_address(0xA0000u));
    if (heap_init() != 0) {
        fprintf(stderr, "ke_lockstep: cannot map the fixed heap arena at %08X\n", LS_HEAP_BASE);
        return 2;
    }
    ke_config_load(1, argv);
    ke_config.irq_async = 0;
    ke_config.sound_blaster = 0;
    ke_config.joystick = 0;
    ke_config.windows_host = 0;
    ke_config.log_level = KE_LOG_INFO;
    ke_log_init(log_path);
    if (vhw_init() != 0)
        return 2;
    if (!SetCurrentDirectoryA(data_dir)) {
        fprintf(stderr, "ke_lockstep: cannot enter data directory %s\n", data_dir);
        return 2;
    }
    if (!mode_orig) {
        if (!portmap_path || load_portmap(portmap_path) != 0) {
            fprintf(stderr, "ke_lockstep: port mode needs --portmap (port/tools/lockstep.py)\n");
            return 2;
        }
    } else {
        memset(dg_mask, 1, sizeof dg_mask);
        if (setup_original() != 0)
            return 2;
    }
    if (out_path) {
        uint32_t size = DG_SIZE;
        dump = fopen(out_path, "wb");
        if (!dump) {
            fprintf(stderr, "ke_lockstep: cannot write %s\n", out_path);
            return 2;
        }
        fwrite("LSD1", 1, 4, dump);
        fwrite(&size, 4, 1, dump);
        fwrite(dg_mask, 1, DG_SIZE, dump);
    }
    if (replay_path && ke_replay_load(replay_path) != 0)
        return 2;
    ke_replay_use_clock(lockstep_clock);
    vhw_lockstep_ms_hook = ke_replay_pump;
    ke_replay_start();
    vhw_lockstep_idle_hook = lockstep_idle;
    if (io_trace)
        vhw_io_trace_hook = trace_io;
    ke_frame_entry_hook = lockstep_frame;
    heap_active = 1;
    watch_install();
    code = ke_game_run_here(mode_orig ? orig_main : ke_game_main);
    finish(code == 3 ? "fault (see log)" : "game exit");
    ke_replay_report();
    return code == 3 ? 3 : 0;
}
