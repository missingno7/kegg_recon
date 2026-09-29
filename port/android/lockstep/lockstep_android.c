/* lockstep_android.c - libkelockstep.so: the port's lockstep "port mode" on 64-bit Android.
 *
 *   ke_lockstep_loader --data DIR --out FILE.lsd --portmap FILE --frames N
 *                      [--replay FILE.kereplay] [--idle-keys 39,b9] [--sound] [--log FILE]
 *
 * The same deterministic virtual PC as port/oracle/lockstep.c (vhw_lockstep = 1: time
 * advances only by emulated events, no device threads, synchronous interrupts, the replay
 * keyed by the Nth wait_for_tick entry) runs the ILP32 game world on a 64-bit CPU and writes
 * the same LSD2/LSF1 dump. port/android/tools/lockstep64.py compares it with the Windows
 * i686 port's dump of the same run: any difference is a difference of the 64-bit build.
 *
 * The heap arena sits at the Windows runner's fixed address (0x20000000, bump allocator,
 * identical block addresses), so heap pointers stored in game data compare as equal values.
 * The portmap lists library-relative addresses (lockstep64.py derives them from the Windows
 * portmap and this library's symbols); DGROUP is captured in the original layout.
 */
#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../../vhw/vhw.h"
#include "../../include/ke_port.h"
#include "../../host/replay.h"
#include "../../platform/ke_platform.h"
#include "../ke_android.h"

#define DG_SIZE 0xE610u
#define PLANE_BLOCK 1024u
#define PLANE_BLOCKS (0x40000u / PLANE_BLOCK)
#define LS_HEAP_BASE 0x20000000u
#define LS_HEAP_SIZE (96u << 20)
#define SOUND_STATE_BYTES (VSB_DEBUG_BYTES + 64u)

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

void wait_for_tick(short wait_flags);
void ke_game_main(void);
extern void (*ke_frame_entry_hook)(void);
void ke_input_push_scancode(uint8_t code);
void ke32_set_heap(void *base, size_t size);

/* ---- deterministic bump heap, the same addresses as the Windows runner ------------------ */
static uint8_t *heap;
static uint32_t heap_top;
static struct { uint32_t off, size; uint8_t freed; } heap_blocks[4096];
static int heap_count;

static uint32_t ls_malloc(uint32_t n)
{
    uint32_t size = (n + 15u) & ~15u;
    void *p;
    if (!heap || heap_count >= 4096 || size > LS_HEAP_SIZE - heap_top || n == 0)
        return 0;
    p = heap + heap_top;
    heap_blocks[heap_count].off = heap_top;
    heap_blocks[heap_count].size = size;
    heap_blocks[heap_count].freed = 0;
    heap_count++;
    heap_top += size;
    memset(p, 0, size);
    return (uint32_t)(uintptr_t)p;
}

static void ls_free(uint32_t p32)
{
    uint8_t *p = (uint8_t *)(uintptr_t)p32;
    int i;
    if (!heap || p < heap || p >= heap + LS_HEAP_SIZE)
        return;
    for (i = heap_count - 1; i >= 0; i--)
        if (heap + heap_blocks[i].off == p) {
            heap_blocks[i].freed = 1;
            break;
        }
    while (heap_count > 0 && heap_blocks[heap_count - 1].freed) {
        heap_count--;
        heap_top = heap_blocks[heap_count].off;
    }
}

extern uint32_t (*ke32_malloc_hook)(uint32_t);
extern void (*ke32_free_hook)(uint32_t);

static uint64_t lockstep_clock(void) { return vhw_lockstep_ns; }

/* ---- the frame hook: replay.c's __wrap_wait_for_tick (the ILP32 world's calls to
 * wait_for_tick from other units are renamed to it, as ld --wrap does on Windows) ------- */
void __real_wait_for_tick(short wait_flags) { wait_for_tick(wait_flags); }

