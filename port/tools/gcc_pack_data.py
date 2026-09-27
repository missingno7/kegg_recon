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
import hashlib
import ast
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
CONST_OWNER = "u_00010.c"


def const3_symbol(offset: int) -> str:
    return f"__ke_original_const3_{offset:08x}"

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


def original_const_layout():
    """Recover each C unit's original CONST byte range and referenced offsets.

    The LE image is the authority for both literal placement and -ot padding. Manifest
    CONST placements seed units whose start is recorded explicitly; the original code
    fixups and initialized pointer fixups locate the remaining units' constants.
    """
    if str(ROOT) not in sys.path:
        sys.path.insert(0, str(ROOT))
    from tools.le import LE

    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    exe_path = ROOT / "assets" / "KE.EXE"
    exe = exe_path.read_bytes()
    expected_hash = manifest["original"]["KE.EXE"]["sha256"]
    actual_hash = hashlib.sha256(exe).hexdigest()
    if actual_hash != expected_hash:
        raise ValueError(f"original CONST source hash mismatch: {actual_hash} != {expected_hash}")
    le = LE(exe_path)
    original = le.object_bytes(le.objects[2])
    fixups = {(obj, off): (kind, tgt) for obj, off, kind, tgt in le.resolved_fixups()}
    symbol_locations = {}
    for name, value in manifest["symbols"].items():
        if value.startswith(("1:", "2:", "3:")):
            obj, off = value.split(":", 1)
            symbol_locations[name] = (int(obj), int(off, 16))
    symbol_offsets = {name: off for name, (obj, off) in symbol_locations.items() if obj == 3}

    const_limit = min(
        int(place.split("=", 1)[1], 16)
        for unit in manifest["units"]
        for place in unit.get("place", [])
        if place.startswith("_DATA=")
    )
    source_units: dict[str, list[tuple[int, int]]] = {}
    explicit_starts: dict[str, int] = {}
    for unit in manifest["units"]:
        source = Path(unit["src"]).name
        source_units.setdefault(source, []).append(
            (int(unit["start"], 16), int(unit["end"], 16))
        )
        for place in unit.get("place", []):
            if place.startswith("CONST="):
                explicit_starts[source] = int(place.split("=", 1)[1], 16)

    # Associate initialized globals with their defining C unit. Only file-scope
    # declarations are considered; a source reference inside a function cannot claim
    # ownership of another unit's data.
    owners: dict[str, str] = {}
    known_symbols = set(symbol_offsets)
    for source_path in (ROOT / "src").glob("*.c"):
        source_text = source_path.read_text(encoding="utf-8", errors="replace")
        source_text = re.sub(r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
                             " ", source_text, flags=re.S)
        depth = 0
        for line in source_text.splitlines():
            if depth == 0 and "=" in line and not line.lstrip().startswith("#"):
                lhs = line.split("=", 1)[0]
                if "(" not in lhs and "extern" not in lhs.split():
                    for name in re.findall(r"[A-Za-z_]\w*", lhs):
                        if name in known_symbols:
                            owners[name] = source_path.name
            depth += line.count("{") - line.count("}")
            if depth < 0:
                depth = 0

    targets: dict[str, dict[int, set[int]]] = {
        name: {1: set(), 3: set()} for name in source_units
    }
    # C code is in LE object 1; each manifest range is the historical function range.
    for (obj, src_off), (_kind, tgt) in fixups.items():
        if obj != 1 or tgt.get("kind") != "internal" or tgt.get("obj") not in (1, 3):
            continue
        target_obj = tgt["obj"]
        target = tgt.get("off", 0xffffffff)
        if target_obj == 3 and target >= const_limit:
            continue
        for source, spans in source_units.items():
            if any(start <= src_off < end for start, end in spans):
                targets[source][target_obj].add(target)

    # Global pointer initializers live in object 3. Attribute them to the source file
    # that defines the symbol, including pointer arrays (all fixups up to the next
    # defined global belong to that object's initialized-data contribution).
    owned_symbols: dict[str, list[tuple[str, int]]] = {}
    for name, source in owners.items():
        off = symbol_offsets[name]
        owned_symbols.setdefault(source, []).append((name, off))
    for source, symbols in owned_symbols.items():
        symbols.sort(key=lambda item: item[1])
        for i, (_name, start) in enumerate(symbols):
            end = symbols[i + 1][1] if i + 1 < len(symbols) else const_limit
            for (obj, src_off), (_kind, tgt) in fixups.items():
                if obj == 3 and start <= src_off < end and tgt.get("kind") == "internal" and tgt.get("obj") == 3:
                    target = tgt.get("off", const_limit)
                    if target < const_limit:
                        targets.setdefault(source, {1: set(), 3: set()})[3].add(target)

    # Public const aggregates may themselves be placed in the historical code object.
    # Their symbol offsets are layout evidence even when no pointer fixup is present.
    for source, symbols in owned_symbols.items():
        for name, _off in symbols:
            location = symbol_locations.get(name)
            if location and location[0] == 1 and location[1] < le.objects[0]["vsize"]:
                targets.setdefault(source, {1: set(), 3: set()})[1].add(location[1])

    starts = dict(explicit_starts)
    for source, offsets in targets.items():
        const_offsets = offsets.get(3, set())
        if source not in starts and const_offsets:
            starts[source] = min(const_offsets)
    ordered = sorted((start, source) for source, start in starts.items() if start < const_limit)
    ranges = {}
    for i, (start, source) in enumerate(ordered):
        end = next((later for later, _ in ordered[i + 1:] if later > start), const_limit)
        if end <= start:
            raise ValueError(f"invalid original CONST range for {source}: {start:#x}..{end:#x}")
        ranges[source] = (start, end)
    object_data = [le.object_bytes(obj) for obj in le.objects]
    return (original[:const_limit], const_limit, ranges, targets, symbol_offsets,
            fixups, object_data, symbol_locations)


