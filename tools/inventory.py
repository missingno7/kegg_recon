"""Deterministic code-object region inventory for assets/KE.EXE.

Run from any working directory with ``python tools/inventory.py``.  Exact
Watcom library matches come from libscan/omf, object fixups from le.py, and
instruction decoding from Capstone when available.  The reduced mode used
when Capstone is absent keeps its call counts explicitly approximate.
"""
from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "build" / "pylib"))
import le as lemod  # noqa: E402
import libscan  # noqa: E402
import omf  # noqa: E402  (used by libscan's library-member reader)

try:
    from capstone import (  # type: ignore
        CS_ARCH_X86,
        CS_MODE_16,
        CS_MODE_32,
        CS_GRP_JUMP,
        CS_GRP_RET,
        Cs,
    )
    from capstone.x86 import X86_OP_IMM, X86_OP_MEM  # type: ignore
    CAPSTONE_ERROR = None
except ImportError as exc:  # pragma: no cover - environment-dependent path
    Cs = None
    X86_OP_IMM = X86_OP_MEM = -1
    CS_ARCH_X86 = CS_MODE_16 = CS_MODE_32 = CS_GRP_JUMP = CS_GRP_RET = 0
    CAPSTONE_ERROR = str(exc)


IMAGE = ROOT / "assets" / "KE.EXE"
LIBS = [
    Path("C:/tools/watcom-10.0/LIB386/DOS/clib3s.lib"),
    Path("C:/tools/watcom-10.0/LIB386/math387s.lib"),
    Path("C:/tools/watcom-10.0/LIB386/DOS/emu387.lib"),
    Path("C:/tools/watcom-10.0/LIB386/math3s.lib"),
]

# These boundaries are binary-derived anchors, checked against the signatures
# and exact library matches below.  They are not compiler/linker output claims.
C_START = 0x10
C_ASM = 0x112FA
ASM_CRT = 0x13B9C
OBJ2_TEMPLATE_SIG = bytes.fromhex("66608cd86650b8")
OBJ2_FAR_END = bytes.fromhex("ea4433221190")

C_PROLOGUE_PREFIXES = (
    bytes.fromhex("5356575589e581ec"),  # push ebx/esi/edi/ebp; mov ebp,esp; sub esp,imm32
    bytes.fromhex("5356575589e583ec"),  # same, short stack adjustment
    bytes.fromhex("5356575589e5"),       # retained to report compiler prologue variants
)
ASM_PROLOGUES = {
    "pushad-lea-ebp": bytes.fromhex("608d6c241c"),
    "push-ebp-lea-ebp": bytes.fromhex("558d2c24"),
}


def hx(value: int) -> str:
    return f"0x{value:05X}"


def all_hits(data: bytes, needle: bytes, start: int, end: int):
    pos = start
    while True:
        pos = data.find(needle, pos, end)
        if pos < 0:
            return
        yield pos
        pos += 1


def c_prologues(code: bytes, start: int, end: int) -> list[tuple[int, str]]:
    found: dict[int, str] = {}
    for prefix in C_PROLOGUE_PREFIXES:
        for off in all_hits(code, prefix, start, end):
            found.setdefault(off, "watcom-c-prologue")
    return sorted(found.items())


def asm_prologues(code: bytes, start: int, end: int) -> list[tuple[int, str]]:
    found = []
    for tag, pattern in ASM_PROLOGUES.items():
        found.extend((off, tag) for off in all_hits(code, pattern, start, end))
    return sorted(set(found))


def fixup_tables(fixups, obj_no: int) -> list[dict]:
    """Find contiguous obj1 off32 fixup runs that may be embedded pointer tables."""
    sites = []
    for source_obj, source_off, typ, target in fixups:
        if (source_obj == obj_no and typ == "off32" and target.get("kind") == "internal"
                and target.get("obj") == obj_no and isinstance(target.get("off"), int)):
            sites.append((source_off, target["off"]))
    sites.sort()
    out = []
    i = 0
    while i < len(sites):
        j = i + 1
        while j < len(sites) and sites[j][0] == sites[j - 1][0] + 4:
            j += 1
        if j - i >= 2:
            out.append({"start": sites[i][0], "end": sites[j - 1][0] + 4,
                        "targets": [target for _, target in sites[i:j]],
                        "evidence": ["contiguous-off32-fixups", "possible-inline-jump-table"],
                        "confidence": "STRONG"})
        i = j
    return out


