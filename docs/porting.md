# SDL3 migration map

## Goal and principle

The frozen historical source (`historical-exact-clean-v1`) is the behavioral oracle. The portable branch keeps the original game code and replaces the DOS platform underneath it. This map is a proposal for that separate branch; it does not add platform abstractions or SDL concepts to the historical tree.

Function identity is the manifest `(object, start)` pair, not a symbol name. The JSON map contains all 378 manifest functions. `python tools/portmap.py --refresh` updates names, units, and source paths after renames. Addresses below are historical object offsets.

## Target architecture

- **Rendering:** game code draws to 8-bit indexed software pages. The framebuffer emulates the VGA planar and page-flip semantics the game observes; six-bit palette values are converted to the SDL3 texture format at presentation. Palette cycling and fades change palette state without rewriting indexed pixels.
- **Input:** SDL3 events update the same key-state bitmap and scan/ASCII state, mouse coordinates/buttons, and joystick calibration/direction state that the game reads. Keep action dispatch, repeat, pause, and chord behavior in game code.
- **Time:** replace PIT/IRQ with a portable monotonic tick source that preserves logical tick rates and callback order. Run simulation at the original logical step; rendering can run at a different rate.
- **Audio:** replace Sound Blaster/DMA interrupts with an SDL3 audio stream fed by the original request mixer and ProTracker player where portable. Preserve request ordering, sample-rate changes, half-buffer behavior, and tracker BPM/tick timing.
- **Files and assets:** use host file streams and bounded host buffers. Keep original C decoders and format behavior; translate assembly decoders to portable routines with matching output and errors.

## Subsystem boundaries

The table names representative boundary functions using their current manifest names. State and timing describe the observed contract; hardware replacement and risk describe the proposed port.

