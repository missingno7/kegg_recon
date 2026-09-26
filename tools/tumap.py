"""Propose translation-unit boundaries and data ownership from KE.EXE.

This is an evidence report, not a byte-reconstruction oracle. It combines the
manifest's function extents, inventory regions, and LE relocation targets. In
particular, an LE fixup says which address was referenced, not which TU owns
the referenced object; ownership and TU splits remain labeled hypotheses.

    python tools/tumap.py [--output build/tumap.json]
"""
from __future__ import annotations

import argparse
import bisect
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "build" / "pylib"))
import le as lemod  # noqa: E402
try:
    from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # type: ignore
    from capstone.x86 import X86_OP_MEM  # type: ignore
except ImportError:  # pragma: no cover - environment dependent
    CS_ARCH_X86 = CS_MODE_32 = X86_OP_MEM = None
    Cs = None

IMAGE = ROOT / "assets" / "KE.EXE"
MANIFEST = ROOT / "manifest.json"
INVENTORY = ROOT / "build" / "inventory.json"


def hx(n: int) -> str:
    return f"0x{n:05X}"


def num(x) -> int:
    return int(x, 16) if isinstance(x, str) else int(x)


def in_ranges(value: int, ranges: list[tuple[int, int]]) -> bool:
    return any(a <= value < b for a, b in ranges)


def c_string(data: bytes, pos: int) -> tuple[bytes, int] | None:
    """Return a printable NUL-terminated string and exclusive end."""
    if pos >= len(data) or not (0x20 <= data[pos] <= 0x7e):
        return None
    end = data.find(b"\0", pos)
    if end < 0 or end == pos or any(c < 0x20 or c > 0x7e for c in data[pos:end]):
        return None
    return data[pos:end], end + 1


def leading_const_pool(data: bytes) -> tuple[int, list[dict]]:
    """Walk the leading aligned string pool after LE's four-byte nullarea.

    Watcom places CONST before initialized _DATA. Consecutive referenced,
    printable C strings, with at most three zero alignment bytes between
    strings, give a content-derived lower-level boundary. A non-string CONST2
    tail cannot be distinguished from _DATA from bytes alone and is reported.
    """
    pos = 4 if data[:4] == b"\x01\x01\x01\0" else 0
    strings: list[dict] = []
    last_end = pos
    while pos < len(data):
        found = c_string(data, pos)
        if found is None:
            break
        raw, end = found
        strings.append({"start": pos, "end": end, "text": raw.decode("latin-1")})
        last_end = end
        aligned = (end + 3) & ~3
        if aligned > len(data) or any(data[i] != 0 for i in range(end, aligned)):
            pos = end
            break
        pos = aligned
    return (last_end + 3) & ~3, strings


