#!/usr/bin/env python3
"""Exact native scalar/text Array admission; language Array migration still pending."""
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
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--shared", action="store_true", help="Actual shared product ABI; sanitizer instruments host only")
    parser.add_argument("--projections", action="store_true", help="Exact owned boxed identity, snapshots, V2 metadata and rollback")
    parser.add_argument("--iterator", action="store_true", help="Native Array snapshots through the real language ValueIterator ABI")
    parser.add_argument("--reads", action="store_true", help="Owned strict/safe/keyed native and wire index reads")
    parser.add_argument("--imports", action="store_true", help="Profiled native import; exact scalar/text codecs and rollback")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which("gcc"), shutil.which("nm")
    objdump = shutil.which("objdump") if args.shared and os.name == "nt" else None
    if not cc or not nm or (args.shared and os.name == "nt" and not objdump) or out.exists() or (args.sanitize and os.name == "nt") or sum((args.iterator,args.projections,args.reads,args.imports))>1:
        parser.error("Require gcc/nm, fresh evidence and Linux for sanitizers")
    host = "test/cases/stdlib_array_projection_identity_native_host.c" if args.projections else "test/cases/stdlib_array_primitive_admission_native_host.c"
    if args.iterator: host="test/cases/stdlib_array_value_iterator_native_host.c"
    if args.reads: host="test/cases/stdlib_array_value_read_native_host.c"
    if args.imports: host="test/cases/stdlib_array_primitive_import_native_host.c"
    common = ["test/support/array_value_native.c", "src/runtime/xl6_sequence.c",
              "src/runtime/xl6_array.c", "src/runtime/xl6_array_iterator.c", "src/runtime/xl6_array_erased.c",
              "src/runtime/xl6_array_reference_value.c", "src/runtime/xl6_ownership_plan.c",
              "src/runtime/xl6_array_primitive_admission.c", "src/runtime/xl6_value_admission.c"]
    if args.projections:
        common += ["src/runtime/xl6_array_value.c"]
    if args.iterator:
        common += ["src/runtime/xl6_value_iterator.c","src/runtime/xl6_array_value_iterator.c"]
    if args.reads:
        common += ["src/runtime/xl6_array_value_read.c"]
    if args.imports:
        common += ["src/runtime/xl6_array_import.c","src/runtime/xl6_array_input.c"]
    common.append('src/api/xl_foreign_error.c')
    graph_sources=["test/cases/stdlib_array_value_iterator_graph_host.c",
        "test/support/collection_count_native.c","src/runtime/xl6_array_reference_admission.c",
        "src/runtime/xl6_array_default.c"] if args.iterator else []
    profiles = [[], *[[f"src/runtime/xl6_nullable_{name}.c", f"src/runtime/xl6_nullable_{name}_value.c"]
        for name in ("bool", "i64", "u64", "f32", "f64", "time", "char")],
        ["src/runtime/xl6_char_type.c", "src/runtime/xl6_char_value.c"],
        ["src/runtime/xl6_string_type.c", "src/runtime/xl6_string_value.c"],
        ["src/runtime/xl6_nullable_string.c"]]
    product = root / ("release/xl.dll" if os.name == "nt" else "release/libxl.so")
    files = [Path(__file__), Path(cc), Path(nm), Path(sys.executable), root / host,
             root / "lib/xrt/xrt.h", root / "lib/xruntime/xruntime.h"]
    files.append(root / 'src/api/xl_foreign_error.h')
    files += [root / name for name in common + [p for profile in profiles for p in profile]]
    files += [root / name for name in graph_sources]
    if args.projections or args.iterator or args.reads or args.imports:
        files += [root / "test/cases/stdlib_array_primitive_admission_native_host.c",root / "test/cases/stdlib_sequence_retired_admission_abi.c"]
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
    report = {"Complete": False, "InputsUnchanged": False, "Platform": sys.platform,
              "Scope": __doc__, "SharedRuntime": args.shared, "Sanitized": args.sanitize,
              "RuntimeInstrumented": args.sanitize and not args.shared,
              "Inputs": inputs, "Rows": rows, "Sweeps": sweeps, "Profiles": len(profiles)}
    report["Projections"] = args.projections
    report["Iterator"] = args.iterator
    report["OwnedReads"] = args.reads
    report["Imports"] = args.imports
    if args.imports: report["Scope"]="Profiled native Array import, same-layout unboxed copy and exact scalar/text conversion; not source carrier cutover"
    if args.reads: report["Scope"]="Owned strict/safe/keyed native Array projections and wire retain; not whole language carrier cutover"
    if args.iterator: report["Scope"]="Native Array scalar/text snapshots through resident ValueIterator V2; not whole language carrier cutover"
    env = dict(os.environ)
    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I.", "-Irelease", "-Irelease/include"]
    links = [] if os.name == "nt" else ["-pthread", "-lm"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
        env.update(ASAN_OPTIONS="detect_stack_use_after_return=1:detect_leaks=1:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str], reject: bool = False) -> str:
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=60)
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        complete = result.returncode != 0 if reject else result.returncode == 0
        rows.append({"Name": name, "Command": command, "Exit": result.returncode, "Complete": complete})
        if not complete:
            raise RuntimeError(f"{name}: {data[-4000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    try:
        for level in (0, 2):
            lane = f"O{level}"
            objects = {}
            # Compile each actual resident TU once in a frozen lane. Link only
            # the selected profile objects; sharing build work must not turn
            # into an all-profile runtime dependency or relaxed compiler flags.
            if not args.shared:
                for source in sorted(set(common + [p for profile in profiles for p in profile] + graph_sources[1:])):
                    obj = out / (lane + "." + Path(source).stem + ".o")
                    run(lane + ".compile." + Path(source).stem,
                        [cc, *strict, f"-O{level}", "-c", source, "-o", str(obj)])
                    objects[source] = str(obj)
            for profile, sources in enumerate(profiles):
                name = f"{lane}.profile{profile}"
                binary = out / (name + (".exe" if os.name == "nt" else ""))
                selected = [objects[source] for source in common + sources] if not args.shared else []
                linked = links
                if args.shared:
                    linked = [str(product)] if os.name == "nt" else ["-Lrelease", "-lxl", "-Wl,-rpath," + str(root / "release"), *links]
                run(name + ".build", [cc, *strict, f"-O{level}", f"-DXL6_TEST_PRIMITIVE_PROFILE={profile}",
                    host, *selected, *linked, "-o", str(binary)])
                # Release products are stripped. Check PE exports on Windows
                # and ELF dynamic symbols on Linux, not absent debug symbols.
                symbol_command = ([objdump, "-p", str(product)] if objdump else
                    [nm, *(["-D"] if args.shared else []), "--defined-only", str(product if args.shared else binary)])
                symbols = run(name + ".symbols", symbol_command)
                names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
                assert {"xl6SequencePrimitiveCreateV1", "xl6ValueAdmitsV1", "xl6SequenceAdmissionTakeV3"} <= names
                if not args.shared:
                    factories = {n for n in names if re.match(r"xl6(?:Nullable.*|String|Char)TypeV1$", n)}
                    expected = set()
                    if 1 <= profile <= 7:
                        family = ("Bool", "I64", "U64", "F32", "F64", "Time", "Char")[profile-1]
                        expected = {f"xl6Nullable{family}TypeV1"}
                    elif profile == 8:
                        expected = {"xl6CharTypeV1"}
                    elif profile == 9:
                        expected = {"xl6StringTypeV1"}
                    elif profile == 10:
                        expected = {"xl6NullableStringTypeV1"}
                    assert factories == expected, (factories, expected)
                    forbidden = {"xl6SequenceNominalCreateV1",
                        "xl6SequenceReferenceCreateV1", "xl6StackCreateV1"} & names
                    if not args.projections:
                        forbidden |= {"xl6ArrayFromValueV1", "xl6ArrayToValueV1"} & names
                    assert not forbidden
                    assert not {"xl6SequenceAdmissionCopyV1","xl6SequenceAdmissionTakeV1","xl6SequenceAdmissionDropV1"} & names
                    assert not any(re.match(r"xl6(?:Log|Regex|Process)[A-Z]|xrtTypedStack", n) for n in names)
                run(name + ".vectors", [str(binary)])
                if args.projections and profile == 0:
                    retired=out / (lane+".retired-admission.o")
                    run(lane+".retired-admission.compile",[cc,*strict,"-c",
                        "test/cases/stdlib_sequence_retired_admission_abi.c","-o",str(retired)])
                    rejected=run(lane+".retired-admission.link",[cc,*strict,str(retired),*selected,*linked,
                        "-o",str(out / (lane+".retired-admission"))],reject=True)
                    assert "undefined reference" in rejected
                    for operation in ("Copy","Take","Drop"):
                        assert f"xl6SequenceAdmission{operation}V1" in rejected
                operations = ("create", "clone", "erased-clone", "cursor", "read", "take", "push", "set", "insert", "resize") + (("snapshot",) if args.projections else ())
                if args.iterator: operations=("create","first","second")
                if args.reads: operations=("read","safe","keyed")
                if args.imports: operations=("wire","native","fallback")
                for operation in operations:
                    for point in range(513):
                        log = run(f"{name}.{operation}.{point}", [str(binary), operation, str(point)])
                        hits = re.findall(r"hit=([01]) balanced", log)
                        if not hits:
                            raise RuntimeError("Missing allocation/ownership outcome")
                        if all(hit=="0" for hit in hits):
                            sweeps.append({"Lane": lane, "Profile": profile, "Operation": operation, "FirstNoHit": point})
                            break
                    else:
                        raise RuntimeError("Incomplete logical allocation prefix")
            if args.iterator:
                binary=out/(lane+'.graph'+('.exe' if os.name=='nt' else ''))
                selected=[objects[source] for source in common+graph_sources[1:] if source!='test/support/array_value_native.c'] if not args.shared else []
                run(lane+'.graph.build',[cc,*strict,f'-O{level}',graph_sources[0],*selected,*linked,'-o',str(binary)])
                assert '75 complete transactions, 2400 OOM budgets' in run(lane+'.graph.run',[str(binary)])
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        report["Complete"] = report["InputsUnchanged"]
        if not report["Complete"]:
            raise RuntimeError("Acceptance inputs changed")
    except Exception as error:
        report["Error"] = f"{type(error).__name__}: {error}"
        print(error, file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Primitive Array: {len(rows)} checks, {len(sweeps)} closed prefixes, "
          f"{sum(row['FirstNoHit']+1 for row in sweeps)} positions, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