| Subsystem | Boundary functions (historical name @ address) | State | Timing semantics | Hardware replaced | Main risk |
|---|---|---|---|---|---|
| Gameplay flow | `run_gameplay_session` @ `0x00010`; `run_main_menu` @ `0x00708`; `run_level` @ `0x06b02`; `main` @ `0x13a95` | Menu/session/level transitions, lives, counters, and exit state | Session logic advances one logical frame at a time using `retrace_tick_count` / `ticklim` | Renderer, events, clock, files, and audio are service boundaries; game rules stay in place | Preserve entry/exit paths and frame order |
| Menus and high scores | `run_main_menu` @ `0x00708`; `enter_high_score_name` @ `0x01e87`; `write_high_score_table` @ `0x02250`; `validate_high_score_records` @ `0x0228e` | Menu selection and 25-byte score records | Menu waits consume keyboard/mouse state and logical ticks | Replace screen, input, and file services | Keep record layout, checksum, restart-code rules, and wait behavior |
| Level assets and HUD | `load_and_draw_level` @ `0x02f9e`; `draw_status` @ `0x035e4`; `draw_level_tile_on_pages` @ `0x03ac0`; `refresh_video_pages` @ `0x03beb` | Level data, tile/page buffers, palette, and status fields | Updates occur within the level/game step | Replace VGA page copies and asset reads | Keep front/back pages and partial tile redraws coherent |
| Timed gameplay events | `process_timed_level_changes` @ `0x03caa`; `tick_level_change_queue` @ `0x03cc9`; `queue_timed_level_change` @ `0x03e0f` | Queued changes, remaining delay, and dispatch state | Queue countdowns use game ticks, not wall-clock seconds | No direct device dependency; feed the same fixed-step service | Preserve queue order and exact tick boundary |
| Gameplay rules | `update_falling_spells` @ `0x04066`; `update_racket_state` @ `0x04cf2`; `update_enemy_projectiles` @ `0x05966`; `update_game_balls` @ `0x085a4` | Entity, collision, bonus, projectile, and level state | Called in the historical per-frame order | No direct hardware; keep the original rules and PRNG | Moving rendering or time work into these routines can change collision/update order |
| Frame and sprite scheduling | `init_tracks` @ `0x082cc`; `queue_draws` @ `0x083b1`; `await_input` @ `0x084a0`; `process_sprite_update_list` @ `0x12a9c`; `show_page` @ `0x0ed38` | Sprite update records and logical draw/display page indices | Waits synchronize input and tick service | Replace VGA update-list raster and page presentation | Preserve update-list ordering, clipping, and page ownership |
| Indexed video and presentation | `set_display_mode` @ `0x0e095`; `set_video_display_address` @ `0x0e914`; `load_palette` @ `0x0608a`; `draw_bob_sprite` @ `0x12cbd`; `copy_screen_span` @ `0x12f9c`; `read_vga_pixel` @ `0x13324`; `write_vga_pixel` @ `0x133f6`; `draw_text` @ `0x0b1df` | Indexed pixels, `vga_state`, page indices, viewport, plane masks, palette, font, and clip state | Presentation follows logical updates; fades/cycles update palette values | Replace VGA ports, planar memory, DAC, BIOS mode calls, and glyph pixel writes with software pages, palette conversion, and SDL3 texture upload | Match planar/page geometry, transparency, clipping, palette cycling, and captures |
| Input: game actions | `handle_gameplay_keypress` @ `0x03e75`; `handle_keyboard_controls` @ `0x0814b` | Game actions consume scan/ASCII bytes, key bitmaps, chord/repeat state, and hooks | Dispatched at input service boundaries | SDL events supply the legacy input state | Preserve action mapping, repeat, and exit/abort chords |
| Input: keyboard | `install_keyboard_input_handler` @ `0x0f6a1`; `read_keyboard_scan_code` @ `0x0f8bb`; `update_key_state_from_scan_code` @ `0x0f905`; `dispatch_keyboard` @ `0x0fb17` | `scan_code_bitmap`, `translated_key_bitmap`, scan/ASCII state, lock flags, repeat, and action hooks | Pump events at the same service boundaries; preserve game repeat state | Replace IRQ1, ports `0x60`/`0x64`, PIC EOI, and BIOS keyboard memory | DOS make/break mapping, ASCII entry, repeat, pause, and chord behavior |
| Input: pointer and joystick | `update_mouse` @ `0x10137`; `poll_joystick_ports` @ `0x0efa0`; `update_joystick_directions` @ `0x0f0f6`; `recalculate_joystick_thresholds` @ `0x0f46c` | Mouse coordinates/buttons and smoothing history; joystick centers, extrema, thresholds, and button history | Mouse history updates with events; joystick state updates at the original poll/service points | Replace INT `0x33` and RC timing on gameport `0x201` with SDL mouse/controller events | Preserve bounds, sensitivity, calibration, and threshold policy |
| Timing | `wait_for_tick` @ `0x09d40`; `process_timer_events` @ `0x09e54`; `measure_pit_channel0` @ `0x09f64`; `pit_channel0_interrupt` @ `0x0a067`; `a_e0` @ `0x000e0` | `ticklim` is the logical count; measured `timer_delta` units pace periodic callback records | Source cadence is measured against VGA retrace; the initial `0xffff` reload is setup state, not the steady game rate | Replace PIT ports `0x40`/`0x43`, VGA status polling, IRQ vectors, and PIC EOI with monotonic fixed-step ticks | Preserve retrace cadence, callback order, and tick consumers across different display refresh rates |
| Audio | `queue_audio` @ `0x0c14b`; `transfer_audio_stream_block` @ `0x0c3fb`; `parse_protracker_module` @ `0x11530`; `a_0` @ `0x00000`; `a_70` @ `0x00070` | Request queue, `0x500`-byte buffer with `0x280`-byte halves, mixer source, and ProTracker order/pattern/sample/channel state | Module period is derived from requested sample rate; tracker BPM/ticks-per-row remain musical time | Replace DSP ports, DMA programming, IRQ templates/vectors, and physical buffers with an SDL3 stream | Buffer ownership/races, request order, end padding, sample-rate changes, and BPM tick behavior |
| Assets and decoders | `load_assets` @ `0x02da3`; `decode_picture` @ `0x0a658`; `decode_and_verify_asset` @ `0x0982c`; `decode_iff_ilbm_image` @ `0x0a284`; `decode_gif_image_entry` @ `0x11df8` | File cursor/work arena, decoder workspace/error state, dimensions, indexed pixels, and palette | Format transforms do not have independent game-frame timing | Replace DOS paths, DPMI work memory, and int-encoded pointers with bounded AssetStore spans; keep C decoders and translate assembly checksum/IFF/GIF routines | Preserve bounds, supported format subset, palette, output size, and errors |
| File I/O and persistence | `get_file_length` @ `0x105c2`; `read_file_with_decoder` @ `0x1065b`; `write_file_buffer` @ `0x107b6`; `load_next_file` @ `0x1085a` | Filename, length, bytes written, nested load cursor, four-byte alignment, and legacy `0x201`..`0x206` results | No frame timing dependency | Replace DOS path assumptions with host streams | Keep seek offsets, partial read/write results, cursor alignment, and legacy error values |
| System and memory | `sys_report` @ `0x108a9`; `sysinit` @ `0x10d72`; `check_hardware` @ `0x11051`; `alloc_heap_block` @ `0x0ddb9`; `allocate_dpmi_memory` @ `0x0dea6` | Startup diagnostics, capability fields, handles/selectors, conventional-memory state, and callback records | Initialization/shutdown and callback lifecycle | Replace DOS/BIOS/XMS/EMS/VCPI/DPMI and vector services with host queries and ordinary allocation | Retain useful diagnostics and fatal behavior without requiring DOS hardware |
| Procedural image and randomness | `configure_height_generation` @ `0x0acdd`; `generate_heightfield` @ `0x0ad63`; `perturb_height_midpoint` @ `0x0b03c`; `seed_random_generator` @ `0x0dce0`; `random_in_range` @ `0x0dd53` | Height bounds/noise, indexed samples, and two-word random state | No timer; exact PRNG call order matters | Replace VGA pixel reads/writes with indexed software-page access | Preserve recursion/sample order, clamping, floating-point operations, and the random sequence |

