#!/usr/bin/env python3
"""Read-only inventory of C/ASM types, constants, and catalogue drift.

Usage: python tools/structs.py [--root .] [--catalogue build/workers/types/types.json]
Only the Python standard library is used.  The C layout reader follows the
repository's packed-1 rule; unknown declarators are kept in the report rather
than guessed.
"""
import argparse
import collections
import json
import re
from pathlib import Path


def clean_c(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', '""', text)


def strip_c_comments(text):
    """Remove comments while preserving quoted text and physical line numbers."""
    out, i, state = [], 0, "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if ch == '"':
                state = "string"
                out.append(ch)
            elif ch == "'":
                state = "char"
                out.append(ch)
            elif ch == "/" and nxt == "*":
                state = "block"
                out.extend((" ", " "))
                i += 1
            elif ch == "/" and nxt == "/":
                state = "line"
                out.extend((" ", " "))
                i += 1
            else:
                out.append(ch)
        elif state in {"string", "char"}:
            out.append(ch)
            if ch == "\\" and i + 1 < len(text):
                i += 1
                out.append(text[i])
            elif (state == "string" and ch == '"') or (state == "char" and ch == "'"):
                state = "code"
        elif state == "block":
            if ch == "*" and nxt == "/":
                out.extend((" ", " "))
                i += 1
                state = "code"
            elif ch == "\n":
                out.append("\n")
        elif state == "line":
            if ch == "\n":
                out.append("\n")
                state = "code"
        i += 1
    return "".join(out)


def c_defines(source, display_path):
    """Collect object/function macros, including values continued with backslashes."""
    text = strip_c_comments(source)
    out, pending, start_line = [], "", None
    for number, line in enumerate(text.splitlines(), 1):
        if not pending:
            start_line = number
            pending = line
        else:
            pending += line
        if pending.rstrip().endswith("\\"):
            pending = pending.rstrip()[:-1] + " "
            continue
        m = re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?(?:\s+(.*))?$", pending)
        if m:
            out.append({"name": m.group(1), "value": (m.group(3) or "").strip(),
                        "args": m.group(2), "line": start_line, "file": display_path})
        pending = ""
    if pending:
        m = re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?(?:\s+(.*))?$", pending)
        if m:
            out.append({"name": m.group(1), "value": (m.group(3) or "").strip(),
                        "args": m.group(2), "line": start_line, "file": display_path})
    return out


def split_top(text, sep=","):
    out, start, depth = [], 0, 0
    for i, c in enumerate(text):
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == sep and depth == 0:
            out.append(text[start:i].strip())
            start = i + 1
    out.append(text[start:].strip())
    return [x for x in out if x]


def matching(text, start, left="{", right="}"):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == left:
            depth += 1
        elif text[i] == right:
            depth -= 1
            if depth == 0:
                return i
    return -1


def c_types(path, display_path=None):
    source = path.read_text(encoding="latin-1")
    text = clean_c(source)
    display_path = (display_path or path).as_posix()
    found = []
    # Composite definitions. Alias and tag are both indexed to the same layout.
    pat = re.compile(r"(?P<pre>typedef\s+)?(?P<kind>struct|union|enum)\s*(?P<tag>[A-Za-z_]\w*)?\s*\{")
    pos = 0
    while True:
        m = pat.search(text, pos)
        if not m:
            break
        end = matching(text, m.end() - 1)
        if end < 0:
            break
        semi = text.find(";", end)
        if semi < 0:
            break
        body = text[m.end():end]
        suffix = text[end + 1:semi].strip()
        kind, tag = m.group("kind"), m.group("tag")
        if kind == "enum":
            vals, nextval = [], 0
            for item in split_top(body):
                name, eq, value = item.partition("=")
                name = name.strip()
                if not re.fullmatch(r"[A-Za-z_]\w*", name):
                    continue
                raw = value.strip() if eq else (str(nextval) if nextval is not None else None)
                try:
                    number = enum_integer(raw) if raw is not None else None
                    nextval = number + 1 if number is not None else None
                except ValueError:
                    nextval = None
                vals.append({"name": name, "value": raw})
            layout = {"kind": kind, "values": vals}
        else:
            fields, offset, valid = [], 0, True
            bit_start, bit_type, bits_used, bit_capacity = None, None, 0, 0
            for decl in split_top(body, ";"):
                if not decl:
                    continue
                # A nested anonymous aggregate remains explicit/unknown instead of
                # silently receiving an incorrect byte offset.  Resolve its
                # immediate packed fields, which covers anonymous union views.
                if "{" in decl:
                    nm = re.search(r"\}\s*(\w+)\s*((?:\[\s*\d+\s*\])*)\s*$", decl)
                    op = decl.find("{")
                    cl = matching(decl, op)
                    inner_kind = re.search(r"\b(struct|union)\b", decl[:op])
                    width = 0
                    ok = bool(nm and cl >= 0 and inner_kind)
                    if ok:
                        for inner in split_top(decl[op + 1:cl], ";"):
                            ibase, inames = parse_field_decl(inner)
                            iw = primitive_size(ibase)
                            if not inames or iw is None:
                                ok = False
                                break
                            for _, icount, ibits in inames:
                                nbytes = ((ibits + 7) // 8 if ibits is not None else iw * icount)
                                if inner_kind.group(1) == "union":
                                    width = max(width, nbytes)
                                else:
                                    width += nbytes
                    if ok:
                        count = 1
                        for dim in re.findall(r"\[\s*(\d+)\s*\]", nm.group(2)):
                            count *= int(dim)
                        width *= count
                        field_offset = offset if kind == "struct" else 0
                        fields.append({"name": nm.group(1), "type": f"@anonymous-{inner_kind.group(1)}",
                                       "count": count, "bit_width": None, "offset": field_offset, "width": width})
                        if kind == "struct":
                            offset += width
                    else:
                        fields.append({"declaration": re.sub(r"\s+", " ", decl), "offset": None, "width": None})
                        valid = False
                    continue
                base, names = parse_field_decl(decl)
                if not names:
                    fields.append({"declaration": re.sub(r"\s+", " ", decl), "offset": None, "width": None})
                    valid = False
                    continue
                for name, count, bitwidth in names:
                    unit = primitive_size(base)
                    width = unit * count if unit is not None else None
                    if bitwidth is not None:
                        width = (bitwidth + 7) // 8 if unit is not None else None
                        if kind == "struct" and unit is not None:
                            if bit_start is None or bit_type != base or bits_used + bitwidth > bit_capacity:
                                bit_start, bit_type, bits_used, bit_capacity = offset, base, 0, unit * 8
                                offset += unit
                            field_offset = bit_start
                            bits_used += bitwidth
                        else:
                            field_offset = offset if kind == "struct" else 0
                    else:
                        bit_start, bit_type, bits_used, bit_capacity = None, None, 0, 0
                        field_offset = offset if kind == "struct" else 0
                    fields.append({"name": name, "type": base, "count": count, "bit_width": bitwidth,
                                   "offset": field_offset if width is not None else None, "width": width})
                    if width is None:
                        valid = False
                    elif kind == "struct" and bitwidth is None:
                        offset += width
            layout = {"kind": kind, "fields": fields, "size": offset if valid and kind == "struct" else
                      (max((f["width"] or 0 for f in fields), default=0) if valid else None)}
        names = []
        if tag:
            names.append(tag)
        if m.group("pre"):
            for alias in split_top(suffix):
                nm = declarator_name(alias)
                if nm:
                    names.append(nm)
        elif not tag:
            nm = declarator_name(suffix)
            if nm:
                names.append(nm)
        decl_id = f"{display_path}:{text.count(chr(10), 0, m.start()) + 1}"
        for name in dict.fromkeys(names):
            found.append({"name": name, "kind": kind, "layout": layout, "decl": decl_id})
        pos = semi + 1

    # Simple typedef aliases, including scalar/pointer typedefs.
    for m in re.finditer(r"\btypedef\s+([^;{}]+);", text):
        raw = re.sub(r"\s+", " ", m.group(1).strip())
        items = split_top(raw)
        base_alias = None
        for i, item in enumerate(items):
            nm = declarator_name(item)
            if not nm:
                continue
            at = item.rfind(nm)
            alias = (item[:at] + item[at + len(nm):]).strip()
            if alias in {"*", "**", "***"} and base_alias:
                alias = base_alias + alias
            if alias:
                base_alias = alias
            elif base_alias:
                alias = base_alias
            decl_id = f"{display_path}:{text.count(chr(10), 0, m.start()) + 1}"
            if not any(t["name"] == nm and t["decl"] == decl_id for t in found):
                found.append({"name": nm, "kind": "typedef", "layout": {"alias": alias}, "decl": decl_id})

    # Explicit forward declarations are inventory entries, not complete layouts.
    for m in re.finditer(r"^\s*(struct|union|enum)\s+([A-Za-z_]\w*)\s*;\s*$", text, re.M):
        found.append({"name": m.group(2), "kind": "forward", "layout": {},
                      "decl": f"{display_path}:{text.count(chr(10), 0, m.start()) + 1}"})

    defines = c_defines(source, display_path)
    enum_seen = set()
    for t in found:
        if t.get("kind") == "enum":
            for value in t.get("layout", {}).get("values", []):
                key = (t.get("decl"), value["name"])
                if key in enum_seen:
                    continue
                enum_seen.add(key)
                defines.append({"name": value["name"], "value": value.get("value"), "args": None,
                                "line": int(t["decl"].rsplit(":", 1)[1]), "file": display_path,
                                "kind": "enum"})
    return found, defines


def enum_integer(value):
    if value is None:
        return None
    value = value.strip()
    m = re.fullmatch(r"([+-]?)(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*)([uUlL]*)", value)
    if not m:
        return None
    sign = -1 if m.group(1) == "-" else 1
    digits = m.group(2)
    base = 16 if digits.lower().startswith("0x") else (8 if len(digits) > 1 and digits.startswith("0") else 10)
    return sign * int(digits, base)


def declarator_name(text):
    text = text.split(":", 1)[0].strip()
    m = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^]]*\]\s*)*$", text)
    return m.group(1) if m else None


