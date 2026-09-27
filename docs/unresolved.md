# Unresolved historical source details

The table records the seven distinct address-derived names that remain after the final naming review (nine per-unit occurrences). `PROVEN` describes recovered bytes, types, or uses; unresolved meaning is marked `HYPOTHESIS`.

## Remaining address-derived names

| Name | Defining unit | Kind | What is known | Reason it stays |
|---|---|---|---|---|
| `g_387e` | U00010 | global byte array | **PROVEN:** 12 zero bytes in the high-score data block; no consumer is identified. | **HYPOTHESIS:** purpose is unknown, so a semantic name would invent a role. |
| `g_8de0` | U02f0c | global byte array | **PROVEN:** unreferenced 28-byte object; no owning record or behavior is established. | **HYPOTHESIS:** role is unknown. |
| `g_e46b` | No recovered definition; required extern in U06b02 | external byte declaration | **PROVEN:** the source tree has no definition; the declaration is not read or written in U06b02. Removing it leaves code instructions unchanged but makes the verifier reject `_BSS` symbol-base consistency (the reported candidate bases are `3:8e4c`, `3:910c`, and `3:8e48`). | **PROVEN:** retain the historical spelling because removing even this unused declaration breaks the exact gate; its role remains unknown. |
| `f_4cd0` | U04066 | function | **PROVEN:** empty no-op body at 0x4cd0. | **HYPOTHESIS:** purpose is unknown. |
| `f_4ce1` | U04066 | function | **PROVEN:** empty no-op body at 0x4ce1. | **HYPOTHESIS:** purpose is unknown. |
| `g_dee8_d0` | U08585 | global byte array | **PROVEN:** unreferenced 28-byte object adjacent to game-state globals. | **HYPOTHESIS:** original role is not established. |
| `f_ca51` | U0ca51 | function | **PROVEN:** matched body only tests `vga_state`; declarations also occur in U0c886 and U0ca6a. | **HYPOTHESIS:** original purpose is not established. |

## Remaining gotos (50)

The labels below enter shared tails, jump into existing branches/case bodies, or resume loops at non-template points. The reasons are transcribed from the adjacent source comments; they are kept because Watcom's proven control-flow templates do not produce these layouts without a `goto`.

| Unit | Label | Layout reason from adjacent comment |
|---|---|---|
| U00010 | `session_failed_to_menu` | Shared session cleanup. |
| U00010 | `session_finished` | Exit to shared session cleanup. |
| U00010 | `session_finished` | Arcade completion shares the session tail. |
| U00010 | `session_finished` | User exits arcade play. |
| U00010 | `gameplay_frame_loop` | Resume without reinitializing the round. |
| U00010 | `start_round_after_load` | Retry this round. |
| U00010 | `check_level_code` | Shared code path skips the ordinary increment. |
| U00708 | `menu_wait_complete` | Input leaves both nested polling loops at once. |
| U00708 | `order_screen_poll` | Re-poll this screen without reloading its assets. |
| U04cf2 | `draw_racket_state` | Skip autonomous movement and draw now. |
| U04cf2 | `racket_mouse_control` | Take the manual mouse-control path. |
| U04cf2 | `draw_racket_state` | Skip autonomous movement and draw now. |
| U04cf2 | `select_racket_frame` | Rejoin the shared frame selector. |
| U04cf2 | `select_racket_frame` | Rejoin the shared frame selector. |
| U05966 | `process_existing_projectiles` | Keep active projectiles moving after bonus spawns stop. |
| U05966 | `remove_hit_projectile` | Both successful collision paths share the impact tail. |
| U05966 | `check_racket_shield_collision` | A captured bolt falls through to the shield test. |
| U05966 | `draw_surviving_projectile` | Skip the hit path and draw the surviving bolt. |
| U05966 | `draw_surviving_projectile` | Skip the hit path and draw the surviving bolt. |
| U0608a | `next_player_shot` | A hit removes this shot and skips its remaining update. |
| U0608a | `queue_player_shot_draw` | Queue the shot draw without checking bonus_stage_brick_counts. |
| U0608a | `next_brick_column` | Skip unsupported tile codes. |
| U0608a | `queue_player_shot_draw` | Fall through to the shared draw-record path. |
| U08585 | `queue_current_ball_draw` | Attached balls and portal hits skip collision passes but still queue this ball. |
| U08585 | `check_right_wall_edge` | An inward left-wall pass joins the right-wall test. |
| U08585 | `check_top_wall_edge` | An inward right-wall pass joins the ceiling test. |
| U08585 | `reflect_ceiling_contact` | The ceiling response shares the wall-impact effect tail. |
| U08585 | `check_racket_collision` | Wall handling rejoins the racket collision pass here. |
| U08585 | `play_wall_impact_effect` | Side and ceiling contacts share this impact effect. |
| U08585 | `skip_racket_contact_resolution` | A missed racket edge skips direction handling and rejoins target tests. |
| U08585 | `reflect_right_racket_edge` | The right edge uses the shared side-contact bounce. |
| U08585 | `reflect_left_racket_edge` | The left edge uses the shared side-contact bounce. |
| U08585 | `reflect_racket_center_contact` | A center hit uses the separate spin-adjusted bounce path. |
| U08585 | `skip_racket_contact_resolution` | A missed racket edge skips direction handling and rejoins target tests. |
| U08585 | `skip_racket_contact_resolution` | A missed racket edge skips direction handling and rejoins target tests. |
| U08585 | `reflect_right_racket_edge` | The right edge uses the shared side-contact bounce. |
| U08585 | `reflect_left_racket_edge` | The left edge uses the shared side-contact bounce. |
| U08585 | `process_ball_target_collisions` | Racket outcomes converge before moving-target and brick checks. |
| U08585 | `reflect_racket_side_contact` | Both side edges share the vertical bounce. |
| U08585 | `process_ball_target_collisions` | Racket outcomes converge before moving-target and brick checks. |
| U08585 | `queue_current_ball_draw` | Attached balls and portal hits skip collision passes but still queue this ball. |
| U08585 | `return_no_brick_contact` | Out-of-field, empty, and unsupported cells share the no-contact result. |
| U08585 | `classify_repeated_brick_contact` | A repeated edge cell skips duplicate effects but still classifies bounce direction. |
| U08585 | `return_no_brick_contact` | Out-of-field, empty, and unsupported cells share the no-contact result. |
| U08585 | `process_portal_contact` | Both portal codes share the offset and portal-result path. |
| U08585 | `process_portal_contact` | Both portal codes share the offset and portal-result path. |
| U08585 | `return_no_brick_contact` | Out-of-field, empty, and unsupported cells share the no-contact result. |
| U08585 | `return_collision_result` | Every brick-contact path returns through this shared result tail. |
| U08585 | `return_collision_result` | Every brick-contact path returns through this shared result tail. |
| U0b1df | `newline_control` | Reuse the line-feed case's shared cursor update. |

## Proven per-unit type variant

`EnemyProjectile.animation_sequence` is a `struct SpriteFrame *` in the shared type catalogue, but **PROVEN** as `int` in U05966. `update_enemy_projectiles` subtracts 8 to move one frame in a byte cursor; pointer subtraction would scale by `sizeof(struct SpriteFrame)`. The U05966 comment and exact gate preserve this unit-specific form. See [docs/types.md](types.md#enemyprojectile).
