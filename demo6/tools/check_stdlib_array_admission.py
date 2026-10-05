#!/usr/bin/env python3
"""Owned native Array element policies, not completion of language Array migration."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--nominal", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--shared", action="store_true", help="Test real product ABI; not pruned or runtime-instrumented")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which("gcc"), shutil.which("nm")
    objdump = shutil.which("objdump") if args.shared and os.name == "nt" else None
    if not cc or not nm or (args.shared and os.name == "nt" and not objdump) or out.exists() or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc/nm, fresh evidence and Linux for sanitizers")
    component = "nominal" if args.nominal else "reference"
    sources = ["test/cases/stdlib_array_admission_native_host.c", "test/support/array_value_native.c",
               "src/runtime/xl6_sequence.c", "src/runtime/xl6_array.c", "src/runtime/xl6_array_iterator.c",
               "src/runtime/xl6_array_erased.c", "src/runtime/xl6_array_reference_value.c",
               "src/runtime/xl6_ownership_plan.c",
               f"src/runtime/xl6_array_{component}_admission.c",
               "src/runtime/xl6_nominal_admission.c" if args.nominal else "src/runtime/xl6_value_admission.c"]
    files = [Path(__file__), Path(cc), Path(nm), Path(sys.executable), root / "lib/xrt/xrt.h", root / "lib/xruntime/xruntime.h"]
    files += [root / name for name in sources]
    product = root / ("release/xl.dll" if os.name == "nt" else "release/libxl.so")
    if args.shared:
        files += [product]
        if objdump:
            files += [Path(objdump)]
    for directory in ("src/runtime", "release/include"):
        files += sorted((root / directory).rglob("*.h"))
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    if args.shared and os.name == "nt":
        shutil.copy2(product, out / "xl.dll")
    rows, sweeps = [], []
    report = {"Complete": False, "InputsUnchanged": False, "Nominal": args.nominal, "Sanitized": args.sanitize,
              "SharedRuntime": args.shared, "RuntimeInstrumented": args.sanitize and not args.shared,
              "Scope": ("Actual shared product ABI; --sanitize instruments host only, not runtime; not a pruned closure"
                        if args.shared else __doc__),
              "Platform": sys.platform, "Inputs": inputs, "Rows": rows, "Sweeps": sweeps}
    env = dict(os.environ)
    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I.", "-Irelease", "-Irelease/include"]
    links = [] if os.name == "nt" else ["-pthread", "-lm"]
    compiled_sources = sources
    if args.shared:
        compiled_sources = [sources[0]]
        links = [str(product)] if os.name == "nt" else ["-Lrelease", "-lxl", "-Wl,-rpath," + str(root / "release"), *links]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.nominal:
        strict += ["-DXL6_TEST_ARRAY_NOMINAL"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
        env.update(ASAN_OPTIONS="detect_stack_use_after_return=1:detect_leaks=1:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str]) -> str:
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=60)
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode, "Complete": result.returncode == 0})
        if result.returncode:
            raise RuntimeError(f"{name}: {data[-4000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    try:
        for level in (0, 2):
            lane = f"O{level}"
            binary = out / (lane + (".exe" if os.name == "nt" else ""))
            run(lane + ".build", [cc, *strict, f"-O{level}", *compiled_sources, *links, "-o", str(binary)])
            # Linux release products are stripped; their public ABI lives in
            # the dynamic symbol table, not the removed static symbol table.
            # Stripped release DLLs retain PE exports, not debug symbols.
            symbol_command = ([objdump, "-p", str(product)] if objdump else
                [nm, *(["-D"] if args.shared else []), "--defined-only", str(product if args.shared else binary)])
            symbols = run(lane + ".symbols", symbol_command)
            names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            expected = "xl6ArrayNominalCreateV2" if args.nominal else "xl6ArrayReferenceCreateV2"
            absent = {"xl6ArrayReferenceCreateV2", "xl6ValueAdmitsV1"} if args.nominal else {
                "xl6ArrayNominalCreateV2", "xl6ValueAdmitsNominalV1"}
            assert expected in names, f"Missing factory export: {expected}"
            assert {"xl6SequenceAdmissionTakeV3", "xl6SequenceAdmitSlotV1", "xl6SequenceAdmitValueV1"} <= names, "Missing resident admission ABI"
            if not args.shared:
                assert not absent & names
                assert not {"xl6ArrayFromValueV1", "xl6ArrayToValueV1", "xl6StackCreateV1"} & names
                assert not any(re.match(r"xl6(?:Log|Regex|Process)[A-Z]|xrtTypedStack", name) for name in names)
            run(lane + ".vectors", [str(binary)])
            operations = ["create", "clone", "erased-clone", "cursor"]
            if not args.nominal:
                operations += ["resize", "resize-spare"]
            for operation in operations:
                for point in range(513):
                    log = run(f"{lane}.{operation}.{point}", [str(binary), operation, str(point)])
                    match = re.search(r"hit=([01]) balanced", log)
                    if not match:
                        raise RuntimeError("Missing allocation/ownership outcome")
                    if match[1] == "0":
                        sweeps.append({"Lane": lane, "Operation": operation, "FirstNoHit": point})
                        break
                else:
                    raise RuntimeError("Incomplete allocation prefix")
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        report["Complete"] = report["InputsUnchanged"]
        if not report["Complete"]:
            raise RuntimeError("Acceptance inputs changed")
    except Exception as error:
        report["Error"] = str(error)
        print(error, file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Array admission: {len(rows)} checks, {len(sweeps)} closed allocation prefixes, "
          f"{sum(row['FirstNoHit']+1 for row in sweeps)} fault positions, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