def parse_field_decl(text):
    # Each comma-separated field uses the first declarator's type prefix.
    first = split_top(text)[0]
    m = re.match(r"(?P<base>.*?)(?P<name>[A-Za-z_]\w*)\s*(?P<dims>(?:\[[^]]*\]\s*)*)(?::\s*(?P<bits>\d+))?$", first.strip())
    if not m:
        return re.sub(r"\s+", " ", first.strip()), []
    base = re.sub(r"\s+", " ", m.group("base").strip())
    names = []
    for i, part in enumerate(split_top(text)):
        if i == 0:
            dm = m
        else:
            dm = re.match(r"(?P<name>[A-Za-z_]\w*)\s*(?P<dims>(?:\[[^]]*\]\s*)*)(?::\s*(?P<bits>\d+))?$", part.strip())
            if not dm:
                continue
        dims = re.findall(r"\[\s*(\d+)\s*\]", dm.group("dims") or "")
        count = 1
        for d in dims:
            count *= int(d)
        names.append((dm.group("name"), count, int(dm.group("bits")) if dm.group("bits") else None))
    return base, names


def primitive_size(base):
    b = re.sub(r"\b(const|volatile|register|static)\b", " ", base)
    b = re.sub(r"\s+", " ", b).strip()
    if "*" in b:
        return 4
    if b in {"unsigned", "signed"}:
        return 4
    return {"char": 1, "short": 2, "int": 4, "long": 4, "float": 4, "double": 8,
            "unsigned char": 1, "unsigned short": 2, "unsigned int": 4, "unsigned long": 4,
            "signed char": 1, "signed short": 2, "signed int": 4, "signed long": 4,
            "void": 0}.get(b)


