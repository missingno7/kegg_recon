/* replay.c - input-only replay aligned to wait_for_tick entries, with a timed pump for
 * blocking input loops that continue polling without reaching another frame entry.
 *
 * Input events are intentionally applied before the historical wait routine polls the
 * keyboard and mouse. The linker wrapper touches no game state or game logic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "ke_port.h"
#include "replay.h"
#include "../vhw/vhw.h"

typedef enum ReplayKind { REPLAY_MOUSE, REPLAY_KEY, REPLAY_JOYSTICK } ReplayKind;
typedef struct ReplayEvent {
    unsigned occurrence;
    ReplayKind kind;
    int a, b, buttons;
} ReplayEvent;

static ReplayEvent *events;
static size_t event_count, next_event;
static unsigned frame_occurrence, replay_frame_count;
static volatile LONG wait_for_tick_count;
static int replay_loaded, replay_started, replay_complete, replay_reported;
static uint64_t last_frame_entry_ns;
static uint64_t last_key_event_ns;
static uint64_t frame_period_ns = 1000000000ull / 70;
static CRITICAL_SECTION replay_lock;
static int replay_lock_initialized, replay_pump_stop;
/* Lockstep runs (port/oracle/lockstep.c) replace host time with the virtual machine clock
 * and call ke_replay_pump() themselves instead of the pump thread. */
static uint64_t (*replay_clock)(void) = ke_now_ns;
static int replay_external_pump;

void ke_replay_use_clock(uint64_t (*clock)(void))
{
    replay_clock = clock ? clock : ke_now_ns;
    replay_external_pump = clock != NULL;
}

static unsigned replay_time_occurrence(uint64_t now)
{
    uint64_t elapsed, advanced;
    unsigned due;
    if (!last_frame_entry_ns || !frame_occurrence)
        return 0;
    elapsed = now - last_frame_entry_ns;
    advanced = frame_period_ns ? elapsed / frame_period_ns : 0;
    due = frame_occurrence - 1;
    if (advanced > 0xffffffffu - due)
        due = 0xffffffffu;
    else
        due += (unsigned)advanced;
    if (due >= replay_frame_count)
        due = replay_frame_count - 1;
    return due;
}

static void replay_apply_event(const ReplayEvent *e)
{
    if (e->kind == REPLAY_MOUSE) {
        ke_input_set_mouse_position(e->a, e->b);
        ke_input_set_mouse_buttons(e->buttons);
    } else if (e->kind == REPLAY_JOYSTICK) {
        float x = (float)e->a / 1000.0f;
        float y = (float)e->b / 1000.0f;
        vjoy_set(0, x, e->buttons);
        vjoy_set(1, y, e->buttons);
        vjoy_set(2, 0.0f, e->buttons);
        vjoy_set(3, 0.0f, e->buttons);
    } else {
        ke_input_push_scancode((uint8_t)e->a);
    }
}

static void replay_apply_through(unsigned occurrence, uint64_t now)
{
    while (next_event < event_count && events[next_event].occurrence <= occurrence) {
        const ReplayEvent *event = &events[next_event];
        if (event->kind == REPLAY_KEY && last_key_event_ns &&
            now - last_key_event_ns < frame_period_ns)
            return;
        ++next_event;
        replay_apply_event(event);
        if (event->kind == REPLAY_KEY) {
            last_key_event_ns = replay_clock();
            return;
        }
    }
}

/* One pump step: apply the events due by time since the last frame entry. Returns 0 once
 * the replay is stopped or complete. */
static int replay_pump_step(void)
{
    uint64_t now;
    EnterCriticalSection(&replay_lock);
    if (replay_pump_stop || replay_complete) {
        LeaveCriticalSection(&replay_lock);
        return 0;
    }
    now = replay_clock();
    replay_apply_through(replay_time_occurrence(now), now);
    if (frame_occurrence >= replay_frame_count && next_event >= event_count)
        replay_complete = 1;
    LeaveCriticalSection(&replay_lock);
    return 1;
}

void ke_replay_pump(void)
{
    if (replay_lock_initialized && replay_started)
        replay_pump_step();
}

static DWORD WINAPI replay_pump_thread(LPVOID unused)
{
    (void)unused;
    for (;;) {
        Sleep(1);
        if (!replay_pump_step())
            break;
    }
    return 0;
}