## Game state machine and frame order

`main` (`0x13a95`) initializes memory and display/audio hooks, installs `decode_and_verify_asset` as the file decode hook, registers image pages, then enters `run_main_menu`. The menu dispatcher selects title, menu/order, instruction, high-score, ending, or gameplay screens.

`run_gameplay_session` prepares the run, loads the board/assets, resets a round, and loops through level play. `next_lvl` selects a regular board or `run_level` encounter. A lost life returns to `init_round` while lives remain; exhausted lives clean up to the menu/game-over path. Completed runs may show ending pages. `run_level` has its own encounter loop. The source initializes `remaining_level_time` to 100 and projectile damage reduces it; it is an encounter meter, not wall-clock time.

For each gameplay frame, `run_gameplay_session` synchronizes input/ticks, handles the S-key action, increments the frame count, then updates racket movement, racket/effect state, balls, shot cooldown, falling spells, animated sprites, enemy projectiles, and timed level changes. It advances `retrace_tick_count` to `ticklim`, redraws the indexed image region, checks image errors, updates the status panel, and may release the racket. It then tests completion; when play continues it handles keyboard controls, presents a page, and checks arcade/life-loss conditions.

The encounter order is `wait_for_tick(3)`, HUD update, S-key handling, enemy attack cycle, auxiliary projectiles, shields, animated sprites, image redraw/error check, page presentation, then level-intro ticks/effects and completion/input checks. Preserve both loops and their order; SDL render cadence must not become the simulation cadence.

## Class counts

These counts use the merged 378-record map after overlap reconciliation. `KEEP` retains portable behavior; `ADAPT` connects it to a host service; `REIMPLEMENT` replaces hardware/interrupt code or translates hand-written assembly.