def function_records(manifest: dict) -> list[dict]:
    out = []
    for f in manifest.get("functions", []):
        if f.get("kind", "c") == "c":
            start, end = num(f["start"]), num(f["end"])
            out.append({"name": f["name"], "start": start, "end": end,
                        "kind": f.get("kind", "c"), "status": f.get("status"),
                        "profile": f.get("profile", "game-c"), "manifest": f})
    return sorted(out, key=lambda f: (f["start"], f["end"]))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path, default=ROOT / "build" / "tumap.json")
    args = ap.parse_args(argv)
    if not IMAGE.is_file() or not MANIFEST.is_file() or not INVENTORY.is_file():
        raise SystemExit("need assets/KE.EXE, manifest.json and build/inventory.json")

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    image = lemod.LE(IMAGE)
    objs = {o["n"]: o for o in image.objects}
    obj3 = image.object_bytes(objs[3])[:objs[3]["vsize"]]
    init_end = len(image.object_bytes(objs[3]))
    obj3_size = objs[3]["vsize"]
    code1 = image.object_bytes(objs[1])[:objs[1]["vsize"]]
    funcs = function_records(manifest)
    fstarts = [f["start"] for f in funcs]

    # Direct relocation bindings from game/runtime code into object 3.
    ref_rows: list[dict] = []
    refs_by_fn: dict[str, list[dict]] = defaultdict(list)
    refs_by_target: dict[int, set[str]] = defaultdict(set)
    source_refs = defaultdict(list)
    for src_obj, src_off, typ, target in image.resolved_fixups():
        if src_obj != 1 or target.get("kind") != "internal" or target.get("obj") != 3:
            continue
        dest = target.get("off")
        if not isinstance(dest, int):
            continue
        idx = bisect.bisect_right(fstarts, src_off) - 1
        fn = funcs[idx] if idx >= 0 and funcs[idx]["start"] <= src_off < funcs[idx]["end"] else None
        region = None
        for r in inventory.get("regions", []):
            if r.get("object") == 1 and num(r["start"]) <= src_off < num(r["end"]):
                region = r
                break
        source_name = fn["name"] if fn else (region.get("name") if region else None)
        if dest < 4:
            cls = "CONST"
        elif dest < init_end:
            cls = "CONST" if dest < 0 else "_DATA"  # refined below from string-pool evidence
        else:
            cls = "_BSS"
        row = {"source": src_off, "source_name": source_name, "source_function": fn,
               "type": typ, "target": dest, "class": cls,
               "source_region_kind": region.get("kind") if region else "unassigned"}
        ref_rows.append(row)
        source_refs[src_off].append(row)
        if fn:
            refs_by_fn[fn["name"]].append(row)
        if source_name:
            refs_by_target[dest].add(source_name)

    const_end, leading_strings = leading_const_pool(obj3[:init_end])
    pool_strings = list(leading_strings)
    # A leading pool is not guaranteed to begin at byte 4: the image may have
    # a small linker/runtime prefix or a non-string CONST2 block. Inventory
    # all referenced C strings so the class boundary can be corrected from
    # actual literal references below and kept auditable in the JSON.
    referenced_string_candidates = []
    for target in sorted({r["target"] for r in ref_rows if 4 <= r["target"] < init_end}):
        found = c_string(obj3, target)
        if found:
            raw, end = found
            candidate_users = [r for r in ref_rows if r["target"] == target]
            referenced_string_candidates.append({"start": target, "end": end,
                "text": raw.decode("latin-1"), "users": len(refs_by_target.get(target, ())),
                "game_users": sum(r["source"] < 0x13B9C for r in candidate_users),
                "runtime_users": sum(r["source"] >= 0x13B9C for r in candidate_users),
                "sample_sources": sorted({r.get("source_name") for r in candidate_users if r.get("source_name")})[:8]})
    plausible_strings = [s for s in referenced_string_candidates
                         if len(s["text"]) >= 3 and any(ch.isalnum() for ch in s["text"])]
    runtime_strings = [s for s in plausible_strings if s["runtime_users"]]
    if runtime_strings:
        # Runtime members are after the game objects in link order. Their
        # referenced CONST strings therefore cap the combined CONST pool;
        # later game-targeted strings are candidate initialized _DATA.
        const_end = (max(s["end"] for s in runtime_strings) + 3) & ~3
        pool_strings = [s for s in plausible_strings if s["start"] < const_end]
    elif plausible_strings:
        pool_strings = list(plausible_strings)
        const_end = max(const_end, (max(s["end"] for s in pool_strings) + 3) & ~3)
    # Only regard a leading string as confirmed CONST when code actually points
    # at that exact address. Unreferenced pool tails remain part of the inferred
    # class boundary but lower the boundary confidence.
    pool_starts = {s["start"] for s in pool_strings}
    refd_pool = {r["target"] for r in ref_rows if r["target"] in pool_starts}
    pool_string_end = max((s["end"] for s in pool_strings), default=4)
    class_ranges = {
        # __nullarea occupies the special linker prefix at 0..4; it is kept
        # out of game TU CONST/_DATA ownership.
        "CONST": (4, min(const_end, init_end)),
        "_DATA": (min(const_end, init_end), init_end),
        "_BSS": (init_end, obj3_size),
    }
    for row in ref_rows:
        off = row["target"]
        row["class"] = next((k for k, (a, b) in class_ranges.items() if a <= off < b), "out_of_range")
        if row["target"] in pool_starts:
            row["is_string_literal"] = True
            row["binding"] = "referenced-string"
        elif row["class"] == "CONST":
            row["binding"] = "constant-or-const2"
        elif len(refs_by_target.get(off, ())) > 1:
            row["binding"] = "shared-address; owner unresolved"
        else:
            row["binding"] = "single-address-reference; owner unresolved"

    # Give every C function its direct targets, with explicit class totals.
    function_output = []
    for f in funcs:
        rs = refs_by_fn.get(f["name"], [])
        by_class = {c: sorted({r["target"] for r in rs if r["class"] == c})
                    for c in ("CONST", "_DATA", "_BSS")}
        function_output.append({"name": f["name"], "code": [f["start"], f["end"]],
            "status": f["status"], "profile": f["profile"], "references": by_class,
            "reference_sites": [{"site": r["source"], "target": r["target"],
                                 "class": r["class"], "binding": r["binding"]} for r in rs]})

    # Find code starts with the explicit hand-assembly entry shape inside the
    # nominal C area. Requiring preceding zero fill and dword alignment keeps
    # ordinary C instructions from becoming TU boundaries.
    asm_sigs = {
        "pushad-lea-ebp": bytes.fromhex("608d6c241c"),
        "push-ebp-lea-ebp": bytes.fromhex("558d2c24"),
    }
    asm_markers = []
    for tag, needle in asm_sigs.items():
        p = 0x10
        while True:
            p = code1.find(needle, p, 0x112FA)
            if p < 0:
                break
            if p % 4 == 0 and code1[max(0, p - 3):p] and all(x == 0 for x in code1[max(0, p - 3):p]):
                asm_markers.append({"start": p, "tag": tag, "evidence": "known-asm-prologue-after-zero-fill"})
            p += 1
    for off in (0x982C, 0x9F64, 0xA284):
        if off < min(0x112FA, len(code1)):
            fill = max((n for n in (1, 2, 3) if code1[off - n:off] == bytes(n)), default=0)
            local = {"start": off, "tag": "prompted-aligned-asm-candidate",
                     "fill_bytes": fill, "entry_bytes": code1[off:off + 8].hex(),
                     "evidence": ("task-listed address; dword aligned with preceding zero fill"
                                  if off % 4 == 0 and fill and code1[off] != 0
                                  else "task-listed address; local alignment/prologue bytes remain ambiguous"),
                     "confidence": "STRONG" if off % 4 == 0 and fill and code1[off] != 0
                                  else "HYPOTHESIS"}
            old = next((m for m in asm_markers if m["start"] == off), None)
            if old:
                old.update(local)
            else:
                asm_markers.append(local)
    asm_markers.sort(key=lambda x: x["start"])
    asm_markers = list({m["start"]: m for m in asm_markers}.values())

    # Infer constant-pool TU cuts only where two adjacent code cohorts refer to
    # separate strings and the preceding string is followed by dword-alignment
    # zeros. These are candidate cuts; a string can be shared within a TU.
    func_idx = {f["name"]: i for i, f in enumerate(funcs)}
    literals_by_fn = {}
    for f in funcs:
        literals_by_fn[f["name"]] = sorted({r["target"] for r in refs_by_fn.get(f["name"], [])
                                            if r["target"] in pool_starts})
    cuts = []
    for left_i in range(len(funcs)):
        left = funcs[left_i]
        left_addrs = literals_by_fn[left["name"]]
        if not left_addrs:
            continue
        # Search forward to the first later function with literal bindings.
        right_i = left_i + 1
        while right_i < len(funcs) and not literals_by_fn[funcs[right_i]["name"]]:
            right_i += 1
        if right_i >= len(funcs):
            continue
        right = funcs[right_i]
        right_addrs = literals_by_fn[right["name"]]
        a, b = max(left_addrs), min(right_addrs)
        astring = next((s for s in pool_strings if s["start"] == a), None)
        if astring is None or b <= a:
            continue
        gap_start, gap_end = astring["end"], b
        if (0 <= gap_end - gap_start <= 3 and b % 4 == 0
                and all(c == 0 for c in obj3[gap_start:gap_end])):
            cuts.append({"after": left_i, "before": right_i, "code_start": right["start"],
                         "class": "CONST", "left_literal": a, "right_literal": b,
                         "evidence": ["referenced-leading-strings", "zero-fill-to-dword-alignment"],
                         "confidence": "STRONG" if right_i == left_i + 1 else "HYPOTHESIS"})
    # Keep only non-overlapping textual transitions and merge duplicates.
    cuts_by_start = {}
    for cut in cuts:
        cuts_by_start.setdefault(cut["code_start"], cut)
    cuts = sorted(cuts_by_start.values(), key=lambda x: x["code_start"])

    # Classify the -ot profile using the manifest/compiler notes and verify its
    # alignment/padding shape in code. The profile itself is an input claim.
    ot_funcs = [f for f in funcs if "ot" in f["profile"].lower()]
    profile_groups = []
    for f in ot_funcs:
        if not profile_groups or f["start"] - profile_groups[-1]["end"] > 4:
            profile_groups.append({"start": f["start"], "end": f["end"], "functions": [f["name"]]})
        else:
            profile_groups[-1]["end"] = f["end"]
            profile_groups[-1]["functions"].append(f["name"])

    # Split code order at evidence-bearing boundaries. These are proposed TUs,
    # with data ownership left empty where only external/shared references exist.
    c_start, c_end, asm_start, late_c = 0x10, 0x112FA, 0x112FA, 0x13A95
    # A known asm prologue embedded among game C functions marks an asm
    # contribution. Its end is the next asm marker or next C prologue, whichever
    # comes first; this extent is a candidate until a full CFG is recovered.
    internal_asm_blocks = []
    for mi, marker in enumerate(asm_markers):
        later_c = [f["start"] for f in funcs if f["start"] > marker["start"]]
        next_marker = asm_markers[mi + 1]["start"] if mi + 1 < len(asm_markers) else c_end
        end = min(next_marker, min(later_c, default=c_end))
        if marker["start"] < end:
            internal_asm_blocks.append({"start": marker["start"], "end": end,
                                        "tag": marker["tag"],
                                        "confidence": marker.get("confidence", "STRONG"),
                                        "marker_evidence": marker.get("evidence", "asm entry candidate")})
    hard_cuts = {c_start, c_end, late_c, 0x13B9C}
    for block in internal_asm_blocks:
        hard_cuts.update((block["start"], block["end"]))
    cut_evidence = defaultdict(list)
    cut_conf = {}
    for cut in cuts:
        if c_start < cut["code_start"] < c_end:
            hard_cuts.add(cut["code_start"])
            cut_evidence[cut["code_start"]].append("CONST string-pool alignment transition")
            cut_conf[cut["code_start"]] = cut["confidence"]
    for g in profile_groups:
        if c_start < g["start"] < c_end:
            hard_cuts.add(g["start"])
            cut_evidence[g["start"]].append("manifest game-c-ot profile start")
            cut_conf[g["start"]] = "STRONG"
        if c_start < g["end"] < c_end:
            hard_cuts.add(g["end"])
            cut_evidence[g["end"]].append("manifest game-c-ot profile end")
            cut_conf[g["end"]] = "STRONG"
    for block in internal_asm_blocks:
        hard_cuts = {x for x in hard_cuts
                     if not block["start"] < x < block["end"] or x in (block["start"], block["end"])}
    hard_cuts = sorted(hard_cuts)

    def funcs_in(a, b):
        return [f for f in funcs if a <= f["start"] < b]

    tus = []
    def add_tu(start, end, kind, evidence, confidence, fs=None, ident=None, object_no=1):
        fs = fs if fs is not None else funcs_in(start, end)
        all_refs = [r for r in ref_rows if object_no == 1 and start <= r["source"] < end]
        # Referenced extents are conservative observed bounds. They are not
        # claimed to be the complete contribution when symbols are unreferenced.
        data = {}
        for cls in ("CONST", "_DATA", "_BSS"):
            vals = [r["target"] for r in all_refs if r["class"] == cls]
            if vals:
                data[cls] = [min(vals), max(vals) + 1]
            else:
                data[cls] = None
        names = [f["name"] for f in fs]
        if not names:
            names = [r.get("name", f"region_{num(r['start']):05X}")
                     for r in inventory.get("regions", []) if r.get("object") == object_no
                     and start <= num(r["start"]) < end
                     and r.get("kind") in ("asm", "crt")]
        tus.append({"id": ident or f"tu{len(tus)+1:03d}", "object": object_no,
                    "code": [start, end], "kind": kind,
                    "data": data, "functions": names,
                    "evidence": list(evidence), "confidence": confidence,
                    "data_ranges_are": "referenced extents only; inferred ownership is unresolved"})

    # Link order known from code order; preserve identified game C and asm
    # regions. Boundaries with only string alignment evidence are hypotheses.
    for a, b in zip(hard_cuts, hard_cuts[1:]):
        if a >= c_start and b <= c_end:
            asm_block = next((x for x in internal_asm_blocks
                              if x["start"] == a and x["end"] == b), None)
            if asm_block:
                add_tu(a, b, "asm", [asm_block["tag"], asm_block["marker_evidence"]],
                       asm_block["confidence"], [])
                if not tus[-1]["functions"]:
                    tus[-1]["functions"] = [f"asm_{a:05X}"]
                continue
            fs = funcs_in(a, b)
            profiles = {f["profile"] for f in fs}
            kind = "c-ot" if any("ot" in p.lower() for p in profiles) else "c"
            ev = ["manifest C function extents", "code/link order"]
            ev.extend(cut_evidence.get(a, []))
            confidence = cut_conf.get(a, "HYPOTHESIS" if a != c_start else "STRONG")
            add_tu(a, b, kind, ev, confidence, fs)
    # Assembly modules/routines: inventory has entry candidates, but only a
    # dword-aligned entry preceded by zero fill is promoted to an object split.
    # Other prologues remain in the enclosing asm contribution.
    inv_asm_starts = sorted({num(r["start"]) for r in inventory.get("regions", [])
        if r.get("object") == 1 and r.get("kind") == "asm"
        and asm_start <= num(r["start"]) < late_c})
    asm_object_starts = [off for off in inv_asm_starts if off % 4 == 0
                         and any(code1[off - n:off] == bytes(n) for n in (1, 2, 3))]
    asm_cuts = sorted(set([asm_start, late_c] + asm_object_starts))
    for a, b in zip(asm_cuts, asm_cuts[1:]):
        names = [r.get("name", f"asm_{num(r['start']):05X}")
                 for r in inventory.get("regions", []) if r.get("object") == 1
                 and r.get("kind") == "asm" and a <= num(r["start"]) < b]
        evidence = ["hand-assembly region"]
        confidence = "STRONG" if a == asm_start else "HYPOTHESIS"
        if a in asm_object_starts:
            evidence.append("dword-aligned inventory asm entry after zero fill")
            confidence = "STRONG"
        add_tu(a, b, "asm", evidence, confidence, [])
        if names:
            tus[-1]["functions"] = names
    # The late C TU and CRT members are anchored by the evidence register.
    late_fs = funcs_in(late_c, 0x13B9C)
    add_tu(late_c, 0x13B9C, "c", ["late C-shaped compiler prologues", "linked after hand-asm modules"],
           "STRONG", late_fs)
    obj2_regions = [r for r in inventory.get("regions", [])
                    if r.get("object") == 2 and r.get("kind") == "asm"]
    for r in obj2_regions:
        add_tu(num(r["start"]), num(r["end"]), "asm",
               ["obj2 16-bit interrupt-template region", r.get("name", "inventory template")],
               "STRONG", [], object_no=2)
    add_tu(0, 0x10, "crt", ["exact BEGTEXT runtime thunk"], "PROVEN", [])
    crt_regions = [r for r in inventory.get("regions", []) if r.get("object") == 1
                   and r.get("kind") == "crt" and num(r["start"]) >= 0x13B9C]
    # Runtime members are grouped by contiguous extents; gaps stay explicit.
    crt_regions.sort(key=lambda r: num(r["start"]))
    for r in crt_regions:
        r_evidence = r.get("evidence", [])
        if "libscan-masked-exact-hit" in r_evidence and not r.get("ambiguous"):
            crt_confidence = "PROVEN"
        elif r.get("confidence") in ("PROVEN-ENTRYPOINT", "STRONG-RUNTIME-ATTRIBUTION"):
            crt_confidence = "STRONG"
        else:
            crt_confidence = "HYPOTHESIS"
        add_tu(num(r["start"]), num(r["end"]), "crt",
               [*r_evidence, r.get("name", r.get("kind", "crt"))],
               crt_confidence, [])
    def link_phase(t):
        start = t["code"][0]
        if t["object"] == 1 and start < 0x10:
            return 0
        if t["object"] == 1 and start < 0x13B9C:
            return 1
        if t["object"] == 2:
            return 2
        return 3
    tus.sort(key=lambda t: (link_phase(t), t["code"][0], t["code"][1]))
    for i, tu in enumerate(tus, 1):
        tu["id"] = f"tu{i:03d}"

    # Infer owners from address-use cohorts. A target used only by one proposed
    # TU is an ownership anchor; shared targets are attached only when their
    # address falls inside a monotone block bracketed by private anchors.
    source_tu = {}
    refs_by_tu: dict[str, list[dict]] = defaultdict(list)
    target_tus: dict[int, set[str]] = defaultdict(set)
    for tu in tus:
        if tu["object"] != 1:
            continue
        start, end = tu["code"]
        for r in ref_rows:
            if start <= r["source"] < end:
                refs_by_tu[tu["id"]].append(r)
                target_tus[r["target"]].add(tu["id"])
        for name in tu["functions"]:
            source_tu[name] = tu["id"]
    inferred_blocks: dict[str, dict[str, list[int]]] = defaultdict(dict)
    anchor_ranges: dict[str, dict[str, list[int]]] = defaultdict(dict)
    ownership_conflicts = []
    for cls, (class_start, class_end) in class_ranges.items():
        active = []
        for tu in tus:
            if tu["object"] != 1:
                continue
            anchors = sorted({r["target"] for r in refs_by_tu[tu["id"]]
                              if r["class"] == cls and target_tus[r["target"]] == {tu["id"]}})
            if anchors:
                active.append((tu, anchors))
        for (prev, pvals), (cur, cvals) in zip(active, active[1:]):
            if max(pvals) >= min(cvals):
                ownership_conflicts.append({"type": "nonmonotone-private-data-anchors",
                    "class": cls, "earlier_tu": prev["id"], "earlier_max": hx(max(pvals)),
                    "later_tu": cur["id"], "later_min": hx(min(cvals)),
                    "evidence": "single-TU reference targets reverse address order",
                    "confidence": "STRONG"})
        for i, (tu, anchors) in enumerate(active):
            if i == 0:
                left = class_start
            else:
                prev_max = max(active[i - 1][1])
                left = (prev_max + min(anchors)) // 2
            if i + 1 == len(active):
                right = class_end
            else:
                next_min = min(active[i + 1][1])
                right = (max(anchors) + next_min) // 2
            if right < left:
                left, right = min(anchors), max(anchors) + 1
            block = [left, right]
            inferred_blocks[tu["id"]][cls] = block
            anchor_ranges[tu["id"]][cls] = [min(anchors), max(anchors)]
            tu["data"][cls] = block
            tu.setdefault("data_evidence", {})[cls] = {
                "anchor_count": len(anchors), "anchor_min": hx(min(anchors)),
                "anchor_max": hx(max(anchors)),
                "method": "ordered private-reference anchors; midpoint between adjacent TU cohorts",
                "confidence": "STRONG" if len(anchors) >= 3 and right >= left else "HYPOTHESIS"}
    target_owner = {}
    target_owner_confidence = {}
    for target, users in target_tus.items():
        cls = next((c for c, (a, b) in class_ranges.items() if a <= target < b), None)
        if not cls:
            continue
        if len(users) == 1:
            target_owner[target] = next(iter(users))
            target_owner_confidence[target] = "STRONG"
            continue
        candidates = [tid for tid, ranges in inferred_blocks.items()
                      if cls in ranges and ranges[cls][0] <= target < ranges[cls][1]]
        if len(candidates) == 1:
            target_owner[target] = candidates[0]
            target_owner_confidence[target] = "HYPOTHESIS"
        elif candidates:
            referencing_candidates = [tid for tid in candidates if tid in users]
            pool = referencing_candidates or candidates
            distances = {tid: abs(target - sum(anchor_ranges[tid][cls]) // 2) for tid in pool}
            best_distance = min(distances.values())
            best = [tid for tid, distance in distances.items() if distance == best_distance]
            if len(best) == 1:
                target_owner[target] = best[0]
                target_owner_confidence[target] = "HYPOTHESIS"
    external_targets = defaultdict(lambda: {"class": None, "owner_tu": None, "source_tus": set(),
                                             "sources": []})
    for r in ref_rows:
        owner = target_owner.get(r["target"])
        source = next((tu["id"] for tu in tus if tu["object"] == 1
                       and tu["code"][0] <= r["source"] < tu["code"][1]), None)
        r["source_tu"] = source
        r["owner_tu"] = owner
        r["owner_confidence"] = target_owner_confidence.get(r["target"])
        if owner is not None and owner == source:
            r["binding"] = "local-to-defining-TU"
        elif owner is not None:
            r["binding"] = "shared-extern; defining-TU inferred"
            e = external_targets[r["target"]]
            e["class"], e["owner_tu"] = r["class"], owner
            if source:
                e["source_tus"].add(source)
            e["sources"].append({"site": hx(r["source"]), "tu": source})
        elif len(target_tus.get(r["target"], ())) > 1:
            r["binding"] = "shared-extern; defining-TU unresolved"
        elif r["class"] == "CONST" and r["target"] in pool_starts:
            r["binding"] = "string-literal owner unresolved"
    for target, item in sorted(external_targets.items()):
        item["target"] = hx(target)
        item["source_tus"] = sorted(item["source_tus"])
        ownership_conflicts.append({"type": "cross-TU-extern-reference", **item,
            "evidence": "referenced address falls in another TU's inferred data block",
            "confidence": target_owner_confidence.get(target, "HYPOTHESIS")})

    # Per-TU cross references, ordering conflicts, and exact validation of
    # matching functions against the inferred class ranges.
    owner_tu = {}
    for tu in tus:
        for name in tu["functions"]:
            owner_tu[name] = tu["id"]
    conflicts = ownership_conflicts
    unassigned = []
    for obj_no, size in ((1, objs[1]["vsize"]), (2, objs[2]["vsize"])):
        intervals = sorted((t["code"][0], t["code"][1]) for t in tus if t["object"] == obj_no)
        cursor = 0
        for start, end in intervals:
            if start > cursor:
                unassigned.append({"object": obj_no, "code": [cursor, start],
                                   "reason": "no TU boundary inferred for this code gap"})
            elif start < cursor:
                conflicts.append({"type": "overlapping-code-TU-ranges", "object": obj_no,
                                  "overlap": [start, min(cursor, end)], "confidence": "STRONG"})
            cursor = max(cursor, end)
        if cursor < size:
            unassigned.append({"object": obj_no, "code": [cursor, size],
                               "reason": "no TU boundary inferred for trailing code region"})
    for cls, (class_start, class_end) in class_ranges.items():
        intervals = sorted((t["data"][cls][0], t["data"][cls][1]) for t in tus
                           if t["object"] == 1 and t["data"].get(cls)
                           and t["data"][cls][1] > t["data"][cls][0])
        cursor = class_start
        for start, end in intervals:
            if start > cursor:
                unassigned.append({"object": 3, "segment": cls, "data": [cursor, start],
                                   "reason": "no private-reference owner block inferred"})
            cursor = max(cursor, end)
        if cursor < class_end:
            unassigned.append({"object": 3, "segment": cls, "data": [cursor, class_end],
                               "reason": "trailing data has no private-reference owner block"})
    for f in funcs:
        tid = owner_tu.get(f["name"])
        if tid is None:
            unassigned.append({"code": [f["start"], f["end"]], "reason": "function not in proposed TU"})
            continue
    for target, users in sorted(target_tus.items()):
        if len(users) > 1 and target not in target_owner:
            cls = next((c for c, (a, b) in class_ranges.items() if a <= target < b), "unknown")
            conflicts.append({"type": "shared-extern-owner-unresolved", "class": cls,
                "target": hx(target), "source_tus": sorted(users),
                "evidence": "same obj3 address referenced by multiple proposed TUs; no unique owner block contains it",
                "confidence": "HYPOTHESIS"})
    matching = [f for f in funcs if f["status"] == "matching"]
    validation = {"matching_functions": len(matching), "string_bindings_in_class": 0,
                  "string_bindings_checked": 0, "unbound_string_references": [],
                  "bindings_checked": 0, "bindings_in_defining_TU_block": 0,
                  "cross_TU_extern_bindings": 0, "unresolved_data_references": []}
    for f in matching:
        for r in refs_by_fn.get(f["name"], []):
            if r["class"] == "CONST" and r.get("is_string_literal"):
                validation["string_bindings_checked"] += 1
            if r["class"] in ("CONST", "_DATA", "_BSS"):
                validation["bindings_checked"] += 1
                defining_tu = target_owner.get(r["target"])
                extent = inferred_blocks.get(defining_tu, {}).get(r["class"])
                in_owner = bool(extent and extent[0] <= r["target"] < extent[1])
                if in_owner:
                    validation["bindings_in_defining_TU_block"] += 1
                    if defining_tu != source_tu.get(f["name"]):
                        validation["cross_TU_extern_bindings"] += 1
                else:
                    validation["unresolved_data_references"].append({"function": f["name"],
                        "target": hx(r["target"]), "class": r["class"],
                        "reason": "no inferred defining-TU block contains the LE target"})
                if r["class"] == "CONST" and r.get("is_string_literal"):
                    if in_owner and defining_tu == source_tu.get(f["name"]):
                        validation["string_bindings_in_class"] += 1
                    else:
                        validation["unbound_string_references"].append({"function": f["name"],
                            "target": hx(r["target"]), "tu": source_tu.get(f["name"]),
                            "defining_tu": defining_tu})
            elif r["class"] in ("_DATA", "_BSS"):
                validation["bindings_checked"] += 1
                validation["unresolved_data_references"].append({"function": f["name"],
                    "target": hx(r["target"]), "class": r["class"],
                    "reason": "class boundary unresolved"})
    for fo in function_output:
        name = fo["name"]
        fo["reference_sites"] = [{"site": hx(r["source"]), "target": hx(r["target"]),
            "class": r["class"], "binding": r["binding"],
            "source_tu": r.get("source_tu"), "defining_tu": r.get("owner_tu"),
            "owner_confidence": r.get("owner_confidence")}
            for r in refs_by_fn.get(name, [])]

    # Runtime cutoffs inside data classes follow the observed rule that CRT
    # contributions come after game objects. Low shared runtime anchors such as
    # __nullarea are excluded from the suffix estimate.
    runtime_start = 0x13B9C
    runtime_targets = [r for r in ref_rows if r["source"] >= runtime_start and r["target"] >= 4]
    runtime_first = {}
    for cls in ("CONST", "_DATA", "_BSS"):
        vals = [r["target"] for r in runtime_targets if r["class"] == cls]
        if vals:
            runtime_first[cls] = min(vals)

    # Highest directly referenced BSS byte gives only a lower bound on BSS;
    # unreferenced object tails and WLINK's STACK size cannot be identified
    # from LE fixups alone.
    bss_targets = [r["target"] for r in ref_rows if r["class"] == "_BSS"]
    highest_bss_ref = max(bss_targets, default=None)
    highest_bss_sites = [{"site": hx(r["source"]), "function": r.get("source_name"),
                         "target": hx(r["target"]),
                         "context": code1[max(0, r["source"] - 5):r["source"] + 8].hex(" ")}
                        for r in ref_rows if r["target"] == highest_bss_ref]
    if Cs is not None:
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        md.detail = True
        for site in highest_bss_sites:
            site_off = int(site["site"], 16)
            region = next((r for r in inventory.get("regions", []) if r.get("object") == 1
                           and num(r["start"]) <= site_off < num(r["end"])), None)
            if not region:
                continue
            a, b = num(region["start"]), num(region["end"])
            insn = next((ins for ins in md.disasm(code1[a:b], a)
                         if ins.address <= site_off < ins.address + ins.size), None)
            if insn:
                site["instruction"] = {"address": hx(insn.address), "bytes": insn.bytes.hex(),
                    "mnemonic": insn.mnemonic, "operands": insn.op_str,
                    "memory_operands": [{"width": op.size, "displacement": op.mem.disp,
                                         "base": op.mem.base, "index": op.mem.index}
                        for op in insn.operands if op.type == X86_OP_MEM]}
    candidate_stack_size = 0x1000
    candidate_stack_start = obj3_size - candidate_stack_size
    accessed_ends = []
    if highest_bss_ref is not None:
        for site in highest_bss_sites:
            ins = site.get("instruction", {})
            widths = [m["width"] for m in ins.get("memory_operands", [])
                      if (m["displacement"] & 0xFFFFFFFF) == highest_bss_ref
                      and not m["base"] and not m["index"]]
            if widths:
                accessed_ends.extend(highest_bss_ref + width for width in widths)
    highest_access_end = max(accessed_ends, default=None)
    decoded_top_access = highest_access_end == candidate_stack_start
    touches_candidate_top = (decoded_top_access or (highest_bss_ref is not None
                             and highest_bss_ref + 8 == candidate_stack_start))
    stack = {"bss_start": init_end, "obj3_end": obj3_size,
             "highest_bss_fixup_target": highest_bss_ref,
             "highest_bss_fixup_sites": highest_bss_sites,
             "highest_referenced_extent_end": highest_access_end,
             "bss_end": candidate_stack_start if touches_candidate_top else None,
             "stack_start": candidate_stack_start if touches_candidate_top else None,
             "option_stack": candidate_stack_size if touches_candidate_top else None,
             "status": (("STRONG_CANDIDATE: decoded memory access ends at the 4 KiB stack suffix"
                         if decoded_top_access else
                         "HYPOTHESIS: highest BSS target plus 8 bytes reaches a 4 KiB stack suffix; "
                         "operand width and unreferenced BSS tail still need proof")
                        if touches_candidate_top else
                        "UNRESOLVED: LE object size and fixup targets do not distinguish BSS from STACK")}

    # Candidate alignment boundaries, including code-fill facts requested in
    # the brief; distinguish observations from inferred source-file breaks.
    alignment_markers = []
    known_starts = {f["start"] for f in funcs}
    known_starts.update(num(r["start"]) for r in inventory.get("regions", [])
                        if r.get("object") == 1)
    for off in sorted(x for x in known_starts if 4 <= x < min(0x13B9C, len(code1))):
        if off % 4 == 0 and code1[off:off + 1] != b"\0":
            if any(code1[max(0, off - n):off] == bytes(n) for n in (1, 2, 3)):
                alignment_markers.append(off)

    data_boundary_evidence = {
        "obj3_init_end": {"offset": init_end, "hex": hx(init_end), "level": "PROVEN",
                           "reason": "object 3 file-backed page bytes from LE page map"},
        "const_to_data": {"offset": const_end, "hex": hx(const_end),
            "level": "STRONG" if len(refd_pool) >= 4 else "HYPOTHESIS",
            "reason": ("highest plausible runtime-referenced C string, aligned up to dword; "
                       "runtime link order constrains the combined CONST tail" if runtime_strings else
                       "highest plausible referenced C string, aligned up to dword; prefix __nullarea is separate"),
            "strings_found": len(pool_strings), "strings_referenced": len(refd_pool),
            "last_string_end": pool_string_end,
            "runtime_string_candidates": len(runtime_strings),
            "runtime_string_max": hx(max((s["start"] for s in runtime_strings), default=0)),
            "post_boundary_string_candidates": [{"start": hx(s["start"]), "end": hx(s["end"]),
                "text": s["text"], "game_users": s["game_users"], "runtime_users": s["runtime_users"]}
                for s in plausible_strings if s["start"] >= const_end],
            "limitation": "non-string CONST2 tail and isolated strings in initialized globals cannot be separated from bytes alone"},
        "special_obj3_prefix": {"range": [0, 4], "bytes": obj3[:4].hex(),
            "evidence": "__nullarea signature 01 01 01 00", "class": "linker/runtime prefix"},
        "const_data_bss_ranges": {k: [a, b] for k, (a, b) in class_ranges.items()},
        "runtime_first_references": {k: hx(v) for k, v in runtime_first.items()},
    }

    result = {
        "format": "kegg-tumap-1", "image": "assets/KE.EXE",
        "basis": {"manifest_sha256": manifest.get("original", {}).get("KE.EXE", {}).get("sha256"),
                  "inventory_version": inventory.get("inventory_version"),
                  "fixups": "tools/le.py resolved LE fixups from obj1 to obj3",
                  "confidence_legend": {"PROVEN": "direct from binary bytes or exact inventory match",
                      "STRONG": "multiple independent structural signals",
                      "HYPOTHESIS": "plausible boundary or ownership, not proven"}},
        "data_class_evidence": data_boundary_evidence,
        "data_diagnostics": {"obj3_prefix_hex": obj3[:160].hex(" "),
            "referenced_string_candidates": referenced_string_candidates,
            "most_referenced_targets": [{"target": hx(a), "ref_count": n,
                "sample_users": sorted(refs_by_target[a])[:8]}
                for a, n in Counter(r["target"] for r in ref_rows).most_common(80)],
            "target_page_counts": {hx((r["target"] // 0x1000) * 0x1000): sum(
                1 for x in ref_rows if x["target"] // 0x1000 == r["target"] // 0x1000)
                for r in ref_rows}},
        "tus": tus,
        "functions": function_output,
        "string_boundaries": cuts,
        "code_alignment_candidates": [hx(x) for x in alignment_markers],
        "asm_markers": [{**m, "start": hx(m["start"])} for m in asm_markers],
        "runtime_members": [{"code": [num(r["start"]), num(r["end"])], "name": r.get("name"),
                             "member_match": "libscan-masked-exact-hit" in r.get("evidence", []),
                             "kind": r.get("kind"), "evidence": r.get("evidence"),
                             "confidence": r.get("confidence")}
                            for r in crt_regions],
        "validation": validation,
        "conflicts": conflicts,
        "unassigned_ranges": unassigned,
        "stack": stack,
        "summary": {"tu_count": len(tus), "tu_kind_counts": dict(Counter(t["kind"] for t in tus)),
                    "link_order": [t["id"] for t in tus],
                    "function_count": len(funcs), "matching_function_count": len(matching),
                    "obj3_init_end": hx(init_end), "const_data_boundary": hx(const_end),
                    "bss_start": hx(init_end),
                    "bss_end_and_stack_start": hx(candidate_stack_start) if touches_candidate_top else "unresolved",
                    "option_stack_candidate": hx(candidate_stack_size) if touches_candidate_top else None,
                    "weakest_boundaries": [t["id"] for t in tus if t["confidence"] == "HYPOTHESIS"]},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"TU map: {len(tus)} proposed contributions; {len(funcs)} C functions; "
          f"{len(matching)} matching functions checked")
    print(f"obj3: CONST 0x00004..{hx(const_end)}, _DATA {hx(const_end)}..{hx(init_end)}, "
          f"_BSS {hx(init_end)}..{hx(obj3_size)}")
    print(f"string pool: {len(pool_strings)} strings, {len(refd_pool)} referenced; "
          f"matching string refs in TU extent {validation['string_bindings_in_class']}/"
          f"{validation['string_bindings_checked']}")
    if referenced_string_candidates:
        first = pool_strings[:4]
        last = pool_strings[-4:]
        print("referenced strings first=" + ", ".join(
            f"{hx(s['start'])}:{s['text'][:24]!r}" for s in first))
        print("referenced strings last=" + ", ".join(
            f"{hx(s['start'])}:{s['text'][:24]!r}" for s in last))
    print("proposed link order=" + ", ".join(
        f"{t['id']}:{t['kind']}[{hx(t['code'][0])},{hx(t['code'][1])})/{t['confidence']}"
        for t in tus[:32]))
    if len(tus) > 32:
        print(f"... {len(tus)-32} further contributions in JSON")
    print(f"alignment candidates={len(alignment_markers)}, asm object cuts={len(asm_object_starts)}")
    print(f"constant-pool cuts={len(cuts)}, internal asm spans={len(internal_asm_blocks)}")
    print(f"matching bindings in defining-TU blocks="
          f"{validation['bindings_in_defining_TU_block']}/{validation['bindings_checked']}; "
          f"cross-TU externs={validation['cross_TU_extern_bindings']}; "
          f"unresolved={len(validation['unresolved_data_references'])}")
    print("runtime first data references=" + ", ".join(
        f"{k}:{hx(v)}" for k, v in runtime_first.items()))
    print(f"weak TU boundaries={sum(1 for t in tus if t['confidence']=='HYPOTHESIS')}; "
          f"nonmonotone/shared conflicts={len(conflicts)}; unassigned ranges={len(unassigned)}")
    print(f"BSS/STACK boundary: {hx(candidate_stack_start) if touches_candidate_top else 'unresolved'}; "
          f"highest BSS fixup target="
          f"{hx(highest_bss_ref) if highest_bss_ref is not None else 'none'}")
    print(f"conflicts={len(conflicts)}, unassigned={len(unassigned)}; wrote {args.output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