int ke_replay_load(const char *path)
{
    FILE *f;
    char magic[32];
    unsigned version, expected, frames, i;
    ReplayEvent *loaded;
    if (!path || !*path)
        return 0;
    f = fopen(path, "r");
    if (!f) {
        ke_log(KE_LOG_ERROR, "replay", "cannot open replay %s", path);
        return -1;
    }
    if (fscanf(f, "%31s %u %u %u", magic, &version, &expected, &frames) != 4 ||
        strcmp(magic, "KEPORTREPLAY") != 0 || version != 1 || expected > 1000000u ||
        frames > 1000000u) {
        ke_log(KE_LOG_ERROR, "replay", "invalid replay header in %s", path);
        fclose(f);
        return -1;
    }
    loaded = (ReplayEvent *)calloc(expected ? expected : 1, sizeof *loaded);
    if (!loaded) {
        fclose(f);
        ke_log(KE_LOG_ERROR, "replay", "out of memory loading %s", path);
        return -1;
    }
    for (i = 0; i < expected; ++i) {
        char type;
        ReplayEvent *e = &loaded[i];
        if (fscanf(f, " %c %u", &type, &e->occurrence) != 2 ||
            e->occurrence >= frames) {
            free(loaded);
            fclose(f);
            ke_log(KE_LOG_ERROR, "replay", "invalid event %u in %s", i, path);
            return -1;
        }
        if (type == 'M') {
            e->kind = REPLAY_MOUSE;
            if (fscanf(f, "%d %d %d", &e->a, &e->b, &e->buttons) != 3 ||
                e->a < 0 || e->a > 639 || e->b < 0 || e->b > 399 ||
                e->buttons < 0 || e->buttons > 7) {
                free(loaded);
                fclose(f);
                ke_log(KE_LOG_ERROR, "replay", "invalid mouse event %u in %s", i, path);
                return -1;
            }
        } else if (type == 'K') {
            e->kind = REPLAY_KEY;
            if (fscanf(f, "%d", &e->a) != 1 || e->a < 0 || e->a > 255) {
                free(loaded);
                fclose(f);
                ke_log(KE_LOG_ERROR, "replay", "invalid keyboard event %u in %s", i, path);
                return -1;
            }
        } else if (type == 'J') {
            e->kind = REPLAY_JOYSTICK;
            if (fscanf(f, "%d %d %d", &e->a, &e->b, &e->buttons) != 3 ||
                e->a < -1000 || e->a > 1000 || e->b < -1000 || e->b > 1000 ||
                e->buttons < 0 || e->buttons > 15) {
                free(loaded);
                fclose(f);
                ke_log(KE_LOG_ERROR, "replay", "invalid joystick event %u in %s", i, path);
                return -1;
            }
        } else {
            free(loaded);
            fclose(f);
            ke_log(KE_LOG_ERROR, "replay", "unknown event type %c in %s", type, path);
            return -1;
        }
        if (i && e->occurrence < loaded[i - 1].occurrence) {
            free(loaded);
            fclose(f);
            ke_log(KE_LOG_ERROR, "replay", "events are not ordered in %s", path);
            return -1;
        }
    }
    fclose(f);
    free(events);
    events = loaded;
    event_count = expected;
    next_event = 0;
    frame_occurrence = 0;
    replay_frame_count = frames;
    replay_loaded = 1;
    replay_started = replay_complete = replay_reported = 0;
    last_frame_entry_ns = 0;
    last_key_event_ns = 0;
    frame_period_ns = 1000000000ull / 70;
    ke_log(KE_LOG_INFO, "replay", "loaded %u events over %u frame entries from %s",
           expected, frames, path);
    return 0;
}

void ke_replay_start(void)
{
    HANDLE thread;
    if (!replay_loaded || replay_started)
        return;
    InitializeCriticalSection(&replay_lock);
    replay_lock_initialized = 1;
    replay_pump_stop = 0;
    replay_started = 1;
    thread = replay_external_pump ? NULL : CreateThread(NULL, 0, replay_pump_thread, NULL, 0, NULL);
    if (thread)
        CloseHandle(thread);
    else if (!replay_external_pump)
        ke_log(KE_LOG_ERROR, "replay", "could not start replay input pump");
    ke_log(KE_LOG_INFO, "replay", "started at wait_for_tick occurrence 0");
}

void ke_replay_frame_entry(void)
{
    uint64_t now, delta;
    if (!replay_lock_initialized)
        return;
    EnterCriticalSection(&replay_lock);
    if (!replay_started || replay_complete) {
        LeaveCriticalSection(&replay_lock);
        return;
    }
    now = replay_clock();
    if (last_frame_entry_ns && now > last_frame_entry_ns) {
        delta = now - last_frame_entry_ns;
        if (delta >= 7000000ull && delta <= 100000000ull)
            frame_period_ns = (frame_period_ns * 3 + delta) / 4;
    }
    last_frame_entry_ns = now;
    replay_apply_through(frame_occurrence, now);
    if (next_event < event_count && events[next_event].occurrence < frame_occurrence) {
        ke_log(KE_LOG_WARN, "replay", "missed input occurrence %u at frame entry %u",
               events[next_event].occurrence, frame_occurrence);
    }
    ++frame_occurrence;
    if (frame_occurrence &&
        ((frame_occurrence <= 60 && frame_occurrence % 10 == 0) || frame_occurrence % 100 == 0))
        ke_log(KE_LOG_INFO, "replay", "reached wait_for_tick occurrence %u/%u",
               frame_occurrence, replay_frame_count);
    if (frame_occurrence >= replay_frame_count && next_event >= event_count)
        replay_complete = 1;
    LeaveCriticalSection(&replay_lock);
}

void ke_replay_report(void)
{
    if (!replay_loaded || replay_reported)
        return;
    if (replay_lock_initialized) {
        EnterCriticalSection(&replay_lock);
        replay_pump_stop = 1;
    }
    replay_reported = 1;
    ke_log(KE_LOG_INFO, "replay", "stopped at frame entry %u; applied %u/%u events%s",
           frame_occurrence, (unsigned)next_event, (unsigned)event_count,
           replay_complete ? " (complete)" : " (incomplete)");
    if (replay_lock_initialized)
        LeaveCriticalSection(&replay_lock);
}

/* Lockstep runner (port/oracle/lockstep.c): replaces the input step at frame entry; it takes
 * its state snapshot, then calls ke_replay_frame_entry() itself. */
void (*ke_frame_entry_hook)(void);

void __wrap_wait_for_tick(short wait_flags)
{
    InterlockedIncrement(&wait_for_tick_count);
    if (ke_frame_entry_hook)
        ke_frame_entry_hook();
    else
        ke_replay_frame_entry();
    __real_wait_for_tick(wait_flags);
}

uint32_t ke_wait_for_tick_count(void)
{
    return (uint32_t)InterlockedCompareExchange(&wait_for_tick_count, 0, 0);
}