def raw_switch_dispatches(code: bytes, start: int, end: int) -> list[dict]:
    """Recognize the common ``jmp [index*4 + disp32]`` switch idiom."""
    out = []
    for off in range(start, max(start, end - 6)):
        if code[off:off + 2] != b"\xff\x24":
            continue
        sib = code[off + 2]
        scale, index, base = sib >> 6, (sib >> 3) & 7, sib & 7
        if scale != 2 or index == 4 or base != 5:
            continue
        table = int.from_bytes(code[off + 3:off + 7], "little")
        if start <= table < end:
            out.append({"site": off, "table": table, "scale": 4,
                        "evidence": ["x86-indirect-jump", "indexed-absolute-table"],
                        "confidence": "STRONG"})
    return out


def capstone_function(md, code: bytes, start: int, end: int) -> dict:
    """Walk direct control flow within an already identified region."""
    todo = [start]
    visited: set[int] = set()
    calls = []
    returns = []
    indirect_jumps = []
    max_end = start
    while todo:
        pc = todo.pop()
        while start <= pc < end and pc not in visited:
            ins = next(md.disasm(code[pc:end], pc, count=1), None)
            if ins is None or ins.size <= 0:
                break
            visited.add(pc)
            next_pc = pc + ins.size
            max_end = max(max_end, next_pc)
            is_call = ins.mnemonic.startswith("call")
            if is_call and ins.operands and ins.operands[0].type == X86_OP_IMM:
                calls.append({"site": pc, "target": ins.operands[0].imm & 0xFFFFFFFF})
            if ins.group(CS_GRP_RET):
                returns.append(pc)
                break
            if ins.mnemonic == "jmp":
                if ins.operands and ins.operands[0].type == X86_OP_IMM:
                    dest = ins.operands[0].imm & 0xFFFFFFFF
                    if start <= dest < end:
                        todo.append(dest)
                else:
                    mem = ins.operands[0].mem if ins.operands and ins.operands[0].type == X86_OP_MEM else None
                    indirect_jumps.append({
                        "site": pc, "mnemonic": ins.mnemonic,
                        "operand": ins.op_str,
                        "indexed": bool(mem and mem.index),
                        "scale": mem.scale if mem else None,
                        "displacement": mem.disp if mem else None,
                    })
                break
            if ins.group(CS_GRP_JUMP):
                if ins.operands and ins.operands[0].type == X86_OP_IMM:
                    dest = ins.operands[0].imm & 0xFFFFFFFF
                    if start <= dest < end:
                        todo.append(dest)
            pc = next_pc
    return {"calls": calls, "returns": returns, "indirect_jumps": indirect_jumps,
            "reachable_instructions": len(visited), "reachable_end": max_end}


def add_region(regions: list[dict], obj: int, start: int, end: int, kind: str,
               evidence: list[str], *, name: str | None = None, ambiguous=False,
               **extra):
    if start >= end:
        return
    region = {"object": obj, "start": hx(start), "end": hx(end), "kind": kind,
              "evidence": list(evidence), "callers": 0, "call_sites": 0,
              "callers_exact": False, "ambiguous": bool(ambiguous)}
    if name:
        region["name"] = name
    region.update(extra)
    regions.append(region)


