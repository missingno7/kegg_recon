#!/usr/bin/env python3
"""Refresh or validate the SDL3 portability map against manifest identities."""

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "manifest.json"
PORTING = ROOT / "docs" / "porting.json"
CLASSES = ("KEEP", "ADAPT", "REIMPLEMENT")


def identity(record):
    return (record.get("object", 1), record.get("start"))


def identity_key(object_number, start):
    return "{}:{}".format(object_number, start.lower())


def read_json(path):
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def load_manifest():
    data = read_json(MANIFEST)
    records = {}
    for function in data.get("functions", []):
        ident = identity(function)
        if ident in records:
            raise ValueError("duplicate manifest function identity {}".format(ident))
        records[ident] = function
    return records


def load_porting():
    data = read_json(PORTING)
    if data.get("format") != "kegg-porting-map-1" or not isinstance(data.get("functions"), dict):
        raise ValueError("unsupported docs/porting.json format")
    return data


def rename_text(text, rx, renamed):
    return rx.sub(lambda m: renamed[m.group(1)], text) if rx else text


def rename_values(value, rx, renamed):
    if isinstance(value, str):
        return rename_text(value, rx, renamed)
    if isinstance(value, list):
        return [rename_values(v, rx, renamed) for v in value]
    if isinstance(value, dict):
        return {k: rename_values(v, rx, renamed) for k, v in value.items()}
    return value


def refresh():
    """names/units/src from the manifest by (object, start); every other old symbol name (callers, callees, state,
    prose) through the manifest's cumulative rename log; `name` @ `addr` pairs in docs/porting.md by address"""
    import re
    manifest = load_manifest()
    renamed = read_json(MANIFEST).get("renamed", {})
    renamed = {k: v for k, v in renamed.items() if k != v}
    rx = re.compile(r"\b(" + "|".join(map(re.escape, sorted(renamed, key=len, reverse=True))) + r")\b") if renamed else None
    data = load_porting()
    data["functions"] = {k: rename_values(r, rx, renamed) for k, r in data["functions"].items()}
    for k in list(data):
        if k != "functions":
            data[k] = rename_values(data[k], rx, renamed)
    md = ROOT / "docs" / "porting.md"
    if md.exists():
        by_start = {int(f["start"], 16): f["name"] for (obj, _), f in manifest.items() if obj == 1}
        text = rename_text(md.read_text(encoding="utf-8"), rx, renamed)
        text = re.sub(r"`(\w+)` @ `(0x[0-9a-fA-F]+)`",
                      lambda m: "`{}` @ `{}`".format(by_start.get(int(m.group(2), 16), m.group(1)), m.group(2)), text)
        md.write_text(text, encoding="utf-8", newline="\n")
    for key, record in data["functions"].items():
        ident = identity(record)
        current = manifest.get(ident)
        if current is None:
            continue
        expected_key = identity_key(*ident)
        if key != expected_key:
            raise ValueError("record key {} does not match identity {}".format(key, expected_key))
        record["name"] = current["name"]
        record["unit"] = current.get("unit", "")
        record["src"] = current.get("src", "")
    with PORTING.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, indent=2, ensure_ascii=False)
        stream.write("\n")


def check():
    manifest = load_manifest()
    data = load_porting()
    mapped = {}
    errors = []
    for key, record in data["functions"].items():
        ident = identity(record)
        expected_key = identity_key(*ident)
        if key != expected_key:
            errors.append("key {} does not match record identity {}".format(key, expected_key))
        if ident not in manifest:
            errors.append("record {} points to no manifest function".format(key))
        elif ident in mapped:
            errors.append("duplicate record identity {}".format(ident))
        else:
            mapped[ident] = record
    for ident in sorted(manifest.keys() - mapped.keys()):
        errors.append("manifest function {} is missing".format(identity_key(*ident)))

    counts = defaultdict(lambda: {name: 0 for name in CLASSES})
    totals = defaultdict(int)
    for record in data["functions"].values():
        subsystem = record.get("subsystem", "(missing)")
        class_name = record.get("class")
        totals[subsystem] += 1
        if class_name in CLASSES:
            counts[subsystem][class_name] += 1
        else:
            errors.append("{} has invalid class {!r}".format(identity_key(*identity(record)), class_name))
    print("{:<34} {:>6} {:>6} {:>12} {:>7}".format("Subsystem", *CLASSES, "TOTAL"))
    for subsystem in sorted(totals):
        row = counts[subsystem]
        print("{:<34} {:>6} {:>6} {:>12} {:>7}".format(
            subsystem, row["KEEP"], row["ADAPT"], row["REIMPLEMENT"], totals[subsystem]))
    if errors:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("OK: {} manifest functions mapped exactly once.".format(len(manifest)))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--refresh", action="store_true", help="refresh names, units and source paths from manifest.json")
    group.add_argument("--check", action="store_true", help="check identity coverage and print class counts")
    args = parser.parse_args()
    try:
        if args.refresh:
            refresh()
        else:
            return check()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print("portmap: {}".format(exc), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