def _assembly_const_records(lines: list[str]):
    """Collect labels and bytes from all GCC .rdata contributions in assembly order."""
    records = []
    in_const = False
    current = None
    for line in lines:
        section = SECTION.match(line)
        if section:
            name = (section.group(1) or section.group(2)).lstrip(".")
            if name == "rdata":
                in_const = True
                current = None
                continue
            if in_const:
                in_const = False
                current = None
        if not in_const:
            continue
        label = LABEL.match(line)
        if label:
            current = {"name": label.group(1), "bytes": bytearray(), "known": True}
            records.append(current)
            continue
        if current is None:
            continue
        stripped = line.strip()
        if ALIGN.match(line) or not stripped or stripped.startswith(("#", ".cfi", ".loc")):
            continue
        match = re.match(r"\.(ascii|string)\s+(.+)$", stripped)
        if match:
            try:
                value = ast.literal_eval(match.group(2))
                if not isinstance(value, str):
                    raise ValueError("not a string")
                current["bytes"].extend(value.encode("latin-1"))
                if match.group(1) == "string":
                    current["bytes"].append(0)
            except (SyntaxError, ValueError, UnicodeEncodeError):
                current["known"] = False
            continue
        match = re.match(r"\.(byte|2byte|word|short|long|4byte|quad|8byte)\s+(.+)$", stripped)
        if match:
            widths = {"byte": 1, "2byte": 2, "word": 2, "short": 2,
                      "long": 4, "4byte": 4, "quad": 8, "8byte": 8}
            width = widths[match.group(1)]
            try:
                for item in match.group(2).split(","):
                    value = int(item.strip(), 0) & ((1 << (width * 8)) - 1)
                    current["bytes"].extend(value.to_bytes(width, "little"))
            except ValueError:
                current["known"] = False
            continue
        match = re.match(r"\.(zero|space)\s+(\d+)", stripped)
        if match:
            current["bytes"].extend(bytes(int(match.group(2))))
            continue
        if stripped.startswith((".type", ".size", ".def", ".endef")):
            continue
        # CONST records must be byte-addressable data. Failing here prevents a
        # relocation-bearing table from being silently copied without its fixups.
        current["known"] = False
    return records


