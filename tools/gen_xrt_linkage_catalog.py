#!/usr/bin/env python3
"""Derive exact native link ownership from the shipped product and its manifests.

Source sections identify provenance, not sufficient ownership proof: overlay
modules may add definitions to another module's source. Select minimal feature
closures that actually provide each definition; keep equivalent alternatives.
No source-language names or generated C are inspected by the compiler at runtime.
"""
from __future__ import annotations

import argparse
import bisect
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


SECTION = re.compile(r"(?m)^/\* source: ([^*]+) \*/$")
DEFINITION = re.compile(r"\b(xrt\w+)\s*\)?\s*\([^;{}]*\)\s*\{")
LINE = re.compile(r'(?m)^#\s+\d+\s+"__xrt_catalog__/([^"\n]+)"[^\n]*$')
STRINGS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def minimize_roots(roots, provides):
    """Return a deterministic inclusion-minimal, actually witnessed root set.

    The compiler consumes adjacent rows as a conjunction. Some definitions
    have multiple positive feature guards, so no single root provides them.
    Every retained root must have a failed single-removal witness.
    """
    roots=tuple(roots)
    if not provides(roots):raise ValueError("combined roots do not provide the actual definition")
    width=max(1,len(roots)//2)
    while roots:
        changed=False
        for start in range(0,len(roots),width):
            smaller=roots[:start]+roots[start+width:]
            if provides(smaller):roots=smaller;changed=True;break
        if not changed:
            if width==1:break
            width=max(1,width//2)
        else:width=min(width,max(1,len(roots)))
    return tuple(sorted(roots))


def definitions(text: str, public: set[str]) -> dict[str, str]:
    markers = list(LINE.finditer(text))
    positions = [match.start() for match in markers]
    # Preserve positions while removing line directives and C string literals.
    clean = re.sub(r"(?m)^#[^\n]*", lambda m: " " * len(m[0]), text)
    clean = STRINGS.sub(lambda m: " " * len(m[0]), clean)
    braces = list(re.finditer(r"[{}]", clean))
    brace_index = 0
    depth = 0
    result = {}
    for match in DEFINITION.finditer(clean):
        while brace_index < len(braces) and braces[brace_index].start() < match.start():
            depth += 1 if braces[brace_index][0] == "{" else -1
            brace_index += 1
        if depth or match[1] not in public:
            continue
        index = bisect.bisect_right(positions, match.start()) - 1
        if index < 0:
            raise ValueError(f"definition has no implementation provenance: {match[1]}")
        if match[1] in result:
            raise ValueError(f"duplicate actual definition: {match[1]}")
        result[match[1]] = markers[index][1]
    return result


def generate(root: Path, native: Path, compiler: Path, cache: Path) -> tuple[str, dict]:
    header_path = root / "lib/xruntime/xruntime.h"
    core_path = root / "lib/xrt/xrt.h"
    features_path = root / "lib/xruntime/xruntime_features.h"
    header = "\n".join(path.read_text(encoding="utf-8")
                       for path in (features_path, core_path, header_path))
    declarations = set(re.findall(r"XL_XRT_SYMBOL\((\w+)\)",
                                  (root / "src/api/xl_xrt_symbols.inc").read_text(encoding="utf-8")))
    manifests = [native / "config/modules.json", native / "extlibs/xruntime/config/modules.json"]
    owners: dict[str, set[str | None]] = {}
    candidates: set[str] = set()
    for manifest in manifests:
        for module in json.loads(manifest.read_text(encoding="utf-8"))["modules"]:
            feature = module.get("feature")
            selection = feature.replace("_FEATURE_", "_MODULE_", 1) if feature else None
            if selection and not re.search(r"\b" + selection + r"\b", header):
                raise ValueError(f"manifest selection absent from shipped product: {selection}")
            if selection:
                candidates.add(selection)
            for source in module["sources"]:
                owners.setdefault(source, set()).add(selection)
    # Only diagnostic source markers differ from the shipped header. The real
    # preprocessor evaluates guards and expands allocator/scalar definitions.
    # A definition may omit XRT_API; public linkage comes from the declaration.
    probe = SECTION.sub(lambda m: f'#line 1 "__xrt_catalog__/{m[1]}"\n{m[0]}', header)
    fingerprint = digest(("preprocess-definition-schema-3\n" + probe + digest(compiler.read_bytes()) +
                          json.dumps(sorted(declarations))).encode())
    cache.mkdir(parents=True, exist_ok=True)

    def preprocess(selection):
        selections = ["XRT_MODULE_ALL", "XRUNTIME_MODULE_ALL"] if selection is None else (
            list(selection) if isinstance(selection,tuple) else ([selection] if selection else []))
        arguments = [str(compiler), "-E", "-dD", "-x", "c", "-DXRT_IMPLEMENTATION", "-DXRUNTIME_IMPLEMENTATION"]
        arguments += ["-D" + item for item in selections]
        arguments += ["-"]
        key = digest((fingerprint + json.dumps(arguments)).encode())
        target = cache / (key + ".json")
        if target.exists():
            saved = json.loads(target.read_text(encoding="utf-8"))
            if saved["Fingerprint"] != fingerprint or saved["Arguments"] != arguments:
                raise ValueError(f"invalid preprocessing cache: {target}")
            saved["Record"] = {"Path": str(target), "Sha256": digest(target.read_bytes())}
            return selection, saved
        completed = subprocess.run(arguments, input=probe, text=True, encoding="utf-8",
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if completed.returncode:
            raise ValueError(f"preprocessor rejected {selection}: {completed.stderr[:4000]}")
        exported = definitions(completed.stdout, declarations)
        features: set[str] = set()
        for directive in re.finditer(r"(?m)^#(define|undef)\s+((?:XRT|XRUNTIME)_FEATURE_\w+)\b", completed.stdout):
            if directive[1] == "define":
                features.add(directive[2])
            else:
                features.discard(directive[2])
        saved = {"Fingerprint": fingerprint, "Arguments": arguments, "Definitions": exported,
                 "Features": sorted(features), "OutputSha256": digest(completed.stdout.encode()), "Diagnostics": completed.stderr}
        target.write_text(json.dumps(saved, indent=2) + "\n", encoding="utf-8", newline="\n")
        saved["Record"] = {"Path": str(target), "Sha256": digest(target.read_bytes())}
        return selection, saved

    _, full_probe = preprocess(None)
    full = full_probe["Definitions"]
    if set(full) != declarations:
        raise ValueError(f"public/actual definition mismatch: missing={sorted(declarations-set(full))}, "
                         f"extra={sorted(set(full)-declarations)}")
    for symbol, source in full.items():
        if source not in owners:
            raise ValueError(f"unowned implementation: {symbol} in {source}")
    with ThreadPoolExecutor(max_workers=3) as pool:
        providers = dict(pool.map(preprocess, ["", *sorted(candidates)]))
    closures = {name: set(probe["Features"]) for name, probe in providers.items()}
    rows = []
    combined=[]
    for symbol, source in sorted(full.items()):
        available = [name for name, probe in providers.items() if probe["Definitions"].get(symbol) == source]
        selected = sorted(name for name in available if not any(
            closures[other] < closures[name] for other in available))
        if not selected:
            witnesses={}
            def provides(roots):
                _,proof=preprocess(tuple(sorted(roots)))
                witnesses[tuple(sorted(roots))]=proof["Record"]
                return proof["Definitions"].get(symbol)==source
            # Eliminate large/overlapping roots first. Never infer provision
            # from names, manifests alone or the all-features implementation.
            roots=sorted(candidates,key=lambda name:(-len(closures[name]),name))
            selected=list(minimize_roots(roots,provides))
            combined.append({"Symbol":symbol,"Source":source,"Roots":selected,
                "Probes":[{"Roots":list(roots),**proof} for roots,proof in sorted(witnesses.items())]})
        if "" in selected:
            selected = []  # The real no-selection product proves unconditional availability.
        rows.append({"Symbol": symbol, "Source": source, "Modules": selected})
    lines = ["/* Generated by tools/gen_xrt_linkage_catalog.py; do not edit. */",
             "/* Exact public link identities; NULL denotes the unconditional native core. */"]
    for row in rows:
        for selection in row["Modules"] or [None]:
            value = f'"{selection}"' if selection else "NULL"
            lines.append(f'XL6_ARTIFACT_NATIVE_SYMBOL("{row["Symbol"]}", {value})')
    inputs = [core_path, features_path, header_path, *manifests, compiler, Path(__file__).resolve(), root / "src/api/xl_xrt_symbols.inc"]
    report = {"Symbols": len(rows), "Roots": len(candidates), "Definitions": rows,
              "WholeCatalogVerified": True,
              "CombinedDefinitions":combined,
              "Inputs": [{"Path": str(p), "Sha256": digest(p.read_bytes())} for p in inputs],
              "Probes": [{"Selection": "ALL", **full_probe["Record"]}] +
                  [{"Selection": name or "CORE", **probe["Record"]} for name, probe in providers.items()],
              "Scope": "Exact declarations, expanded definitions and per-root availability on this compiler target; not cross-platform execution proof."}
    return "\n".join(lines) + "\n", report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--native-root", required=True, type=Path)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--cache", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.report.exists():
        raise ValueError(f"preserve existing evidence: {args.report}")
    compiler = Path(shutil.which(args.compiler) or args.compiler).resolve(strict=True)
    expected, report = generate(args.root.resolve(), args.native_root.resolve(), compiler, args.cache.resolve())
    output = args.root / "src/compile/xl6_artifact_native_symbols.inc"
    if args.check:
        if not output.exists() or output.read_text(encoding="utf-8") != expected:
            raise ValueError(f"native linkage catalog is stale: {output}")
    else:
        output.write_text(expected, encoding="utf-8", newline="\n")
    report["Catalog"] = {"Path": str(output.resolve()), "Sha256": digest(output.read_bytes())}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f'native linkage catalog: {report["Symbols"]} symbols, {report["Roots"]} candidate roots')
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