| Subsystem | KEEP | ADAPT | REIMPLEMENT | Total |
|---|---:|---:|---:|---:|
| assets and decoders | 0 | 6 | 4 | 10 |
| audio | 5 | 5 | 27 | 37 |
| file I/O and persistence | 1 | 4 | 0 | 5 |
| frame and sprite scheduling | 3 | 2 | 0 | 5 |
| gameplay flow | 3 | 2 | 0 | 5 |
| gameplay rules | 68 | 14 | 0 | 82 |
| indexed video and presentation | 20 | 34 | 33 | 87 |
| input: game actions | 0 | 2 | 0 | 2 |
| input: keyboard | 8 | 13 | 3 | 24 |
| input: pointer and joystick | 0 | 19 | 0 | 19 |
| level assets and HUD | 2 | 8 | 0 | 10 |
| menus and high scores | 5 | 37 | 1 | 43 |
| procedural image and randomness | 5 | 3 | 0 | 8 |
| system and memory | 1 | 9 | 11 | 21 |
| timed gameplay events | 4 | 0 | 0 | 4 |
| timing | 4 | 3 | 9 | 16 |
| **Total** | **129** | **161** | **88** | **378** |

## Overlap decisions

The worker maps overlap on 30 functions. Platform entries contribute hardware dependencies and direct state references; game entries contribute game/format purpose, calls, and broader state. These seven class disagreements use the portable boundary or implementation work required by the source:

- `decode_and_verify_asset` @ `0x0982c`: **REIMPLEMENT**. The transform is portable, but the entry is a hand-written assembly decoder; translate it while preserving checksum and output behavior.
- `decode_pcx_image` @ `0xa0e0`: **ADAPT**. Keep PCX RLE/palette rules and adapt its shared DOS file/work-buffer ownership to bounded host buffers.
- `decode_iff_ilbm_image` @ `0x0a284`: **REIMPLEMENT**. Translate the hand-written IFF/ILBM decoder while preserving indexed output and errors.
- `find_iff_chunk` @ `0x0a4e1`: **REIMPLEMENT**. Translate this exported helper with the hand-written IFF assembly module.
- `draw_zero_padded_number` @ `0x0b4a7`: **ADAPT**. Keep formatting and route glyph writes through the indexed renderer.
- `decode_game_bitmap` @ `0x0c685`: **ADAPT**. Keep bitmap/palette conversion and adapt its shared image/workspace buffers.
- `decode_gif_image_entry` @ `0x11df8`: **REIMPLEMENT**. Translate the hand-written GIF LZW state machine; adapt its wrapper and workspace separately.

## Ordered migration plan

1. Create the portable SDL3 shell and host file/memory services. Keep the historical source tree untouched; establish window lifecycle, asset paths, bounded buffers, and a placeholder audio sink.
2. Port the 8-bit indexed framebuffer: two pages, page indices, observed planar/plane-mask behavior, viewport/clipping, BOB transparency, update lists, palette conversion, and SDL3 texture presentation. Move text and screen capture onto this sink.
3. Bring up asset loading. Keep original C PCX/GIF wrappers and format rules; translate `decode_and_verify_asset`, IFF/ILBM, and GIF assembly entries; preserve palette, pixel, size, and error contracts.
4. Add SDL3 event adapters for key make/break state, ASCII input, mouse history, and joystick calibration/direction fields. Keep keyboard action/repeat/chord rules in game code.
5. Add a monotonic fixed-step clock that supplies legacy tick state and callback order. Connect each wait/service boundary, then exercise render rates above and below the logical rate.
6. Compile the existing menu/gameplay logic against those seams; first reach the title/menu and one playable board with sound disabled, then exercise level transitions, game-over, high scores, and screen capture.
7. Add the SDL3 audio stream. Feed the preserved request mixer and ProTracker sequencer where portable; match rate changes, request ordering, two-half refill, and musical timing.
8. Replace startup hardware diagnostics and DOS-only memory/vector services with host lifecycle/capability behavior; retain useful user-visible messages.

## Evidence and scope

`manifest.json` is the source for function identities and current names. The worker maps provide call/state evidence and original boundary descriptions; source and `asm/irq.asm` corroborate the object-2 IRQ templates. Listed platform operations are observed in source/disassembly. SDL3 services, replacement boundaries, and risks are migration proposals. Decoder rewrites and class resolutions are recorded per function in `docs/porting.json`.
