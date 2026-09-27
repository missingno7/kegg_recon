/* replay.c - input-only replay at the game's wait_for_tick frame entry.
 *
 * Input events are intentionally applied before the historical wait routine polls the
 * keyboard and mouse. The linker wrapper touches no game state or game logic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "ke_port.h"
#include "replay.h"

typedef enum ReplayKind { REPLAY_MOUSE, REPLAY_KEY } ReplayKind;
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
    ke_log(KE_LOG_INFO, "replay", "loaded %u events over %u frame entries from %s",
           expected, frames, path);
    return 0;
}

void ke_replay_start(void)
{
    if (!replay_loaded || replay_started)
        return;
    replay_started = 1;
    ke_log(KE_LOG_INFO, "replay", "started at wait_for_tick occurrence 0");
}

void ke_replay_frame_entry(void)
{
    if (!replay_started || replay_complete)
        return;
    while (next_event < event_count && events[next_event].occurrence == frame_occurrence) {
        const ReplayEvent *e = &events[next_event++];
        if (e->kind == REPLAY_MOUSE) {
            ke_input_set_mouse_position(e->a, e->b);
            ke_input_set_mouse_buttons(e->buttons);
        } else {
            ke_input_push_scancode((uint8_t)e->a);
        }
    }
    if (next_event < event_count && events[next_event].occurrence < frame_occurrence) {
        ke_log(KE_LOG_WARN, "replay", "missed input occurrence %u at frame entry %u",
               events[next_event].occurrence, frame_occurrence);
        while (next_event < event_count && events[next_event].occurrence < frame_occurrence)
            ++next_event;
    }
    ++frame_occurrence;
    if (frame_occurrence &&
        ((frame_occurrence <= 60 && frame_occurrence % 10 == 0) || frame_occurrence % 100 == 0))
        ke_log(KE_LOG_INFO, "replay", "reached wait_for_tick occurrence %u/%u",
               frame_occurrence, replay_frame_count);
    if (frame_occurrence >= replay_frame_count)
        replay_complete = 1;
}

void ke_replay_report(void)
{
    if (!replay_loaded || replay_reported)
        return;
    replay_reported = 1;
    ke_log(KE_LOG_INFO, "replay", "stopped at frame entry %u; applied %u/%u events%s",
           frame_occurrence, (unsigned)next_event, (unsigned)event_count,
           replay_complete ? " (complete)" : " (incomplete)");
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
