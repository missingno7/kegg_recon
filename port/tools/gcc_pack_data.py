"""Compiler launcher for the historical units: byte-packed, declaration-ordered data.

Watcom 10.0 places a unit's _DATA/_BSS variables back to back with no alignment padding,
and the game relies on it: several records are declared as consecutive globals and then
accessed through one struct pointer (e.g. the 57-byte interrupt records key_irq/tmr_rec
in src/u_0d4ba.c, docs/port/architecture.md "Data layout"). gcc aligns every global to
its type and emits _BSS in declaration order. This launcher (CMake C_COMPILER_LAUNCHER of
target ke_game) compiles to assembly, removes .data/.bss padding, reorders each unit's
_BSS by the frozen Watcom order measured with tools/bssorder.py, and assembles:

    python gcc_pack_data.py <gcc> <args ... -o OUT.obj -c SRC.c>

Together with -fno-toplevel-reorder (source order) and -fno-zero-initialized-in-bss
(`= 0` variables stay in .data next to their neighbours, as in Watcom's _DATA), the unit's
data layout matches the original (port/tools/check_layouts.py --data checks both segments).
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

DATA_SECTIONS = re.compile(r"^\s*\.(data|bss)\b|^\s*\.section\s+\.(data|bss)")
OTHER_SECTION = re.compile(r"^\s*\.(text|section)\b")
ALIGN = re.compile(r"^\s*\.(align|p2align|balign)\b")
SECTION = re.compile(r"^\s*\.section\s+([^\s,]+)|^\s*\.(text|data|bss)\b")
GLOBAL = re.compile(r"^\s*\.globl\s+([A-Za-z_.$][\w.$]*)\b")
LABEL = re.compile(r"^\s*([A-Za-z_.$][\w.$]*):")

# Per-TU public BSS order measured from the frozen sources with tools/bssorder.py.
BSS_ORDER_TEXT = """
t06 pit_tick_accumulator5 timer_events4o timer_state_word retrace_spin_count8 pit_counter_snapshot2v timer_error_hundredths0z timer_enabled09 save_cur retrace_tick_count retrace_count5 ticklim
t07 pcx_allocation_size pcx_unresolved_state_word_a8 pcx_unresolved_state_word_bi pcx_pixel_and_palette_payload_bytes
t08 picture_pixels8 pic_of picture_width8 picture_height0 picture_x5 picture_y9n picture_bytes77
t10 decoded_bitmap_bytes decoded_image_bytes
t15_bob primary_joystick_one_frame_ago secondary_joystick_one_frame_ago player_one_joystick_two_frames_ago secondary_joystick_two_frames_ago primary_joystick secondary_stick_state secondary_stick_three_frames_prior player_one_joystick_three_frames_back
t16_keyboard scan_code_bitmap translated_key_bitmap pending_ascii_key previous_key_scan_code latest_scan_byte current_scan_code keyboard_bios_status_flags prior_key_ascii keyboard_scan_byte current_ascii
t17_mouse saved_mouse_sensitivity_threshold previous_mouse_sensitivity_y previous_mouse_sensitivity_x mouse_x_measurement_5 vertical_mouse_value_5 cursor_y_7_value mouse_reading_x_7 pointer_horizontal_sample_6 vertical_mouse_value_6 mouse_reading_x_4 vertical_sample_mouse_4 mouse_driver_speed_threshold x_cursor_sensitivity mouse_y_3_sample x_mouse_sample_3 x_cursor_history_2 vertical_mouse_frame_2 mouse_x_sample_8 mouse_y_sample_1 mouse_y_smooth_average x_mouse_sample_1 history_mouse_x_0 mouse_y_sample_0 previous_smoothed_mouse_x previous_y_average mouse_sensitivity_y_axis mouse_y_mean_recent mouse_x_average_recent
t18_file n_read current_file_size_bytes file_buf_ptr current_file_name file_output_bytes_written
t19_sys fatal_error_code fatal_message_pointer
u_00010 screen_palette_buffer next_extra_life_score sprite_frame_pointer
u_00708 current_spell_effect_handler sprite_frame_offset_scratch high_score_checksum_byte high_score_record_index high_score_char_index
u_02f0c result g_8de0 parity decade scratch
u_04066 restart_word falling_spell_state_word ptrbuf work_value temp frames falling_spell_state_bytes input_char falling_spell_state_byte0
u_0608a level_art_base
u_06b02 auxiliary_projectiles open_brick_x_by_row level_intro_ticks enemy_attack_interval enemy_shot_countdown enemy_fire_acceleration bolt_cursor old_display_mode projectile_index hud_flash_ticks enemy_projectile_count attack_cooldown remaining_level_time last_displayed_enemy_health attack_frame_count attack_delay total_level_time enemy_max_health enemy_health_current enemy_pattern last_displayed_time enemy_attack_timer
u_07bd5 sprite_animations timed_change_records player_shot_records brick_code_map moving_target_records spell_slots falling_spells front_page_bufs back_page_queue sprite_commands vga_buffer_base sfx_data_ptr sprite_memory_base game_sprite_base current_brick_code timed_event_cursor brick_x_index brick_y_index portal_start_x portal_y_source portal_exit_xpos sprite_current portal_destination_y timed_change_count remaining_brick_count timed_change_cursor cell_cursor sprite_instance_count
u_07f8a sprite_removal_index player_shot_cursor moving_target_cursor player_key_flag_storage enemy_cursor player_shot_index falling_spell_cursor current_ball_count game_ball_slot enemy_spawn_wait_time current_ball_pointer motion_dir spell_cursor spell_count moving_target_number player_shot_total moving_target_count enemy_timer palette_base tile_art_base
u_0814b score_state paddle_state_data player_key_flags racket_state_storage points game_art_base
u_08585 racket_object g_dee8_d0 score_storage monster_art brick_art_start level_data_cursor enemy_picture fill_sprite_data spell_art_base tileid level_art_load_base file_mark code_index palette_cycle_offset palette_cycle_delay prompt palette_entries prompt_timeout menu_result menu_frame cursor_direction high_score_name_timer screen_timer menu_sprite_frame_table restart_code_entry_state game_balls life_lost_flag arcade lives high_score_input_redraw_flag difficulty_tier_index done start_decade high_score_cursor bonus_index level_number
u_095ee codeok path_count tick transition_track_data sprite_base sprite_metadata x_delta y_offset width height
u_0acdd heightfield_unreferenced_bytes height_midpoint_value height_noise_scale
u_0b1df font_glyph_metric_table font_bitmap_data text_render_state
u_0b804 collision_box_b_left b_box_top collision_box_b_right collision_rect_b_bottom sprite_rect_c_left collision_box_c_top collision_box_c_right collision_rect_c_bottom collision_box_a_left collision_box_a_top sprite_rect_a_right sprite_bounds_a_bottom collision_box_d_left sprite_bounds_d_top
u_0c14b audio_dma_memory audio_request_entries module_player_state_a5 module_player_state_b5 module_player_state_c5 module_player_state_d8 module_player_state_e5 module_player_state_f5 module_player_state_g5 module_player_state_hk module_player_state_ie module_player_state_je module_player_state_ke module_player_state_l5
u_0c886 gif_decoded_image_state iff_width_pixels iff_height_pixels iff_output_byte_count iff_decoded_pixel_count front_image_page_buffer image_page_back_buffer image_update_list_start screen_page_base render_page_base active_video_page_buffer image_buffer_cursor
u_0cac2 sound_dma_test_result sound_irq_test_flag saved_sound_mixer_value sound_blaster_base_port sound_setup_reserved_word
u_0d2f0 xms_entry_offset xms_entry_segment
u_0d4ba dpmi_entry_selector dpmi_private_data_paragraphs dpmi_entry_offset
u_0ddb9 heap_block
u_0dea6 dpmi_dos_segment dpmi_selector_or_failure_marker dpmi_segment_linear_base
u_0dfc3 vga_state
"""
BSS_ORDER = {
    fields[0]: fields[1:]
    for line in BSS_ORDER_TEXT.splitlines()
    if (fields := line.split())
}

ROOT = Path(__file__).resolve().parents[2]


def original_bss_offsets() -> dict[str, int]:
    """Return canonical object-3 BSS symbol offsets, keyed by the C symbol name."""
    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    bss_start = int(manifest["le"]["objects"][2]["init"])
    return {
        name: int(value.split(":", 1)[1], 16)
        for name, value in manifest["symbols"].items()
        if value.startswith("3:") and int(value.split(":", 1)[1], 16) >= bss_start
    }


def reorder_bss(lines: list[str], offsets: dict[str, int], expected_order=None) -> list[str]:
    """Put BSS globals in the measured Watcom order, with a manifest fallback.

    GCC emits uninitialised globals in declaration order. Watcom orders its _BSS by a
    compiler hash, so declaration order does not reproduce the original layout. The
    per-TU table covers recovered public names too; unknown symbols keep their slots when
    a future TU has no measured table. The LE manifest is the fallback authority.
    """
    sections: list[str | None] = []
    section = None
    for line in lines:
        match = SECTION.match(line)
        if match:
            name = (match.group(1) or match.group(2)).lstrip(".")
            section = name if name in {"text", "data", "bss"} else "other"
        sections.append(section)

    globals_at = [i for i, line in enumerate(lines) if GLOBAL.match(line)]
    chunks = []
    for global_pos, start in enumerate(globals_at):
        limit = globals_at[global_pos + 1] if global_pos + 1 < len(globals_at) else len(lines)
        symbol = GLOBAL.match(lines[start]).group(1)
        label_pos = next(
            (i for i in range(start + 1, limit)
             if (match := LABEL.match(lines[i])) and match.group(1) == symbol),
            None,
        )
        if label_pos is None or sections[label_pos] != "bss":
            continue
        end = limit
        for i in range(label_pos + 1, limit):
            if SECTION.match(lines[i]) and sections[i] != "bss":
                end = i
                break
        chunks.append({"start": start, "end": end, "symbol": symbol, "lines": lines[start:end]})

    if not chunks:
        return lines

    def c_name(symbol: str) -> str:
        return symbol[1:] if symbol.startswith("_") else symbol

    if expected_order is not None:
        expected = {name: i for i, name in enumerate(expected_order)}
        actual = {c_name(chunk["symbol"]) for chunk in chunks}
        if actual != set(expected):
            missing = sorted(set(expected) - actual)
            added = sorted(actual - set(expected))
            raise ValueError(f"BSS order map mismatch: missing {missing}, added {added}")
        mapped_slots = list(range(len(chunks)))
        mapped = sorted(chunks, key=lambda chunk: expected[c_name(chunk["symbol"])])
    else:
        mapped_slots = [i for i, chunk in enumerate(chunks)
                        if c_name(chunk["symbol"]) in offsets]
        mapped = sorted(
            (chunks[i] for i in mapped_slots),
            key=lambda chunk: (offsets[c_name(chunk["symbol"])], chunk["start"]),
        )
    arranged = list(chunks)
    for slot, chunk in zip(mapped_slots, mapped):
        arranged[slot] = chunk

    first = min(chunk["start"] for chunk in chunks)
    removed = set()
    for chunk in chunks:
        removed.update(range(chunk["start"], chunk["end"]))

    out = []
    for i, line in enumerate(lines):
        if i == first:
            for chunk in arranged:
                out.append("\t.bss")
                out.extend(line for line in chunk["lines"] if not SECTION.match(line))
        if i not in removed:
            out.append(line)
    return out


def pack(asm: str, source: str | None = None) -> str:
    out, in_data = [], False
    for line in asm.splitlines():
        if DATA_SECTIONS.match(line):
            in_data = True
        elif OTHER_SECTION.match(line):
            in_data = False
        if in_data and ALIGN.match(line):
            continue
        out.append(line)
    expected_order = BSS_ORDER.get(Path(source).stem) if source else None
    return "\n".join(reorder_bss(out, original_bss_offsets(), expected_order)) + "\n"


def main(argv):
    cmd = list(argv)
    if "-c" not in cmd or "-o" not in cmd:
        return subprocess.call(cmd)
    out = Path(cmd[cmd.index("-o") + 1])
    asm = out.with_suffix(".packed.s")
    s_cmd = [a for a in cmd]
    s_cmd[s_cmd.index("-c")] = "-S"
    s_cmd[s_cmd.index("-o") + 1] = str(asm)
    rc = subprocess.call(s_cmd)
    if rc:
        return rc
    source = next((a for a in reversed(cmd) if a.lower().endswith(".c")), None)
    asm.write_text(pack(asm.read_text(), source))
    return subprocess.call([cmd[0], "-c", str(asm), "-o", str(out)])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
