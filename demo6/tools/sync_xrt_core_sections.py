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


SECTION = re.compile(r"^/\* =+ \*/\n/\* (public|internal|source|feature selection|requested feature selection): ([^\n]+) \*/\n/\* =+ \*/\n", re.M)
TERMINAL_FOOTER = "\n#endif\n#endif\n\n#endif\n"


def guarded_delta(text: str, old: str, new: str, label: str) -> str:
    """Apply exact guarded edits, including duplicated declaration selectors.

    Repeated contexts must have the same population as the old generator
    output; never replace an arbitrary number of approximate occurrences.
    """
    left, right = old.splitlines(keepends=True), new.splitlines(keepends=True)
    # Whole generated declarations contain tens of thousands of repeated C
    # braces/guards. Popular-line filtering only selects anchors; application
    # below still requires exact old bytes and exact occurrence populations.
    for group in reversed(list(difflib.SequenceMatcher(None, left, right, autojunk=True).get_grouped_opcodes(4))):
        needle = "".join(left[group[0][1]:group[-1][2]])
        replacement = "".join(right[group[0][3]:group[-1][4]])
        count = old.count(needle)
        if text.count(needle) == 0 and text.count(replacement) == new.count(replacement):
            continue
        if not needle or text.count(needle) != count:
            raise ValueError(f"changed old span differs/ambiguous in {label}; refuse import")
        text = text.replace(needle, replacement)
    return text


def section(text: str, kind: str, name: str, allow_terminal: bool = False) -> tuple[int, int]:
    markers = list(SECTION.finditer(text))
    matches = [(i, marker) for i, marker in enumerate(markers)
               if marker[1] == kind and marker[2] == name]
    if not matches and kind == "public" and name == "include/xrt/features.h":
        matches = [(i, marker) for i, marker in enumerate(markers)
                   if marker[1] == "feature selection" and marker[2] == name]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one section {kind}: {name}, got {len(matches)}")
    i, marker = matches[0]
    if i + 1 == len(markers):
        # Explicit xruntime source delta only. Preserve the known implementation
        # and header footer; never import product-wide trailing feature guards.
        if not allow_terminal or kind != "source" or not text.endswith(TERMINAL_FOOTER):
            raise ValueError("terminal section requires a product-level update")
        return marker.end(), len(text)
    return marker.end(), markers[i + 1].start()


def replace(text: str, old: str, new: str, kind: str, name: str, allow_terminal: bool = False) -> str:
    old_markers = [m for m in SECTION.finditer(old) if m[1] == kind and m[2] == name]
    new_markers = [m for m in SECTION.finditer(new) if m[1] == kind and m[2] == name]
    current_markers = [m for m in SECTION.finditer(text) if m[1] == kind and m[2] == name]
    if not old_markers:
        if len(new_markers) != 1:
            raise ValueError(f"new section must exist exactly once: {kind}: {name}")
        markers = list(SECTION.finditer(new))
        index = next(i for i, marker in enumerate(markers) if marker.start() == new_markers[0].start())
        if index + 1 == len(markers):
            raise ValueError("new terminal section requires a product-level update")
        block = new[new_markers[0].start():markers[index+1].start()]
        if current_markers:
            start, end = section(text, kind, name)
            if len(current_markers) != 1 or text[current_markers[0].start():end] != block:
                raise ValueError("existing inserted section differs; refuse overwrite")
            return text
        anchor = markers[index+1]
        # The next section must exist in the previous product and target.
        # Insert exactly once before its marker, without changing its body.
        if sum(m[1] == anchor[1] and m[2] == anchor[2] for m in SECTION.finditer(old)) != 1:
            raise ValueError("new section anchor is missing/ambiguous in previous product")
        targets = [m for m in SECTION.finditer(text) if m[1] == anchor[1] and m[2] == anchor[2]]
        if len(targets) != 1: raise ValueError("new section anchor is missing/ambiguous")
        at = targets[0].start()
        return text[:at] + block + text[at:]
    if not new_markers:
        if not current_markers: return text
        start, end = section(text, kind, name)
        old_start, old_end = section(old, kind, name)
        if text[start:end] != old[old_start:old_end]:
            raise ValueError("removed section differs; refuse deletion")
        return text[:current_markers[0].start()] + text[end:]
    start, end = section(text, kind, name, allow_terminal)
    old_start, old_end = section(old, kind, name, allow_terminal)
    new_start, new_end = section(new, kind, name, allow_terminal)
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
    parser.add_argument("--internal", action="append", default=[])
    parser.add_argument("--declaration-delta", action="store_true",
                        help="Apply the exact whole declaration delta, including feature save/restore and repeated selectors")
    parser.add_argument("--runtime-public", action="append", default=[])
    parser.add_argument("--runtime-internal", action="append", default=[])
    parser.add_argument("--runtime-source", action="append", default=[])
    args = parser.parse_args()
    if not any((args.public,args.internal,args.source,args.runtime_public,args.runtime_internal,args.runtime_source)):
        parser.error("name at least one explicit source or public section")
    root = args.root.resolve()
    replacements: dict[Path, bytes] = {}
    for declarations in (False, True):
        if args.public or args.internal or args.source:
            leaf = "xrt_decl.h" if declarations else "xrt.h"
            old = (args.previous / leaf).read_text(encoding="utf-8")
            new = (args.current / leaf).read_text(encoding="utf-8")
            paths = (root / "release/include/xrt_decl.h", root / "release/include/xruntime_decl.h") if declarations else (
                root / "lib/xrt/xrt.h", root / "lib/xruntime/xruntime.h")
            for path in paths:
                text = path.read_text(encoding="utf-8")
                if declarations and args.declaration_delta:
                    text = guarded_delta(text, old, new, "declaration selection state")
                else:
                    for name in args.public: text = replace(text, old, new, "public", name)
                if not declarations:
                    for name in args.internal: text = replace(text, old, new, "internal", name)
                    for name in args.source: text = replace(text, old, new, "source", name)
                replacements[path] = text.encode("utf-8")
        if args.runtime_public or args.runtime_internal or args.runtime_source:
            leaf = "xruntime_decl.h" if declarations else "xruntime.h"
            old = (args.previous / "extlibs" / leaf).read_text(encoding="utf-8")
            new = (args.current / "extlibs" / leaf).read_text(encoding="utf-8")
            path = root / ("release/include/xruntime_decl.h" if declarations else "lib/xruntime/xruntime.h")
            text = replacements.get(path, path.read_bytes()).decode("utf-8")
            for name in args.runtime_public: text = replace(text, old, new, "public", name)
            if not declarations:
                for name in args.runtime_internal: text = replace(text, old, new, "internal", name)
                for name in args.runtime_source: text = replace(text, old, new, "source", name, True)
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