/* ---- dump ------------------------------------------------------------------------------- */
static const char *out_path, *portmap_path, *replay_path, *log_path, *data_dir = ".";
static unsigned max_frames = 0xffffffffu, frame_no;
static uint8_t idle_keys[64];
static int idle_key_count, idle_key_next;
static unsigned idle_calls_without_input;
static FILE *dump, *dma_capture;
static int sound_enabled;
static uint8_t dg[DG_SIZE], dg_mask[DG_SIZE];
static uint8_t planes[0x40000];
static struct { uint32_t orig_off, len; uintptr_t port_addr; } spans[4096];
static int span_count;
static uintptr_t image_base;

static int load_portmap(const char *path)
{
    FILE *f = fopen(path, "r");
    char name[128];
    unsigned off, len;
    unsigned long long rel;
    if (!f)
        return -1;
    while (span_count < 4096 && fscanf(f, "%x %x %llx %127s", &off, &len, &rel, name) == 4) {
        if (off + len > DG_SIZE)
            continue;
        spans[span_count].orig_off = off;
        spans[span_count].len = len;
        spans[span_count].port_addr = image_base + (uintptr_t)rel;
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

static void capture_dma_block(unsigned block_no, const uint8_t *bytes, uint32_t len)
{
    if (!dma_capture)
        return;
    fwrite(&block_no, 4, 1, dma_capture);
    fwrite(&len, 4, 1, dma_capture);
    fwrite(bytes, 1, len, dma_capture);
    fflush(dma_capture);
}

static void write_frame(void)
{
    uint8_t pic[10], pad[3] = {0, 0, 0}, regs[VGA_DEBUG_REGS], dac[768];
    uint8_t sound_state[SOUND_STATE_BYTES];
    uint32_t pit[16], hashes[PLANE_BLOCKS], v;
    uint64_t clock = vhw_lockstep_ns;
    uint8_t full = 0;
    unsigned i;
    int s;
    if (!dump)
        return;
    vpic_debug_state(pic);
    vpit_debug_state(pit);
    vga_debug_state(planes, regs, dac);
    vsb_debug_state(sound_state);
    vdma_debug_state(sound_state + VSB_DEBUG_BYTES);
    for (i = 0; i < PLANE_BLOCKS; i++)
        hashes[i] = fnv1a(planes + i * PLANE_BLOCK, PLANE_BLOCK);
    memset(dg, 0, sizeof dg);
    for (s = 0; s < span_count; s++)
        memcpy(dg + spans[s].orig_off, (const void *)spans[s].port_addr, spans[s].len);
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
    fwrite(dg, 1, DG_SIZE, dump);
    fwrite(sound_state, 1, sizeof sound_state, dump);
}

static void finish(const char *why)
{
    if (dump)
        fclose(dump);
    dump = NULL;
    if (dma_capture)
        fclose(dma_capture);
    dma_capture = NULL;
    printf("lockstep: port mode stopped after %u frame entries (%s), clock %.6f s\n", frame_no,
           why, (double)vhw_lockstep_ns / 1e9);
    fflush(stdout);
}

static void lockstep_frame(void)
{
    if (frame_no >= max_frames) {
        finish("frame limit");
        _exit(0);
    }
    write_frame();
    ke_replay_frame_entry();
    frame_no++;
    if (vcpu_if_flag && !vhw_in_isr && vpic_has_deliverable())
        vpic_deliver_pending();
}

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
        _exit(4);
    }
}

/* The game must run on the low stack (addresses of locals are kept in ints). */
static int run_code;
static unsigned lockstep_thread(void *unused)
{
    (void)unused;
    run_code = ke_game_run_here(ke_game_main);
    return 0;
}

