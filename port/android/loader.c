/* loader.c - libmain.so: maps the game below 2 GB, then runs it (docs/android/architecture.md).
 *
 * SDL's Java side loads libSDL3.so and this library and calls SDL_main() on its SDL thread.
 * The game (libkegame.so: the ILP32 game world, the virtual PC and the host) must live
 * below 2 GB, because the historical code keeps addresses in 32-bit ints. This loader
 *   1. finds a free window below 2 GB in /proc/self/maps (ART's heaps also live in the low
 *      4 GB, so no fixed address is assumed),
 *   2. reserves it with PROT_NONE,
 *   3. loads libkegame.so into the start of the window with
 *      android_dlopen_ext(ANDROID_DLEXT_RESERVED_ADDRESS),
 *   4. maps the rest read/write as the game heap arena and the game thread's stack,
 *   5. calls ke_android_main() with the regions.
 * Nothing here knows about the game; libkegame.so is never loaded by Java.
 */
#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "ke_android.h"

#define TAG "KryptonEgg"
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ERR(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define MB (1024u * 1024u)
#define IMAGE_SIZE (48u * MB)        /* reserved for libkegame.so (it is ~5 MiB)            */
#define HEAP_SIZE (64u * MB)         /* game heap arena (the game allocates < 1 MiB)        */
#define STACK_SIZE (8u * MB)         /* game thread stack                                   */
#define GUARD (2u * MB)              /* PROT_NONE between the parts                         */
#define TOTAL (IMAGE_SIZE + GUARD + HEAP_SIZE + GUARD + STACK_SIZE)
#define LOW_FLOOR 0x01000000ull      /* above DOS memory and anything the kernel keeps low  */
#define LOW_CEILING 0x80000000ull    /* int -> pointer conversions sign-extend: stay < 2 GB */
#define ALIGN (2u * MB)

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

/* Lowest free window of `size` bytes in [LOW_FLOOR, LOW_CEILING), aligned to ALIGN. */
static uintptr_t find_low_window(size_t size)
{
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[512];
    uint64_t cursor = LOW_FLOOR;
    if (!maps)
        return 0;
    while (fgets(line, sizeof line, maps)) {
        unsigned long long lo, hi;
        if (sscanf(line, "%llx-%llx", &lo, &hi) != 2)
            continue;
        if (hi <= cursor)
            continue;
        if (lo >= cursor + size)
            break;                          /* the gap before this mapping fits */
        cursor = (hi + ALIGN - 1) & ~(uint64_t)(ALIGN - 1);
        if (cursor + size > LOW_CEILING)
            break;
    }
    fclose(maps);
    if (cursor + size > LOW_CEILING)
        return 0;
    return (uintptr_t)cursor;
}

static void log_low_mappings(void)
{
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[512];
    if (!maps)
        return;
    while (fgets(line, sizeof line, maps)) {
        unsigned long long lo;
        if (sscanf(line, "%llx", &lo) == 1 && lo < LOW_CEILING) {
            line[strcspn(line, "\n")] = 0;
            ERR("  %s", line);
        }
    }
    fclose(maps);
}

static void *reserve_low(size_t size)
{
    int attempt;
    for (attempt = 0; attempt < 8; attempt++) {
        uintptr_t at = find_low_window(size);
        void *p;
        if (!at)
            break;
        p = mmap((void *)at, size, PROT_NONE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == (void *)at)
            return p;
        if (p != MAP_FAILED)
            munmap(p, size);        /* the kernel ignored the flag and moved it: retry */
    }
    return NULL;
}

/* Map `library` below 2 GB with the heap/stack regions and call its `entry`. */
static int load_and_run(const char *library, const char *entry_name, int argc, char *argv[])
{
    uint8_t *window;
    android_dlextinfo ext;
    void *game;
    KeAndroidMain entry;
    KeLowRegions regions;

    window = reserve_low(TOTAL);
    if (!window) {
        ERR("no free %u MiB window below 2 GB; low mappings:", TOTAL / MB);
        log_low_mappings();
        return 2;
    }
    memset(&ext, 0, sizeof ext);
    ext.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
    ext.reserved_addr = window;
    ext.reserved_size = IMAGE_SIZE;
    game = android_dlopen_ext(library, RTLD_NOW | RTLD_LOCAL, &ext);
    if (!game) {
        ERR("android_dlopen_ext(%s) at %p failed: %s", library, (void *)window, dlerror());
        return 2;
    }
    regions.image = window;
    regions.image_size = IMAGE_SIZE;
    regions.heap = window + IMAGE_SIZE + GUARD;
    regions.heap_size = HEAP_SIZE;
    regions.stack = regions.heap + HEAP_SIZE + GUARD;
    regions.stack_size = STACK_SIZE;
    if (mmap(regions.heap, HEAP_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != regions.heap ||
        mmap(regions.stack, STACK_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != regions.stack) {
        ERR("cannot map the game heap/stack: %s", strerror(errno));
        return 2;
    }
    entry = (KeAndroidMain)dlsym(game, entry_name);
    if (!entry) {
        ERR("%s has no %s: %s", library, entry_name, dlerror());
        return 2;
    }
    LOG("game library at %p, heap %p (%u MiB), stack %p (%u MiB)", (void *)window,
        (void *)regions.heap, HEAP_SIZE / MB, (void *)regions.stack, STACK_SIZE / MB);
    return entry(argc, argv, &regions);
}

#if defined(KE_LOCKSTEP_LOADER)
/* ke_lockstep_loader: command-line runner of the deterministic lockstep machine
 * (port/android/lockstep, docs/android/building.md "64-bit lockstep"). */
int main(int argc, char *argv[])
{
    return load_and_run("libkelockstep.so", "ke_lockstep_main", argc, argv);
}
#else
__attribute__((visibility("default"))) int SDL_main(int argc, char *argv[])
{
    /* Test hook: `am start ... --esa ke.args --lockstep,...` runs the lockstep runner inside
     * the app process (the way to exercise arm64 code under an emulator's ARM translation). */
    if (argc > 1 && strcmp(argv[1], "--lockstep") == 0) {
        argv[1] = argv[0];
        return load_and_run("libkelockstep.so", "ke_lockstep_main", argc - 1, argv + 1);
    }
    return load_and_run("libkegame.so", "ke_android_main", argc, argv);
}
#endif
