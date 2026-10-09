#!/usr/bin/env python3
"""Apply explicitly named XRT core sections to the existing embedded snapshots.

Unlike a product upgrade this preserves all unselected source and extension
sections. Every changed span and its exact old surrounding context must match
the frozen previous header, before any output is changed. Unchanged differences
already present in the embedded snapshot are preserved, never imported.
"""
from __future__ import annotations

import argparse
import difflib
from pathlib import Path
import re

from sync_xrt_snapshot import _api_names, _check_snapshot, _metadata, _write_if_changed
from sync_xrt_snapshot import render_symbol_table, sync_capability_matrix
import json


SECTION = re.compile(r"^/\* =+ \*/\n/\* (public|source): ([^\n]+) \*/\n/\* =+ \*/\n", re.M)


def section(text: str, kind: str, name: str) -> tuple[int, int]:
    markers = list(SECTION.finditer(text))
    matches = [(i, marker) for i, marker in enumerate(markers)
               if marker[1] == kind and marker[2] == name]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one section {kind}: {name}, got {len(matches)}")
    i, marker = matches[0]
    if i + 1 == len(markers):
        raise ValueError("terminal section requires a product-level update")
    return marker.end(), markers[i + 1].start()


def replace(text: str, old: str, new: str, kind: str, name: str) -> str:
    start, end = section(text, kind, name)
    old_start, old_end = section(old, kind, name)
    new_start, new_end = section(new, kind, name)
    old_lines = old[old_start:old_end].splitlines(keepends=True)
    new_lines = new[new_start:new_end].splitlines(keepends=True)
    current = text[start:end]
    # Nearby edits share guard lines. Applying them independently would change
    # the next edit's OLD guard and falsely refuse an exact source delta. Merge
    # overlapping contexts before validation, never weaken the context match.
    changes = list(difflib.SequenceMatcher(None, old_lines, new_lines, autojunk=False).get_grouped_opcodes(4))
    for group in reversed(changes):
        a, b = group[0][1], group[-1][2]
        c, d = group[0][3], group[-1][4]
        needle = "".join(old_lines[a:b])
        replacement = "".join(new_lines[c:d])
        if current.count(needle) == 0 and current.count(replacement) == 1:
            continue  # Exact already-applied span; never append it twice.
        if current.count(needle) != 1:
            raise ValueError(f"changed old span differs/ambiguous in {kind}: {name}; refuse import")
        current = current.replace(needle, replacement, 1)
    return text[:start] + current + text[end:]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--previous", type=Path, required=True, help="frozen previous single/ directory")
    parser.add_argument("--current", type=Path, required=True, help="frozen current single/ directory")
    parser.add_argument("--public", action="append", default=[])
    parser.add_argument("--source", action="append", default=[])
    args = parser.parse_args()
    if not args.public and not args.source:
        parser.error("name at least one explicit source or public section")
    root = args.root.resolve()
    replacements: dict[Path, bytes] = {}
    for declarations in (False, True):
        leaf = "xrt_decl.h" if declarations else "xrt.h"
        old = (args.previous / leaf).read_text(encoding="utf-8")
        new = (args.current / leaf).read_text(encoding="utf-8")
        paths = (root / "release/include/xrt_decl.h", root / "release/include/xruntime_decl.h") if declarations else (
            root / "lib/xrt/xrt.h", root / "lib/xruntime/xruntime.h")
        for path in paths:
            text = path.read_text(encoding="utf-8")
            for name in args.public:
                text = replace(text, old, new, "public", name)
            if not declarations:
                for name in args.source:
                    text = replace(text, old, new, "source", name)
            replacements[path] = text.encode("utf-8")
    # All four previous change spans are checked before any snapshot is written.
    for path, data in replacements.items():
        _write_if_changed(path, data)
    core = root / "lib/xrt/xrt.h"
    runtime = root / "lib/xruntime/xruntime.h"
    core_names, runtime_names = set(_api_names(core)), set(_api_names(runtime))
    for product, implementation, declarations, owned in (
        ("xrt", core, root / "release/include/xrt_decl.h", None),
        ("xruntime", runtime, root / "release/include/xruntime_decl.h", len(runtime_names - core_names)),
    ):
        prefix = "xrt/single/" if product == "xrt" else "xrt/extlibs/xruntime/single/"
        metadata = _metadata(product, prefix + implementation.name, prefix + declarations.name,
                             implementation.read_bytes(), declarations.read_bytes(),
                             len(core_names if product == "xrt" else runtime_names), owned)
        _write_if_changed(implementation.with_name(product + ".snapshot.json"),
                          (json.dumps(metadata, indent=2) + "\n").encode("utf-8"))
    _write_if_changed(root / "src/api/xl_xrt_symbols.inc", render_symbol_table(runtime).encode("utf-8"))
    sync_capability_matrix(root)
    return 0 if _check_snapshot(root) else 1


if __name__ == "__main__":
    raise SystemExit(main())