__attribute__((visibility("default"))) int ke_lockstep_main(int argc, char *argv[],
                                                            const KeLowRegions *regions)
{
    Dl_info info;
    int i, code;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--data") && v) { data_dir = v; i++; }
        else if (!strcmp(a, "--out") && v) { out_path = v; i++; }
        else if (!strcmp(a, "--portmap") && v) { portmap_path = v; i++; }
        else if (!strcmp(a, "--replay") && v) { replay_path = v; i++; }
        else if (!strcmp(a, "--log") && v) { log_path = v; i++; }
        else if (!strcmp(a, "--frames") && v) { max_frames = (unsigned)strtoul(v, NULL, 0); i++; }
        else if (!strcmp(a, "--sound")) { sound_enabled = 1; }
        else if (!strcmp(a, "--dma-capture") && v) {
            dma_capture = fopen(v, "wb");
            if (dma_capture)
                fwrite("SDC1", 1, 4, dma_capture);
            i++;
        } else if (!strcmp(a, "--idle-keys") && v) {
            const char *s = v;
            while (*s && idle_key_count < 64) {
                char *end;
                unsigned long k = strtoul(s, &end, 16);
                if (end == s)
                    break;
                idle_keys[idle_key_count++] = (uint8_t)k;
                s = *end ? end + 1 : end;
            }
            i++;
        } else {
            fprintf(stderr, "ke_lockstep: unknown argument %s\n", a);
            return 2;
        }
    }
    if (!dladdr((void *)ke_lockstep_main, &info))
        return 2;
    image_base = (uintptr_t)info.dli_fbase;
    printf("lockstep: image base %lx\n", (unsigned long)image_base);
    ke32_set_heap(regions->heap, regions->heap_size);
    ke_platform_set_game_stack(regions->stack, regions->stack_size);
    if (ke32_apply_relocs() != 0)
        return 2;
    heap = (uint8_t *)mmap((void *)(uintptr_t)LS_HEAP_BASE, LS_HEAP_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (heap != (uint8_t *)(uintptr_t)LS_HEAP_BASE) {
        fprintf(stderr, "ke_lockstep: cannot map the heap arena at %08X: %s\n", LS_HEAP_BASE,
                strerror(errno));
        return 2;
    }
    ke32_malloc_hook = ls_malloc;
    ke32_free_hook = ls_free;
    vhw_lockstep = 1;
    {
        char *args[2] = {argv[0], NULL};
        ke_config_load(1, args);
    }
    ke_config.irq_async = 0;
    ke_config.sound_blaster = sound_enabled;
    ke_config.joystick = 0;
    ke_config.windows_host = 0;
    ke_config.mouse_native = 0;
    ke_config.log_level = KE_LOG_INFO;
    ke_log_init(log_path);
    if (vhw_init() != 0)
        return 2;
    vsb_init();
    if (dma_capture)
        vsb_set_dma_capture_hook(capture_dma_block);
    if (chdir(data_dir) != 0) {
        fprintf(stderr, "ke_lockstep: cannot enter data directory %s\n", data_dir);
        return 2;
    }
    if (!portmap_path || load_portmap(portmap_path) != 0) {
        fprintf(stderr, "ke_lockstep: needs --portmap (lockstep64.py)\n");
        return 2;
    }
    if (out_path) {
        uint32_t size = DG_SIZE, sound_size = SOUND_STATE_BYTES;
        dump = fopen(out_path, "wb");
        if (!dump)
            return 2;
        fwrite("LSD2", 1, 4, dump);
        fwrite(&size, 4, 1, dump);
        fwrite(&sound_size, 4, 1, dump);
        fwrite(dg_mask, 1, DG_SIZE, dump);
    }
    if (replay_path && ke_replay_load(replay_path) != 0)
        return 2;
    ke_replay_use_clock(lockstep_clock);
    vhw_lockstep_ms_hook = ke_replay_pump;
    ke_replay_start();
    vhw_lockstep_idle_hook = lockstep_idle;
    ke_frame_entry_hook = lockstep_frame;
    {
        KeThread *t = ke_thread_create(lockstep_thread, NULL, 0, KE_THREAD_GAME_STACK);
        if (!t)
            return 2;
        ke_thread_join_ms(t, KE_WAIT_INFINITE);
        code = run_code;
    }
    finish(code == 3 ? "fault (see log)" : "game exit");
    ke_replay_report();
    return code == 3 ? 3 : 0;
}