def asm_types(path, display_path=None):
    lines = path.read_text(encoding="latin-1").splitlines()
    display_path = (display_path or path).as_posix()
    types, equs, active = [], [], None
    for no, raw in enumerate(lines, 1):
        line = raw.split(";", 1)[0].strip()
        m = re.match(r"(?:(\w+)\s+)?STRUC\b", line, re.I)
        if m:
            active = {"name": m.group(1) or "<anonymous>", "kind": "STRUC", "fields": [], "decl": f"{display_path}:{no}"}
            continue
        if active:
            if re.match(r"END\s*(?:S|STRUC)\b", line, re.I):
                types.append(active)
                active = None
            elif line:
                f = re.match(r"(\w+)\s+(DB|DW|DD|DQ|DT)\s*(.*)$", line, re.I)
                if f:
                    width = {"DB": 1, "DW": 2, "DD": 4, "DQ": 8, "DT": 10}[f.group(2).upper()]
                    count = 1
                    mult = re.match(r"(\d+)\s+DUP", f.group(3), re.I)
                    if mult:
                        count = int(mult.group(1))
                    off = sum(x["width"] for x in active["fields"])
                    active["fields"].append({"name": f.group(1), "offset": off, "width": width * count})
        eq = re.match(r"^(\w+)\s+EQU\s+(.+)$", line, re.I)
        if eq:
            equs.append({"name": eq.group(1), "value": eq.group(2).strip(), "line": no, "file": display_path})
    for t in types:
        fields = t.pop("fields")
        t["layout"] = {"kind": "STRUC", "fields": fields,
                        "size": sum(x["width"] for x in fields)}
    return types, equs