def pack_constants(asm: str, source: str | None) -> str:
    """Place a unit's CONST labels at original offsets in a writable data section."""
    lines = asm.splitlines()
    if not any(re.match(r"^\s*\.section\s+\.rdata(?:,|\s|$)", line) for line in lines):
        return asm
    if not source:
        raise ValueError("cannot lay out CONST without the historical source unit")

    (original, const_limit, ranges, source_targets, symbol_offsets, fixups,
     object_data, symbol_locations) = original_const_layout()
    source_name = Path(source).name
    start, end = ranges.get(source_name, (0, 0))
    targets = source_targets.get(source_name, {1: set(), 3: set()})
    records = _assembly_const_records(lines)
    if not records:
        raise ValueError(f"{source_name} has a .rdata section but no addressable CONST labels")

    # Resolve initializer labels directly through the original object's pointer fixups.
    label_targets: dict[str, tuple[int, int]] = {}
    in_data = False
    current_symbol = None
    current_orig = None
    for line in lines:
        section = SECTION.match(line)
        if section:
            name = (section.group(1) or section.group(2)).lstrip(".")
            in_data = name == "data"
            current_symbol = current_orig = None
            continue
        if not in_data:
            continue
        label = LABEL.match(line)
        if label:
            raw_name = label.group(1).lstrip("_")
            current_symbol = raw_name
            current_orig = symbol_offsets.get(raw_name)
            continue
        if current_orig is None:
            continue
        directive = re.match(r"^\s*\.(byte|2byte|word|short|long|4byte|quad|8byte)\s+(.+)$", line)
        if not directive:
            continue
        widths = {"byte": 1, "2byte": 2, "word": 2, "short": 2,
                  "long": 4, "4byte": 4, "quad": 8, "8byte": 8}
        width = widths[directive.group(1)]
        for item in directive.group(2).split(","):
            target = re.fullmatch(r"\s*([A-Za-z_.$][\w.$]*)(?:\s*\+\s*(0x[0-9a-fA-F]+|\d+))?\s*", item)
            if target:
                name = target.group(1)
                kind_and_target = fixups.get((3, current_orig))
                if width == 4 and kind_and_target:
                    target_obj = kind_and_target[1]
                    if target_obj.get("kind") == "internal" and target_obj.get("obj") == 3:
                        offset = target_obj.get("off", const_limit)
                        if offset < const_limit:
                            prior = label_targets.get(name)
                            if prior is not None and prior != (3, offset):
                                raise ValueError(f"conflicting CONST targets for {name} in {source_name}")
                            label_targets[name] = (3, offset)
            current_orig += width

    def matching_bytes(obj: int, off: int, payload: bytes) -> bool:
        data = object_data[obj - 1]
        if data[off:off + len(payload)] == payload:
            return True
        # Some decompiled printf literals omit only a final LF in GCC's pool. The
        # original bytes and fixup target remain authoritative for the emitted span.
        return (len(payload) > 1 and payload.endswith(b"\0") and
                data[off:off + len(payload) - 1] == payload[:-1] and
                data[off + len(payload) - 1] in (0x0a, 0x0d))

    for record in records:
        if record["name"] in label_targets:
            continue
        named = record["name"].lstrip("_")
        if named in symbol_locations:
            obj, off = symbol_locations[named]
            if obj in (1, 3):
                label_targets[record["name"]] = (obj, off)
                continue
        payload = bytes(record["bytes"])
        if not payload or not record["known"]:
            continue
        candidates = []
        for obj in (3, 1):
            if obj == 3:
                offsets = {off for off in targets.get(3, set()) if off < const_limit}
            else:
                offsets = targets.get(1, set())
            candidates.extend((obj, off) for off in offsets if matching_bytes(obj, off, payload))
        if not candidates:
            candidates.extend((3, off) for off in range(0, max(0, const_limit - len(payload) + 1))
                              if matching_bytes(3, off, payload))
        if len(candidates) == 1:
            label_targets[record["name"]] = candidates[0]

    # Remaining duplicate literals are selected in original order. Translation units
    # deduplicate equal literals, so an unresolved duplicate is an actual ambiguity.
    previous = {1: -1, 3: start - 1}
    for record in records:
        if record["name"] in label_targets:
            obj, off = label_targets[record["name"]]
            previous[obj] = max(previous[obj], off)
            continue
        payload = bytes(record["bytes"])
        candidates = []
        for obj in (3, 1):
            offsets = targets.get(obj, set())
            candidates.extend((obj, off) for off in offsets
                              if off >= previous[obj] and matching_bytes(obj, off, payload))
        if not candidates and payload:
            candidates.extend((3, off) for off in range(0, max(0, const_limit - len(payload) + 1))
                              if off >= previous[3] and matching_bytes(3, off, payload))
        if len(candidates) != 1:
            raise ValueError(f"cannot map CONST label {record['name']} in {source_name}; "
                             f"{len(candidates)} original locations match")
        label_targets[record["name"]] = candidates[0]
        obj, off = candidates[0]
        previous[obj] = off

    for record in records:
        obj, off = label_targets[record["name"]]
        payload = bytes(record["bytes"])
        if obj == 3:
            if off >= const_limit or off + len(payload) > const_limit:
                raise ValueError(f"CONST label {record['name']} exceeds original object data in {source_name}")
            if record["known"] and payload and not matching_bytes(obj, off, payload):
                raise ValueError(f"CONST label {record['name']} does not match original bytes in {source_name}")
        elif record["known"] and payload and not matching_bytes(obj, off, payload):
            raise ValueError(f"CONST label {record['name']} does not match original object {obj} bytes")

    if source_name == CONST_OWNER:
        source_fixups = [off for (obj, off), (_kind, _target) in fixups.items()
                         if obj == 3 and off < const_limit]
        if source_fixups:
            raise ValueError(f"original CONST data has relocation sites at {source_fixups[:8]}; "
                             "byte copy needs relocation handling")

    emitted = []
    aliases: dict[int, list[str]] = {}
    for name, (obj, off) in label_targets.items():
        if obj == 3:
            aliases.setdefault(off, []).append(name)
    if source_name == CONST_OWNER:
        const_offsets = {
            target.get("off", const_limit)
            for (_obj, _off), (_kind, target) in fixups.items()
            if target.get("kind") == "internal" and target.get("obj") == 3
            and 0 <= target.get("off", const_limit) < const_limit
        }
        const_offsets.update(
            off for location_obj, off in symbol_locations.values()
            if location_obj == 3 and 0 <= off < const_limit
        )
        const_offsets.update(
            off for unit_targets in source_targets.values() for off in unit_targets.get(3, set())
            if 0 <= off < const_limit
        )
        const_offsets.update(aliases)
        emitted.append('\t.section .data$KECONST3$00000000,"dw"')
        emitted.append(f"# KE_CONST_SECTION 3 0x0 {const_limit:#x}")
        cursor = 0
        for off in sorted(const_offsets):
            while cursor < off:
                count = min(off - cursor, 16)
                emitted.append("\t.byte " + ",".join(f"0x{b:02x}" for b in original[cursor:cursor + count]))
                cursor += count
            symbol = const3_symbol(off)
            emitted.append(f"# KE_CONST_LABEL 3 {off:#x} {symbol}")
            emitted.append(f"\t.globl {symbol}")
            emitted.append(f"{symbol}:")
        while cursor < const_limit:
            count = min(const_limit - cursor, 16)
            emitted.append("\t.byte " + ",".join(f"0x{b:02x}" for b in original[cursor:cursor + count]))
            cursor += count

    for off, names in sorted(aliases.items()):
        symbol = const3_symbol(off)
        for name in sorted(names):
            emitted.append(f"# KE_CONST_ALIAS 3 {off:#x} {name} {symbol}")
            emitted.append(f"\t.set {name}, {symbol}")

    other_aliases: dict[tuple[int, int], list[str]] = {}
    for name, (obj, off) in label_targets.items():
        if obj != 3:
            other_aliases.setdefault((obj, off), []).append(name)
    for (obj, off), names in sorted(other_aliases.items()):
        payload_size = max(len(bytes(next(r["bytes"] for r in records if r["name"] == name)))
                           for name in names)
        raw = object_data[obj - 1][off:off + payload_size]
        emitted.append(f'\t.section .data$KECONST{obj},"dw"')
        emitted.append(f"# KE_CONST_LABEL {obj} {off:#x} {payload_size:#x} {' '.join(sorted(names))}")
        emitted.extend(f"{name}:" for name in sorted(names))
        for base in range(0, len(raw), 16):
            emitted.append("\t.byte " + ",".join(f"0x{b:02x}" for b in raw[base:base + 16]))

    out = []
    in_const = False
    inserted = False
    for line in lines:
        section = SECTION.match(line)
        if section:
            name = (section.group(1) or section.group(2)).lstrip(".")
            if name == "rdata":
                if not inserted:
                    out.extend(emitted)
                    inserted = True
                in_const = True
                continue
            in_const = False
        if in_const:
            stripped = line.strip()
            if GLOBAL.match(line) or stripped.startswith((".type", ".size", ".def", ".endef")):
                out.append(line)
            continue
        out.append(line)
    return "\n".join(out) + "\n"


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
    asm = pack_constants(asm, source)
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
    try:
        asm.write_text(pack(asm.read_text(), source))
    except (KeyError, OSError, ValueError) as error:
        print(f"gcc_pack_data.py: {error}", file=sys.stderr)
        return 1
    return subprocess.call([cmd[0], "-c", str(asm), "-o", str(out)])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