def inventory():
    if not IMAGE.is_file():
        raise FileNotFoundError(f"missing original image: {IMAGE}")
    for lib in LIBS:
        if not lib.is_file():
            raise FileNotFoundError(f"missing pinned runtime library: {lib}")

    image = lemod.LE(IMAGE)
    obj_by_no = {obj["n"]: obj for obj in image.objects}
    code1 = image.object_bytes(obj_by_no[1])[:obj_by_no[1]["vsize"]]
    code2 = image.object_bytes(obj_by_no[2])[:obj_by_no[2]["vsize"]]
    fixups = list(image.resolved_fixups())
    cstarts = c_prologues(code1, C_START, C_ASM)
    if not cstarts or cstarts[0][0] != C_START:
        cstarts.insert(0, (C_START, "main-entry"))
    asmstarts = asm_prologues(code1, C_ASM, ASM_CRT)
    late_c_shapes = [off for off, _ in c_prologues(code1, ASM_CRT - 0x400, ASM_CRT)]

    # libscan uses omf.load() to identify exact code-segment matches with
    # relocation bytes masked. Preserve every hit so duplicate/overlap cases
    # stay visible rather than being arbitrarily resolved.
    lib_results = libscan.scan(str(IMAGE), [str(p) for p in LIBS], obj_index=1)
    member_hits = []
    for result in lib_results:
        for hit in result["hits"]:
            start, end = hit, hit + result["size"]
            if start >= ASM_CRT and end <= len(code1):
                member_hits.append({
                    "start": start, "end": end,
                    "name": f"{result['lib']}:{result['module']}:{result['seg']}",
                    "publics": result["publics"],
                    "evidence": "libscan-masked-exact-hit",
                })
    member_hits.sort(key=lambda item: (item["start"], item["end"], item["name"]))
    crt_entry = image.hdr["eip"] if image.hdr["eip_object"] == 1 else None
    crt_entry_end = min((h["start"] for h in member_hits
                         if crt_entry is not None and h["start"] > crt_entry),
                        default=len(code1))
    crt_string_at = code1.find(b"WATCOM C/C++32 Run-Time system")

    regions: list[dict] = []
    function_refs: list[dict] = []
    # BEGTEXT is independently matched as the exact 16-byte library member.
    add_region(regions, 1, 0, C_START, "crt",
               ["libscan-exact-match", "begtext-thunk"], name="CLIB3S:cstrt386",
               ambiguous=False, confidence="PROVEN")

    # The compiler prologue is a strong start marker.  The next such marker is
    # a deterministic extent boundary; Capstone refines reachable code ends
    # when available, while the surrounding interval remains owned here.
    for i, (start, tag) in enumerate(cstarts):
        end = cstarts[i + 1][0] if i + 1 < len(cstarts) else C_ASM
        add_region(regions, 1, start, end, "c",
                   [tag, "next-c-prologue-boundary"], name=f"c_fn_{start:05X}",
                   ambiguous=CAPSTONE_ERROR is not None,
                   confidence="STRONG", boundary_method="next compiler prologue")
        function_refs.append({"object": 1, "start": start, "end": end,
                              "kind": "c", "name": f"c_fn_{start:05X}"})

    # Hand-written code has several entry-prologue families.  Partition the
    # broad asm range at those signatures, but keep them marked as candidates;
    # without symbols/CFG proof the exact per-routine limits remain ambiguous.
    boundaries = [C_ASM] + [off for off, _ in asmstarts] + late_c_shapes + [ASM_CRT]
    boundaries = sorted(set(x for x in boundaries if C_ASM <= x <= ASM_CRT))
    asm_tags = dict(asmstarts)
    for start, end in zip(boundaries, boundaries[1:]):
        is_late_c = start in late_c_shapes
        is_candidate = start in asm_tags
        if is_late_c:
            kind, evidence = "unknown", ["c-shaped-prologue-outside-c-range", "unmatched-runtime-gap"]
            name = f"ambiguous_fn_{start:05X}"
        else:
            kind = "asm"
            evidence = (["hand-asm-region"] if start == C_ASM else ["asm-prologue-candidate", asm_tags[start]])
            name = f"asm_region_{start:05X}"
        add_region(regions, 1, start, end, kind, evidence, name=name,
                   ambiguous=(kind == "unknown" or is_candidate or CAPSTONE_ERROR is not None),
                   confidence="STRONG" if start == C_ASM else "HYPOTHESIS",
                   boundary_method="prologue candidate or exact asm/CRT boundary")
        if kind == "asm":
            function_refs.append({"object": 1, "start": start, "end": end,
                                  "kind": "asm", "name": name})

    # Partition the runtime interval at all exact match extents. Overlapping
    # members become explicit ambiguous atomic ranges; unmatched gaps are
    # retained as unknown instead of being attributed to a nearby member.
    cuts = {ASM_CRT, len(code1)}
    if crt_entry is not None and ASM_CRT <= crt_entry < len(code1):
        cuts.add(crt_entry)
        cuts.add(crt_entry_end)
    for hit in member_hits:
        cuts.add(hit["start"])
        cuts.add(hit["end"])
    cuts = sorted(cuts)
    for start, end in zip(cuts, cuts[1:]):
        covering = [h for h in member_hits if h["start"] <= start and h["end"] >= end]
        if not covering:
            gap = code1[start:end]
            is_padding = len(gap) <= 0x10 and bool(gap) and all(b in (0x00, 0x90, 0xCC) for b in gap)
            is_cstart = (crt_entry is not None and start >= crt_entry and end <= crt_entry_end
                         and crt_string_at >= crt_entry and crt_string_at < crt_entry_end)
            if is_padding and not is_cstart:
                add_region(regions, 1, start, end, "pad",
                           ["inter-member-alignment", "fill-bytes"],
                           name=f"crt_pad_{start:05X}", ambiguous=True,
                           confidence="HYPOTHESIS", bytes=gap.hex())
            elif is_cstart:
                add_region(regions, 1, start, end, "crt",
                           ["le-entrypoint", "watcom-crt-identification-string", "version-gap"],
                           name="crt_cstart_version_gap", ambiguous=True,
                           confidence="PROVEN-ENTRYPOINT", member_match=False)
            else:
                add_region(regions, 1, start, end, "crt",
                           ["runtime-zone", "no-10.0a-member-match"],
                           name=f"crt_gap_{start:05X}", ambiguous=True,
                           confidence="STRONG-RUNTIME-ATTRIBUTION", member_match=False)
            continue
        names = sorted({h["name"] for h in covering})
        publics = sorted({p for h in covering for p in h["publics"]})
        add_region(regions, 1, start, end, "crt", ["libscan-masked-exact-hit"],
                   name=names[0] if len(names) == 1 else "overlapping-library-matches",
                   ambiguous=len(covering) > 1,
                   confidence="PROVEN-MASKED-MATCH", library_matches=names,
                   publics=publics)
        # Function target identity is the exact beginning of a matched member.
        for hit in covering:
            if hit["start"] == start:
                function_refs.append({"object": 1, "start": start, "end": hit["end"],
                                      "kind": "crt", "name": hit["name"]})

    # Embed potential switch tables in their owning function's extent. A
    # contiguous run of relocated obj1 pointers is visible even without a
    # decoder; indexed indirect jumps add a separate Capstone-based hint.
    tables = fixup_tables(fixups, 1)
    for table in tables:
        owner = next((r for r in regions if r["object"] == 1
                      and int(r["start"], 16) <= table["start"] < int(r["end"], 16)
                      and r["kind"] in ("c", "asm", "crt")), None)
        if owner is not None:
            owner.setdefault("inline_data", []).append({
                "start": hx(table["start"]), "end": hx(table["end"]), "kind": "data",
                "evidence": table["evidence"], "confidence": table["confidence"],
                "targets": [hx(v) for v in table["targets"]],
            })

    for dispatch in raw_switch_dispatches(code1, C_START, ASM_CRT):
        owner = next((r for r in regions if r["object"] == 1
                      and int(r["start"], 16) <= dispatch["site"] < int(r["end"], 16)
                      and r["kind"] in ("c", "asm")), None)
        if owner is not None:
            owner.setdefault("inline_data", []).append({
                "start": hx(dispatch["table"]), "end": None, "kind": "data",
                "evidence": dispatch["evidence"], "confidence": dispatch["confidence"],
                "dispatch_site": hx(dispatch["site"]),
                "note": "table extent is inside the owner's region; exact entry count is unresolved",
            })

    # Object 2 contains four ORG-0 paragraph-aligned IRQ templates. Their
    # placeholder far jump plus NOP identifies the code end; zero fill to the
    # next paragraph is exposed as pad.
    obj2_starts = list(all_hits(code2, OBJ2_TEMPLATE_SIG, 0, len(code2)))
    obj2_starts = [off for off in obj2_starts if off % 0x10 == 0]
    if not obj2_starts or obj2_starts[0] != 0:
        obj2_starts.insert(0, 0)
    obj2_starts = sorted(set(obj2_starts))
    for idx, start in enumerate(obj2_starts):
        limit = obj2_starts[idx + 1] if idx + 1 < len(obj2_starts) else len(code2)
        far = list(all_hits(code2, OBJ2_FAR_END, start, limit))
        code_end = far[-1] + len(OBJ2_FAR_END) if far else limit
        add_region(regions, 2, start, code_end, "asm",
                   ["paragraph-aligned-template", "placeholder-far-jump" if far else "template-signature"],
                   name=f"obj2_irq_template_{idx + 1}", ambiguous=not bool(far),
                   confidence="STRONG" if far else "HYPOTHESIS",
                   template_number=idx + 1, boundary_method="placeholder far-jump terminator")
        function_refs.append({"object": 2, "start": start, "end": code_end,
                              "kind": "asm", "name": f"obj2_irq_template_{idx + 1}"})
        if code_end < limit:
            gap = code2[code_end:limit]
            if all(b in (0x00, 0x90, 0xCC) for b in gap):
                add_region(regions, 2, code_end, limit, "pad",
                           ["paragraph-alignment", "zero-or-nop-fill"],
                           name=f"obj2_pad_{code_end:03X}", ambiguous=False,
                           confidence="STRONG")
            else:
                add_region(regions, 2, code_end, limit, "unknown",
                           ["after-template-terminator"], name=f"obj2_gap_{code_end:03X}",
                           ambiguous=True)

    # Repeated byte-identical library candidates can share one physical entry.
    # Keep one graph node per object offset and retain the competing names.
    unique_refs: dict[tuple[int, int], dict] = {}
    for fn in function_refs:
        key = (fn["object"], fn["start"])
        if key not in unique_refs:
            unique_refs[key] = dict(fn)
            unique_refs[key]["candidate_names"] = [fn["name"]]
        else:
            current = unique_refs[key]
            current["end"] = max(current["end"], fn["end"])
            if fn["name"] not in current["candidate_names"]:
                current["candidate_names"].append(fn["name"])
    function_refs = [unique_refs[key] for key in sorted(unique_refs)]

    # Decode known regions and form a call graph.  In reduced mode, only E8
    # candidates that resolve to a known entry are counted; this is useful but
    # deliberately not presented as an exact graph.
    call_edges = []
    decoded_functions = 0
    if Cs is not None:
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        md.detail = True
        for fn in function_refs:
            if fn["object"] != 1 or fn["kind"] not in ("c", "asm", "crt"):
                continue
            detail = capstone_function(md, code1, fn["start"], min(fn["end"], len(code1)))
            decoded_functions += 1
            region = next((r for r in regions if r["object"] == 1
                           and int(r["start"], 16) == fn["start"]), None)
            if region:
                region["reachable_end"] = hx(detail["reachable_end"])
                region["reachable_instructions"] = detail["reachable_instructions"]
                region["return_sites"] = [hx(v) for v in detail["returns"]]
                region["inline_data"] = region.get("inline_data", [])
                for jump in detail["indirect_jumps"]:
                    if jump["indexed"]:
                        region["inline_data"].append({
                            "kind": "data", "evidence": ["capstone-indexed-indirect-jump"],
                            "start": hx(jump["displacement"] & 0xFFFFFFFF)
                            if jump["displacement"] is not None else None,
                            "end": None, "dispatch": jump,
                            "confidence": "HYPOTHESIS",
                        })
            call_edges.extend({"source": fn["start"], **edge} for edge in detail["calls"])
    else:
        for fn in function_refs:
            if fn["object"] == 1 and fn["kind"] in ("c", "asm", "crt"):
                for site in range(fn["start"], max(fn["start"], fn["end"] - 4)):
                    if code1[site] == 0xE8:
                        disp = int.from_bytes(code1[site + 1:site + 5], "little", signed=True)
                        target = site + 5 + disp
                        call_edges.append({"source": fn["start"], "site": site, "target": target,
                                           "candidate": True})

    start_to_fn = {(fn["object"], fn["start"]): fn for fn in function_refs}
    incoming: dict[tuple[int, int], set[int]] = defaultdict(set)
    incoming_sites: dict[tuple[int, int], int] = defaultdict(int)
    resolved_edges = []
    unique_edges = {}
    for edge in call_edges:
        unique_edges[(edge["source"], edge["site"], edge["target"])] = edge
    call_edges = [unique_edges[key] for key in sorted(unique_edges)]
    for edge in call_edges:
        key = (1, edge["target"])
        target_fn = start_to_fn.get(key)
        if target_fn:
            incoming[key].add(edge["source"])
            incoming_sites[key] += 1
            resolved_edges.append({"from": hx(edge["source"]), "to": hx(edge["target"]),
                                   "site": hx(edge["site"]),
                                   "confidence": "PROVEN-DECODED" if Cs is not None else "HYPOTHESIS-E8-CANDIDATE"})
    for region in regions:
        if region["object"] == 1:
            start = int(region["start"], 16)
            key = (1, start)
            region["callers"] = len(incoming[key])
            region["call_sites"] = incoming_sites[key]
            region["callers_exact"] = Cs is not None

    addressed = set()
    non_entry_code_targets = set()
    for source_obj, _, _, target in fixups:
        if target.get("kind") == "internal" and target.get("obj") in (1, 2):
            key = (target["obj"], target.get("off"))
            if key in start_to_fn:
                addressed.add(key)
            elif isinstance(target.get("off"), int) and target["obj"] == 1:
                non_entry_code_targets.add(target["off"])
    for region in regions:
        key = (region["object"], int(region["start"], 16))
        region["address_taken"] = key in addressed

    callable_regions = [fn for fn in function_refs if fn["object"] == 1]
    no_direct = [{"object": fn["object"], "start": hx(fn["start"]), "name": fn["name"],
                  "candidate_names": fn.get("candidate_names", [fn["name"]]),
                  "address_taken": (fn["object"], fn["start"]) in addressed}
                 for fn in callable_regions if not incoming[(fn["object"], fn["start"])]]
    regions.sort(key=lambda r: (r["object"], int(r["start"], 16), int(r["end"], 16)))

    # The normalized inventory must cover the entire vsize of both code
    # objects without holes/overlaps. Fail loudly if any candidate logic drifts.
    for obj_no, size in ((1, obj_by_no[1]["vsize"]), (2, obj_by_no[2]["vsize"])):
        rs = [r for r in regions if r["object"] == obj_no]
        cursor = 0
        for r in rs:
            start, end = int(r["start"], 16), int(r["end"], 16)
            if start != cursor or end <= start:
                raise ValueError(f"object {obj_no} coverage error at {hx(cursor)}: {r}")
            cursor = end
        if cursor != size:
            raise ValueError(f"object {obj_no} ends at {hx(cursor)}, expected {hx(size)}")

    prologues_outside = [off for off, _ in c_prologues(code1, C_ASM, ASM_CRT)]
    boundary_evidence = {
        "c_to_asm": {
            "offset": hx(C_ASM), "level": "STRONG",
            "reason": (f"{sum(C_START <= off < C_ASM for off, _ in cstarts)} Watcom save-register/frame prologues "
                       f"in [0x10,0x112FA), last start {hx(cstarts[-1][0])}; first hand-asm frame signature "
                       f"at {hx(asmstarts[0][0]) if asmstarts else 'none found'}. The transition is STRONG, "
                       "with the two later C-shaped byte sequences listed as anomalies."),
            "late_c_shaped_signatures": [hx(x) for x in prologues_outside],
        },
        "asm_to_crt": {
            "offset": hx(ASM_CRT), "level": "PROVEN-MASKED-MATCH",
            "reason": "first 10.0a runtime code member after the hand-assembly region; libscan exact masked hit",
            "member": next((h["name"] for h in member_hits if h["start"] == ASM_CRT), None),
        },
    }

    data = {
        "image": str(IMAGE.relative_to(ROOT)).replace("\\", "/"),
        "sha256": image.summary()["sha256"],
        "inventory_version": 1,
        "library_reader": "tools/libscan.py via tools/omf.py",
        "decoder": {"name": "capstone", "available": Cs is not None,
                    "mode": "x86-32 CFG for object 1; object 2 template-marker scan",
                    "error": CAPSTONE_ERROR},
        "objects": [
            {"object": 1, "base": hx(obj_by_no[1]["base"]), "vsize": hx(obj_by_no[1]["vsize"]),
             "region_count": sum(r["object"] == 1 for r in regions)},
            {"object": 2, "base": hx(obj_by_no[2]["base"]), "vsize": hx(obj_by_no[2]["vsize"]),
             "region_count": sum(r["object"] == 2 for r in regions)},
        ],
        "boundaries": boundary_evidence,
        "call_graph": {
            "functions_or_regions": len(callable_regions),
            "direct_edges_to_known_entries": len(resolved_edges),
            "functions_with_direct_callers": sum(1 for fn in callable_regions
                                                  if incoming[(fn["object"], fn["start"])]),
            "functions_never_directly_called": len(no_direct),
            "capstone_decoded_functions": decoded_functions,
            "complete": Cs is not None,
            "edge_method": "capstone-cfg" if Cs is not None else "raw-E8 candidates filtered to known starts",
            "non_entry_obj1_fixup_targets": len(non_entry_code_targets),
            "never_directly_called": no_direct,
            "edges": resolved_edges,
        },
        "evidence_counts": {
            "c_prologue_candidates": len(cstarts),
            "asm_prologue_candidates": len(asmstarts),
            "runtime_library_segments_matched": len({h["name"] for h in member_hits}),
            "runtime_library_hit_extents": len(member_hits),
            "library_scan_total_segments_found": sum(1 for r in lib_results if r["hits"]),
            "runtime_library_unmatched_bytes": sum(int(r["end"], 16) - int(r["start"], 16)
                                                    for r in regions if r["object"] == 1
                                                    and r["kind"] == "crt" and r.get("member_match") is False),
            "obj2_templates": len(obj2_starts),
        },
        "regions": regions,
    }
    return data


def main():
    data = inventory()
    out = ROOT / "build" / "inventory.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    print(f"KE.EXE code inventory: obj1={data['objects'][0]['region_count']} regions, "
          f"obj2={data['objects'][1]['region_count']} regions")
    print(f"C starts={data['evidence_counts']['c_prologue_candidates']}; "
          f"asm signatures={data['evidence_counts']['asm_prologue_candidates']}; "
          f"runtime members={data['evidence_counts']['runtime_library_segments_matched']}/"
          f"{data['evidence_counts']['library_scan_total_segments_found']} total "
          f"({data['evidence_counts']['runtime_library_hit_extents']} runtime hit extents); "
          f"obj2 templates={data['evidence_counts']['obj2_templates']}")
    print(f"call graph edges={data['call_graph']['direct_edges_to_known_entries']} "
          f"(complete={data['call_graph']['complete']}); written {out.relative_to(ROOT)}")
    if CAPSTONE_ERROR:
        print(f"WARNING: Capstone unavailable ({CAPSTONE_ERROR}); caller counts use raw E8 candidates.")


if __name__ == "__main__":
    main()