def layout_key(t):
    layout = t.get("layout", {})
    if "fields" in layout:
        return (layout.get("kind"), tuple((f.get("offset"), f.get("width"), f.get("bit_width"))
                                           for f in layout["fields"]), layout.get("size"))
    return (layout.get("kind"), json.dumps(layout, sort_keys=True))


def type_signature(t):
    layout = t.get("layout", {})
    if "fields" in layout:
        return (layout.get("kind"), layout.get("size"), tuple(
            (f.get("name"), f.get("offset"), f.get("width"), f.get("bit_width"))
            for f in layout["fields"]))
    if layout.get("kind") == "enum":
        return ("enum", tuple((x.get("name"), x.get("value")) for x in layout.get("values", [])))
    return None


def field_type_signature(t):
    return tuple((f.get("name"), f.get("type"), f.get("count"), f.get("bit_width"))
                 for f in t.get("layout", {}).get("fields", []))


def normalized_constant(value):
    value = value.strip()
    if re.fullmatch(r"0[xX][0-9a-fA-F]+[uUlL]*", value):
        return int(re.sub(r"[uUlL]+$", "", value), 16)
    m = re.fullmatch(r"([0-9a-fA-F]+)[hH]", value)
    if m:
        return int(m.group(1), 16)
    if re.fullmatch(r"[+-]?\d+[uUlL]*", value):
        return int(re.sub(r"[uUlL]+$", "", value), 10)
    return re.sub(r"\s+", " ", value).upper()


def catalogue_aliases(legacy_views, catalogue_names):
    aliases = collections.defaultdict(set)
    for old_names, explanation in legacy_views.items():
        targets = {name for name in catalogue_names
                   if re.search(r"\b" + re.escape(name) + r"\b", explanation)}
        for alias in old_names.split("/"):
            alias = alias.strip()
            if alias:
                aliases[alias].update(targets)
    return aliases


