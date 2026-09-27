/* test_m_137a8.c - differential tests: original asm/m_137a8_13944 bytes vs port/asm translation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "oracle.h"
#include "oracle_test.h"
#include "../vhw/vhw.h"

void mov_mem(uint32_t source, uint32_t destination, uint32_t byte_count);
void clear_video_bytes(uint32_t destination, uint32_t byte_count);
void probe_cpu_environment(void);
extern int cpu_type, cpu_mode, cpu_iopl;
extern unsigned char vga_state[];

#define ARENA 0x4000

static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n) { rng = rng * 1103515245u + 12345u; return (rng >> 8) % n; }

/* mov_mem on ordinary memory: every overlap/direction/length class, including the QUIRK of
 * backward dword moves that read and write up to 3 bytes beyond the requested range. */
static int test_mov_mem(void)
{
    uint8_t *a = VirtualAlloc(NULL, ARENA, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    uint8_t *b = VirtualAlloc(NULL, ARENA, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    void *orig = oracle_sym("mov_mem");
    int i, failures = 0;
    for (i = 0; i < 2000; i++) {
        uint32_t len = rnd(i < 1000 ? 16 : 600), src = 16 + rnd(ARENA / 2), dst;
        uint32_t args[3];
        int j;
        dst = (i & 1) ? src + rnd(len + 8) : 16 + rnd(ARENA / 2);
        if (i % 3 == 0 && src > len)
            dst = src - rnd(len + 1);
        for (j = 0; j < ARENA; j++)
            a[j] = b[j] = (uint8_t)(j * 7 + i);
        args[0] = (uint32_t)(uintptr_t)(a + src);
        args[1] = (uint32_t)(uintptr_t)(a + dst);
        args[2] = len;
        oracle_call(orig, 3, args);
        mov_mem((uint32_t)(uintptr_t)(b + src), (uint32_t)(uintptr_t)(b + dst), len);
        if (memcmp(a, b, ARENA) != 0) {
            if (failures++ < 5)
                printf("    mov_mem(src+%u, dst+%u, %u): arenas differ\n", src, dst, len);
        }
    }
    VirtualFree(a, 0, MEM_RELEASE);
    VirtualFree(b, 0, MEM_RELEASE);
    return failures;
}

static int test_clear_video_bytes_ram(void)
{
    uint8_t *a = VirtualAlloc(NULL, ARENA, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    uint8_t *b = VirtualAlloc(NULL, ARENA, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    void *orig = oracle_sym("clear_video_bytes_entry");
    int i, failures = 0;
    for (i = 0; i < 500; i++) {
        uint32_t off = rnd(ARENA / 2), len = rnd(300), args[2];
        memset(a, 0x5a, ARENA);
        memset(b, 0x5a, ARENA);
        args[0] = (uint32_t)(uintptr_t)(a + off);
        args[1] = len;
        oracle_call(orig, 2, args);
        clear_video_bytes((uint32_t)(uintptr_t)(b + off), len);
        if (memcmp(a, b, ARENA) != 0 && failures++ < 5)
            printf("    clear_video_bytes(+%u, %u): arenas differ\n", off, len);
    }
    VirtualFree(a, 0, MEM_RELEASE);
    VirtualFree(b, 0, MEM_RELEASE);
    return failures;
}

static void reset_vga_fixture(void)
{
    unsigned char *original_vga_state = oracle_sym("vga_state");
    vga_bios_set_mode(0x13);
    vga_state[0x60] = 0;
    vga_state[0x61] = 0;
    if (original_vga_state) {
        original_vga_state[0x60] = 0;
        original_vga_state[0x61] = 0;
    }
}

static int test_mov_mem_video(void)
{
    uint8_t *source = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    OracleVgaSnapshot *original = malloc(sizeof *original), *ported = malloc(sizeof *ported);
    uint32_t args[3];
    unsigned i;
    int failures = 0;
    if (!source || !original || !ported || !oracle_vga_window_reserved()) {
        printf("    VGA fixture allocation/reservation failed\n");
        failures++;
        goto done;
    }
    for (i = 0; i < 4096; i++)
        source[i] = (uint8_t)(i * 29u + (i >> 3) + 0x51u);
    args[0] = (uint32_t)(uintptr_t)source;
    args[1] = 0xA1237u;
    args[2] = 267;

    reset_vga_fixture();
    oracle_call(oracle_sym("mov_mem"), 3, args);
    if (oracle_vga_snapshot(original) != 0) {
        failures++;
        goto done;
    }
    reset_vga_fixture();
    oracle_port_call((void *)mov_mem, 3, args);
    if (oracle_vga_snapshot(ported) != 0 ||
        oracle_vga_snapshot_equal(original, ported, "mov_mem video") != 0)
        failures++;
done:
    if (source) VirtualFree(source, 0, MEM_RELEASE);
    free(original);
    free(ported);
    return failures;
}

static int test_clear_video_bytes_video(void)
{
    OracleVgaSnapshot *original = malloc(sizeof *original), *ported = malloc(sizeof *ported);
    uint32_t args[2] = {0xA02F1u, 271};
    int failures = 0;
    if (!original || !ported || !oracle_vga_window_reserved()) {
        printf("    VGA fixture allocation/reservation failed\n");
        failures++;
        goto done;
    }
    reset_vga_fixture();
    oracle_call(oracle_sym("clear_video_bytes_entry"), 2, args);
    if (oracle_vga_snapshot(original) != 0) {
        failures++;
        goto done;
    }
    reset_vga_fixture();
    oracle_port_call((void *)clear_video_bytes, 2, args);
    if (oracle_vga_snapshot(ported) != 0 ||
        oracle_vga_snapshot_equal(original, ported, "clear_video_bytes video") != 0)
        failures++;
done:
    free(original);
    free(ported);
    return failures;
}

/* The probe itself is environment dependent: the original reports what this host process
 * is (486+, protected mode, user-mode IOPL 0); the translation reports the DOS/4GW machine
 * it emulates (IOPL 3). Generation and mode must agree; the CLI..STI trace must match. */
static int test_probe_cpu(void)
{
    int *o_type = oracle_sym("cpu_type"), *o_mode = oracle_sym("cpu_mode"), *o_iopl = oracle_sym("cpu_iopl");
    int failures = 0, n, i;
    const OracleEvent *t;
    char seq[16] = "";
    oracle_trace_reset();
    oracle_call(oracle_sym("probe_cpu_environment"), 0, NULL);
    t = oracle_trace();
    n = oracle_trace_count();
    for (i = 0; i < n && i < 15; i++)
        seq[i] = (char)t[i].kind;
    probe_cpu_environment();
    printf("    original: type=%X mode=%d iopl=%d trace=%s | port: type=%X mode=%d iopl=%d IF=%d\n",
           *o_type, *o_mode, *o_iopl, seq, cpu_type, cpu_mode, cpu_iopl, vcpu_interrupts_enabled());
    if (*o_type != cpu_type || *o_mode != cpu_mode)
        failures++;
    if (strcmp(seq, "CCS") != 0 || !vcpu_interrupts_enabled())
        failures++;
    return failures;
}

void register_m_137a8_tests(void)
{
    oracle_register("m_137a8 mov_mem (RAM, 2000 random overlaps)", test_mov_mem);
    oracle_register("m_137a8 clear_video_bytes (RAM)", test_clear_video_bytes_ram);
    oracle_register("O1 VGA hook: mov_mem video planes", test_mov_mem_video);
    oracle_register("O1 VGA hook: clear_video_bytes video planes", test_clear_video_bytes_video);
    oracle_register("m_137a8 probe_cpu_environment", test_probe_cpu);
}
