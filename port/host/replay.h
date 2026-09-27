/* replay.h - deterministic host input and frame-entry replay support. */
#ifndef KE_REPLAY_H
#define KE_REPLAY_H

#include <stdint.h>

/* Host input injection shared by the SDL scheduler and frame replay. Mouse coordinates are
 * the DOS driver's coordinates (twice the game's 320x200 raster coordinates). */
void ke_input_set_mouse_position(int x, int y);
void ke_input_set_mouse_buttons(int buttons);
void ke_input_push_scancode(uint8_t code);

/* The smoke harness converts kegg_forged JSON into the compact KEPORTREPLAY text format. */
int ke_replay_load(const char *path);
void ke_replay_start(void);
void ke_replay_frame_entry(void);
void ke_replay_report(void);

/* GNU ld --wrap=wait_for_tick: inject immediately before the original per-frame wait. */
void __wrap_wait_for_tick(short wait_flags);
void __real_wait_for_tick(short wait_flags);

#endif