def resolve_aggregate_widths(types):
    sizes = {t["name"]: t.get("layout", {}).get("size") for t in types
             if t.get("layout", {}).get("size") is not None}
    for _ in range(4):
        changed = False
        for t in types:
            layout = t.get("layout", {})
            fields = layout.get("fields")
            if not fields or layout.get("kind") not in {"struct", "union"}:
                continue
            offset, maxwidth, valid = 0, 0, True
            bit_start, bit_type, bits_used, bit_capacity = None, None, 0, 0
            for f in fields:
                typ = f.get("type", "")
                width1 = primitive_size(typ)
                if width1 is None and "*" not in typ:
                    tag = re.sub(r"^(?:struct|union|enum)\s+", "", typ).strip()
                    width1 = sizes.get(tag)
                count = f.get("count", 1)
                if width1 is None and typ.startswith("@anonymous") and f.get("width") is not None:
                    width1 = f["width"] // max(count, 1)
                width = width1 * count if width1 is not None else None
                bw = f.get("bit_width")
                if bw is not None and width1 is not None:
                    width = (bw + 7) // 8
                    if layout["kind"] == "struct":
                        if bit_start is None or bit_type != typ or bits_used + bw > bit_capacity:
                            bit_start, bit_type, bits_used, bit_capacity = offset, typ, 0, width1 * 8
                            offset += width1
                        field_offset = bit_start
                        bits_used += bw
                    else:
                        field_offset = 0
                else:
                    bit_start, bit_type, bits_used, bit_capacity = None, None, 0, 0
                    field_offset = offset if layout["kind"] == "struct" else 0
                    if layout["kind"] == "struct" and width is not None:
                        offset += width
                f["width"] = width
                f["offset"] = field_offset if width is not None else None
                if width is None:
                    valid = False
                else:
                    maxwidth = max(maxwidth, field_offset + width)
            new_size = (offset if layout["kind"] == "struct" else maxwidth) if valid else None
            if layout.get("size") != new_size:
                layout["size"] = new_size
                changed = True
            if new_size is not None and sizes.get(t["name"]) != new_size:
                sizes[t["name"]] = new_size
                changed = True
        if not changed:
            break


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--catalogue", default="build/workers/types/types.json")
    ap.add_argument("--overlay", help="use same-named candidate source files for comparison")
    ap.add_argument("--json", help="also write full inventory as JSON")
    args = ap.parse_args()
    root = Path(args.root).resolve()
    overlay = (root / args.overlay).resolve() if args.overlay else None
    types, constants = [], []
    for p in sorted((root / "src").glob("*.c")):
        candidate = overlay / p.name if overlay else None
        ts, ds = c_types(candidate if candidate and candidate.exists() else p, p.relative_to(root))
        types.extend(ts)
        constants.extend(ds)
    for p in sorted((root / "asm").glob("*.asm")):
        ts, ds = asm_types(p, p.relative_to(root))
        types.extend(ts)
        constants.extend(ds)
    resolve_aggregate_widths(types)
    # Same spelling with distinct declarations/layouts.
    by_name = collections.defaultdict(list)
    for t in types:
        if t.get("kind") not in {"typedef", "forward"}:
            by_name[t["name"]].append(t)
    conflicts = []
    for name, entries in sorted(by_name.items()):
        sigs = {type_signature(e) for e in entries}
        sigs.discard(None)
        if len(sigs) > 1:
            conflicts.append({"name": name, "declarations": entries})
    field_type_differences = []
    for name, entries in sorted(by_name.items()):
        if len(entries) > 1 and len({type_signature(e) for e in entries}) == 1:
            sigs = {field_type_signature(e) for e in entries}
            if len(sigs) > 1:
                field_type_differences.append({"name": name, "declarations": entries})
    # Different spellings with the same resolved offset/width profile.
    groups = collections.defaultdict(list)
    for t in types:
        if t.get("layout", {}).get("fields"):
            groups[layout_key(t)].append(t)
    duplicates = []
    for entries in groups.values():
        names = sorted({e["name"] for e in entries})
        if len(names) > 1:
            duplicates.append({"names": names, "declarations": entries})

    cat_drift, catalogue_matches = [], []
    catpath = root / args.catalogue
    if catpath.exists():
        catalogue = json.loads(catpath.read_text(encoding="utf-8"))
        catalogue_names = {c.get("name") for c in catalogue.get("types", [])}
        aliases = catalogue_aliases(catalogue.get("legacy_views", {}), catalogue_names)
        src_by_name = by_name
        matched_decls = set()
        for c in catalogue.get("types", []):
            name = c.get("name")
            entries = list(src_by_name.get(name, []))
            for alias, targets in aliases.items():
                if name in targets:
                    entries.extend(src_by_name.get(alias, []))
            # The catalogue records owning units. Use those as a constrained fallback
            # for source tags whose local name differs, and require every member
            # offset/width plus total size to match before calling it an alias.
            listed_units = set(c.get("units", []))
            for t in types:
                decl_file = t.get("decl", "").rsplit(":", 1)[0]
                if decl_file in listed_units and t not in entries:
                    entries.append(t)
            cf = c.get("fields", [])
            expected = tuple((f.get("offset_bytes"), f.get("width_bytes")) for f in cf)
            matched = False
            matching_entries = []
            for e in entries:
                fs = e.get("layout", {}).get("fields", [])
                actual = tuple((f.get("offset"), f.get("width")) for f in fs)
                size = e.get("layout", {}).get("size")
                if actual == expected and (c.get("size_bytes") is None or size == c.get("size_bytes")):
                    matched = True
                    matching_entries.append(e)
                    matched_decls.add(e.get("decl"))
            if not entries:
                cat_drift.append({"name": name, "problem": "catalogue type has no source declaration"})
            elif not matched:
                cat_drift.append({"name": name, "problem": "source offsets/widths or size differ/unknown",
                                  "catalogue_size": c.get("size_bytes"), "catalogue_fields": list(expected),
                                  "source": entries})
            else:
                source_names = sorted({e["name"] for e in matching_entries})
                catalogue_matches.append({"name": name, "source_names": source_names,
                                          "declarations": [e.get("decl") for e in matching_entries]})
        for name in sorted(set(src_by_name) - catalogue_names):
            rows = src_by_name[name]
            if name not in aliases and not all(row.get("decl") in matched_decls for row in rows):
                cat_drift.append({"name": name, "problem": "source type absent from catalogue", "source": rows})

    by_constant = collections.defaultdict(list)
    for c in constants:
        if not c.get("args"):
            by_constant[c["name"]].append(c)
    constant_conflicts = [{"name": name, "definitions": rows}
                          for name, rows in sorted(by_constant.items())
                          if len({normalized_constant(r["value"]) for r in rows}) > 1]
    report = {"types": types, "constants": constants, "same_name_layout_conflicts": conflicts,
              "same_name_field_type_differences": field_type_differences,
              "identical_layouts_different_names": duplicates, "catalogue_matches": catalogue_matches,
              "catalogue_drift": cat_drift,
              "constant_conflicts": constant_conflicts}
    print(f"Types: {len(types)} declarations ({len(by_name)} names); constants: {len(constants)}")
    print(f"Same-name member/offset conflicts: {len(conflicts)}")
    for x in conflicts:
        places = ", ".join(f"{e['decl']} ({e['layout'].get('size', '?')} bytes)" for e in x["declarations"])
        print(f"  {x['name']}: {places}")
    print(f"Same-layout member type differences: {len(field_type_differences)}")
    for x in field_type_differences:
        places = ", ".join(f"{e['decl']} ({e['layout'].get('size', '?')} bytes)" for e in x["declarations"])
        print(f"  {x['name']}: {places}")
    print(f"Identical layout groups under different names: {len(duplicates)}")
    for x in duplicates:
        print(f"  {', '.join(x['names'])}: {x['declarations'][0]['layout'].get('size', '?')} bytes")
    print(f"Catalogue drift items: {len(cat_drift)}")
    for x in cat_drift:
        print(f"  {x['name']}: {x['problem']}")
    renamed_matches = [x for x in catalogue_matches if x["name"] not in x["source_names"]]
    print(f"Catalogue matches via alternate source names: {len(renamed_matches)}")
    for x in renamed_matches:
        print(f"  {x['name']}: {', '.join(x['source_names'])}")
    print(f"Conflicting constant definitions: {len(constant_conflicts)}")
    for x in constant_conflicts:
        defs = "; ".join(f"{d['file']}={d['value']}" for d in x["definitions"])
        print(f"  {x['name']}: {defs}")
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
