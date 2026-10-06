#!/usr/bin/env python3
"""Verify the native Array foundation; does NOT claim language IR migration."""
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
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--reuse-children", type=Path, help="Reuse independently complete, SHA-unchanged child evidence; never a parent verdict")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    if out.exists():
        parser.error("Use a fresh evidence directory; preserve previous results")
    if args.sanitize and os.name == "nt":
        parser.error("This gate supports ASan/UBSan on Linux only")
    cc, nm = shutil.which(args.cc), shutil.which(args.nm)
    if not cc or not nm:
        parser.error("Native C compiler and nm are required")
    out.mkdir(parents=True)
    files = [Path(__file__), Path(cc), Path(nm), Path(sys.executable)]
    projection_sources = ["test/cases/stdlib_array_value_native_host.c", "test/support/array_value_native.c",
        "src/runtime/xl6_sequence.c", "src/runtime/xl6_array.c", "src/runtime/xl6_array_value.c",
        "src/runtime/xl6_array_reference_value.c",
        "src/runtime/xl6_string_type.c", "src/runtime/xl6_string_value.c", "src/runtime/xl6_nullable_string.c",
        "src/runtime/xl6_char_type.c", "src/runtime/xl6_char_value.c",
        "src/runtime/xl6_ownership_plan.c"]
    for family in ("bool", "i64", "u64", "f32", "f64", "time", "char"):
        projection_sources += [f"src/runtime/xl6_nullable_{family}.c", f"src/runtime/xl6_nullable_{family}_value.c"]
    erased_sources = [name for name in projection_sources if name not in (
        "test/cases/stdlib_array_value_native_host.c", "src/runtime/xl6_array_value.c")]
    erased_sources += ["test/cases/stdlib_array_erased_native_host.c", "src/runtime/xl6_array_erased.c",
                       "src/runtime/xl6_stack.c", "src/runtime/xl6_array_iterator.c", "src/api/xl_foreign_error.c"]
    files.append(root / 'src/api/xl_foreign_error.h')
    files += [root / name for name in erased_sources]
    files += [root / name for name in projection_sources]
    files += [root / name for name in (
        "test/cases/stdlib_array_native_host.c", "test/cases/stdlib_array_iterator_native_host.c", "test/support/array_native.c",
        "test/support/stack_native.c", "src/runtime/xl6_sequence.c",
        "src/runtime/xl6_array.c", "src/runtime/xl6_stack.c", "src/runtime/xl6_nullable_string.c",
        "src/runtime/xl6_array_iterator.c",
        "src/runtime/xl6_ownership_plan.c", "lib/xrt/xrt.h", "lib/xruntime/xruntime.h")]
    files += [root / name for name in (
        "tools/check_stdlib_array_admission.py", "test/cases/stdlib_array_admission_native_host.c",
        "src/runtime/xl6_array_reference_admission.c", "src/runtime/xl6_array_nominal_admission.c",
        "src/runtime/xl6_value_admission.c", "src/runtime/xl6_nominal_admission.c")]
    files += [root / name for name in ("tools/check_stdlib_array_defaults.py",
        "test/cases/stdlib_array_default_native_host.c", "test/support/array_default_native.c",
        "src/runtime/xl6_array_default.c")]
    files += [root / name for name in ("tools/check_stdlib_array_primitive_admission.py",
        "tools/check_stdlib_array_methods.py","test/cases/stdlib_array_methods_native_host.c",
        "test/cases/stdlib_array_extract_native_host.c","test/cases/stdlib_array_extract_core_host.c",
        "test/cases/stdlib_array_extract_header.c",
        "test/cases/stdlib_array_resident_default_host.c","test/cases/stdlib_array_default_header.c",
        "test/cases/stdlib_array_methods_header.c","src/runtime/xl6_array_methods.c",
        "src/runtime/xl6_array_method_insert.c","src/runtime/xl6_array_method_find.c",
        "tools/check_stdlib_array_import.py","test/cases/stdlib_array_import_native_host.c",
        "test/cases/stdlib_array_import_header.c","test/cases/stdlib_array_primitive_import_native_host.c",
        "test/support/array_import_native.c","src/runtime/xl6_array_import.c",
        "test/cases/stdlib_array_import_chain_native_host.c","src/runtime/xl6_array_import_chain.c",
        "test/cases/stdlib_array_primitive_admission_native_host.c",
        "test/cases/stdlib_array_projection_identity_native_host.c",
        "test/cases/stdlib_sequence_retired_admission_abi.c",
        "test/cases/stdlib_array_value_read_native_host.c","src/runtime/xl6_array_value_read.c",
        "test/cases/stdlib_array_value_iterator_native_host.c","src/runtime/xl6_array_value_iterator.c",
        "test/cases/stdlib_array_value_iterator_graph_host.c","test/support/collection_count_native.c",
        "src/runtime/xl6_value_iterator.c",
        "src/runtime/xl6_array_primitive_admission.c")]
    files += [root / name for name in ("tools/check_stdlib_array_c_scalar.py",
        "test/cases/stdlib_array_c_scalar_native_host.c", "src/runtime/xl6_array_c_scalar_admission.c")]
    files += [root / name for name in ("tools/check_stdlib_stack_batch.py",
        "test/cases/stdlib_stack_batch_native_host.c", "src/runtime/xl6_stack_batch.c")]
    files += [root / name for name in ("tools/check_stdlib_array_sequence_admission.py",
        "test/cases/stdlib_array_sequence_admission_native_host.c", "src/runtime/xl6_array_sequence_admission.c")]
    files += [root / name for name in ("tools/check_stdlib_array_nullable_c_scalar.py",
        "test/cases/stdlib_array_nullable_c_scalar_native_host.c",
        "test/cases/stdlib_array_nullable_c_scalar_storage_host.c",
        "src/runtime/xl6_array_nullable_c_scalar_admission.c")]
    for profile in ("i8", "u8", "i16", "u16", "i32", "u32"):
        files += [root / f"src/runtime/xl6_nullable_native_{profile}{suffix}.c" for suffix in ("", "_value")]
    files += [root / name for name in ("tools/check_stdlib_array_numeric_enum.py",
        "test/cases/stdlib_array_numeric_enum_native_host.c",
        "test/cases/stdlib_array_numeric_enum_storage_host.c",
        "src/runtime/xl6_array_numeric_enum_admission.c")]
    files += [root / name for name in ("tools/check_stdlib_array_callable.py",
        "test/cases/stdlib_array_callable_native_host.c", "test/cases/stdlib_array_callable_storage_host.c",
        "test/support/array_callable_native.c",
        "src/runtime/xl6_array_callable_admission.c", "src/runtime/xl6_closure.c",
        "src/runtime/xl6_cell.c", "src/runtime/xl6_code_lease.c")]
    # Freeze project header dependencies, including the real resident layouts.
    files += [root / name for name in ('tools/check_stdlib_sequence_publication.py',
        'test/cases/stdlib_sequence_publication_native_host.c')]
    files += [root / name for name in ('tools/check_stdlib_sequence_factories.py',
        'test/cases/stdlib_sequence_factory_only_host.c','test/support/sequence_factory_native.c')]
    files += [root / name for name in ('tools/check_stdlib_array_scalar_read.py',
        'test/cases/stdlib_array_scalar_read_native_host.c', 'test/cases/stdlib_array_scalar_read_header.c',
        'src/runtime/xl6_array_scalar_read.c')]
    files += [root / name for name in ('src/runtime/xl6_array_string_read.c',
        'test/cases/stdlib_array_value_boundaries_header.c','test/cases/stdlib_array_value_boundaries_native_host.c')]
    files += [root / name for name in ('tools/check_stdlib_collection_count.py',
        'test/cases/stdlib_collection_count_native_host.c','test/cases/stdlib_collection_count_header.c','test/support/collection_count_native.c',
        'src/runtime/xl6_collection_count.c')]
    files += [root / name for name in ('tools/check_stdlib_array_shuffle.py',
        'test/cases/stdlib_array_shuffle_native_host.c','test/support/array_shuffle_native.c',
        'src/runtime/xl6_array_rng_shuffle.c','src/runtime/xl6_array_rand_shuffle.c')]
    files += [root / name for name in ('tools/check_stdlib_array_future.py',
        'test/cases/stdlib_array_future_native_host.c','test/support/array_future_native.c',
        'src/runtime/xl6_array_future_admission.c','test/cases/stdlib_sequence_retired_admission_abi.c')]
    files += [root / name for name in ('tools/check_stdlib_array_trivial_extract.py',
        'test/cases/stdlib_array_extract_trivial_host.c','src/runtime/xl6_array_extract_trivial.c')]
    for directory in ("src/runtime", "src/api", "release/include"):
        files += sorted((root / directory).rglob("*.h"))
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    rows: list[dict[str, object]] = []
    sweeps: list[dict[str, object]] = []
    report = {"Scope": "native Array foundation, not language Array migration",
              "Platform": sys.platform, "Sanitized": args.sanitize, "Inputs": inputs,
              "Rows": rows, "Sweeps": sweeps, "Complete": False, "InputsUnchanged": False}
    report["AdmissionReports"] = []
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str], timeout: int = 45) -> str:
        if args.reuse_children and len(command)>2 and command[0]==sys.executable and "--out" in command:
            destination=Path(command[command.index("--out")+1]).resolve()
            previous=args.reuse_children.resolve()/destination.name
            proof=previous/"results.json"
            if proof.is_file() and not destination.exists() and previous!=destination:
                evidence=json.loads(proof.read_text(encoding="utf-8"))
                matching=evidence.get("Complete") and evidence.get("InputsUnchanged") and \
                    evidence.get("Sanitized")==args.sanitize
                for flag,field in (("--projections","Projections"),("--iterator","Iterator"),("--reads","OwnedReads"),("--imports","Imports")):
                    if flag in command: matching=matching and evidence.get(field) is True
                if matching and evidence.get("Inputs") and all(Path(row["Path"]).is_file() and
                    digest(Path(row["Path"]))==row["Sha256"].lower() for row in evidence["Inputs"]):
                    # Preserve the original report, commands and paths: this is
                    # reuse of an unchanged verdict, NOT a claimed fresh run.
                    inputs.append({"Path":str(proof),"Sha256":digest(proof)})
                    inputs.extend(evidence["Inputs"])
                    shutil.copytree(previous,destination)
                    rows.append({"Name":name,"Command":command,"Exit":0,"Complete":True,
                        "ReusedFrom":str(proof),"FreshExecution":False})
                    (out/(name+".log")).write_text("Reused SHA-unchanged complete child: "+str(proof)+"\n",encoding="utf-8")
                    return ""
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode,
                     "Complete": result.returncode == 0})
        if result.returncode:
            raise RuntimeError(f"{name}: exit {result.returncode}\n{data[-4000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I.", "-Irelease", "-Irelease/include"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    operations = ("create", "reserve", "push", "set", "insert", "read", "take", "clone",
                  "trim", "resize", "append", "self-append", "remove", "swap", "reverse", "find",
                  "clear", "equals", "graph-create", "graph-refresh", "graph-commit",
                  "nullable-create", "nullable-push", "nullable-set", "nullable-insert", "nullable-read",
                  "nullable-clone", "nullable-trim", "nullable-append", "nullable-self-append",
                  "cursor-create", "cursor-read", "nullable-cursor-create", "nullable-cursor-read",
                  "cursor-graph-create", "cursor-graph-refresh", "cursor-graph-commit")
    try:
        for level in (0, 2):
            lane = f"O{level}.erased"
            binary = out / (lane + (".exe" if os.name == "nt" else ""))
            links = [] if os.name == "nt" else ["-pthread", "-lm"]
            run(lane + ".build", [cc, *strict, f"-O{level}", "-DXL6_TEST_ERASED_STACK", *erased_sources, *links, "-o", str(binary)])
            symbols = run(lane + ".symbols", [nm, "--defined-only", str(binary)])
            names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            assert {"xl6ArrayEraseV1", "xl6ArrayErasedReadV1", "xl6ArrayReferenceConverterV1",
                    "xl6SequenceBeginRoleV1", "xrtTypedArrayTakeValue"} <= names, "Missing actual alias/projection closure"
            assert not {"xl6ArrayFromValueV1", "xl6ArrayToValueV1"} & names, "Reference erasure pulled snapshot facade"
            run(lane + ".vectors", [str(binary)])
            for operation in ("erased-read", "erased-take", "erased-set", "erased-push", "erased-insert", "erased-clone"):
                for point in range(513):
                    log = run(f"{lane}.{operation}.{point}", [str(binary), operation, str(point)])
                    match = re.search(r"hit=([01]) balanced", log)
                    if not match:
                        raise RuntimeError("Missing balanced erased fault outcome")
                    if match[1] == "0":
                        sweeps.append({"Lane": lane, "Operation": operation, "FirstNoHit": point})
                        break
                else:
                    raise RuntimeError(f"Incomplete logical allocation prefix: {lane}/{operation}")
            lane = f"O{level}.cursor"
            binary = out / (lane + (".exe" if os.name == "nt" else ""))
            sources = ["test/cases/stdlib_array_iterator_native_host.c", "test/support/array_native.c",
                       "src/runtime/xl6_sequence.c", "src/runtime/xl6_array_iterator.c"]
            links = [] if os.name == "nt" else ["-pthread", "-lm"]
            run(lane + ".build", [cc, *strict, f"-O{level}", *sources, *links, "-o", str(binary)])
            symbols = run(lane + ".symbols", [nm, "--defined-only", str(binary)])
            names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            assert "xl6ArrayIteratorCreateV1" in names and "xl6SequenceWrapV1" in names, "Missing snapshot-only closure"
            assert "xrtTypedArrayCreate" in names, "Missing selected typed storage"
            assert not any(re.match(r"xl6(?:Array(?!Iterator)|Stack)[A-Z]|xrtTypedStack[A-Z]", n) for n in names), "Snapshot-only closure pulled Array/Stack facade"
            assert not {"xl6NullableStringTypeV1", "xl6NullableStringEqualV1", "xl6NullableStringConverterV1",
                        "xl6CharTypeV1", "xl6CharConverterV1", "xl6StringTypeV1", "xl6StringConverterV1", "xl6ArrayFromValueV1", "xl6ArrayToValueV1",
                        "xl6ArrayEraseV1", "xl6ArrayReferenceConverterV1"} & names, "Scalar snapshot pulled text/projection provider"
            run(lane + ".vectors", [str(binary)])
            for mixed in (False, True):
                lane = f"O{level}.{'mixed' if mixed else 'array'}"
                binary = out / (lane + (".exe" if os.name == "nt" else ""))
                sources = ["test/cases/stdlib_array_native_host.c",
                           "test/support/stack_native.c" if mixed else "test/support/array_native.c",
                           "src/runtime/xl6_sequence.c", "src/runtime/xl6_array.c", "src/runtime/xl6_array_iterator.c", "src/runtime/xl6_nullable_string.c", "src/runtime/xl6_ownership_plan.c"]
                defines = ["-DXL6_TEST_MIXED_STACK"] if mixed else []
                if mixed:
                    sources += ["src/runtime/xl6_stack.c"]
                links = [] if os.name == "nt" else ["-pthread", "-lm"]
                run(lane + ".build", [cc, *strict, f"-O{level}", *defines, *sources, *links, "-o", str(binary)])
                symbols = run(lane + ".symbols", [nm, "--defined-only", str(binary)])
                names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
                assert "xl6ArrayCreateV1" in names and "xl6SequenceOwnershipAdapterV1" in names, "Missing Array/Sequence resident owner"
                assert "xl6ArrayIteratorCreateV1" in names and "xl6ArrayIteratorReadV1" in names, "Missing actual native snapshot cursor"
                assert "xrtTypedArrayCreate" in names, "Missing actual typed Array implementation"
                assert ("xl6StackCreateV1" in names) == mixed and ("xrtTypedStackCreate" in names) == mixed, "Incorrect Stack closure"
                assert "xl6StackOwnershipAdapterV1" not in names, "Retired Stack ownership adapter remains"
                unrelated = sorted(n for n in names if re.match(r"(?:xl6(?:Regex|Log|Process)|xrt(?:Regex|Logger|Process))[A-Z]", n))
                assert not unrelated, f"Unrelated native closure: {unrelated}"
                run(lane + ".vectors", [str(binary)])
                # Duplicate the logical-allocation prefix at O0/O2. Mixed graph
                # vectors above share the exact same physical owner admission.
                if mixed:
                    continue
                for operation in operations:
                    for point in range(513):
                        log = run(f"{lane}.{operation}.{point}", [str(binary), operation, str(point)])
                        match = re.search(r"hit=([01]) balanced", log)
                        if not match:
                            raise RuntimeError("Missing balanced fault outcome")
                        if match[1] == "0":
                            sweeps.append({"Lane": lane, "Operation": operation, "FirstNoHit": point})
                            break
                    else:
                        raise RuntimeError(f"Incomplete logical allocation prefix: {lane}/{operation}")
            lane = f"O{level}.projection"
            binary = out / (lane + (".exe" if os.name == "nt" else ""))
            links = [] if os.name == "nt" else ["-pthread", "-lm"]
            run(lane + ".build", [cc, *strict, f"-O{level}", *projection_sources, *links, "-o", str(binary)])
            symbols = run(lane + ".symbols", [nm, "--defined-only", str(binary)])
            names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            assert {"xl6ArrayFromValueV1", "xl6ArrayToValueV1", "xl6StringTypeV1", "xl6StringConverterV1",
                    "xl6CharTypeV1", "xl6CharConverterV1",
                    "xrtTypedArrayFromValue", "xrtTypedArrayToValue"} <= names, "Missing actual projection closure"
            assert not any(re.match(r"(?:xl6(?:Stack|Regex|Log|Process)|xrt(?:TypedStack|Regex|Logger|Process))[A-Z]", n) for n in names), "Projection pulled unrelated implementation"
            run(lane + ".vectors", [str(binary)])
            projection_operations = ["string-init", "string-copy", "string-resize", "string-from", "string-to",
                                     "ref-from", "ref-to", "char-from", "char-to", "graph-create", "graph-refresh", "graph-commit"]
            projection_operations += [f"profile-{family}-{direction}" for family in
                ("bool", "i64", "u64", "f32", "f64", "time", "char") for direction in ("from", "to")]
            for operation in projection_operations:
                for point in range(513):
                    log = run(f"{lane}.{operation}.{point}", [str(binary), operation, str(point)])
                    match = re.search(r"hit=([01]) balanced", log)
                    if not match:
                        raise RuntimeError("Missing balanced projection fault outcome")
                    if match[1] == "0":
                        sweeps.append({"Lane": lane, "Operation": operation, "FirstNoHit": point})
                        break
                else:
                    raise RuntimeError(f"Incomplete projection allocation prefix: {lane}/{operation}")
        # Keep owned write-time element policies on the actual regular Array
        # regression path (and hence the Stack/Sequence ownership parent).
        # Child inputs/results remain independently frozen and inspectable.
        for nominal in (False, True):
            child = out / ("admission-nominal" if nominal else "admission-reference")
            command = [sys.executable, "tools/check_stdlib_array_admission.py", "--out", str(child)]
            if nominal:
                command += ["--nominal"]
            if args.sanitize:
                command += ["--sanitize"]
            run(child.name, command, timeout=120)
            admitted = json.loads((child / "results.json").read_text(encoding="utf-8"))
            if not admitted["Complete"] or not admitted["InputsUnchanged"]:
                raise RuntimeError("Incomplete owned Array admission evidence")
            report["AdmissionReports"].append({"Directory": str(child), "Nominal": nominal,
                "Checks": len(admitted["Rows"]), "ClosedPrefixes": len(admitted["Sweeps"]),
                "AllocationPositions": sum(row["FirstNoHit"]+1 for row in admitted["Sweeps"]),
                "Complete": True, "InputsUnchanged": True})
        child = out / "wire-defaults"
        command = [sys.executable, "tools/check_stdlib_array_defaults.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=120)
        defaults = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not defaults["Complete"] or not defaults["InputsUnchanged"]:
            raise RuntimeError("Incomplete wire Array default evidence")
        report["DefaultReport"] = {"Directory": str(child), "Checks": len(defaults["Rows"]),
            "ClosedPrefixes": len(defaults["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in defaults["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "admission-primitive"
        command = [sys.executable, "tools/check_stdlib_array_primitive_admission.py", "--projections", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=180)
        primitive = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not primitive["Complete"] or not primitive["InputsUnchanged"] or not primitive["Projections"]:
            raise RuntimeError("Incomplete scalar/text owned Array admission evidence")
        report["PrimitiveAdmissionReport"] = {"Directory": str(child), "Checks": len(primitive["Rows"]),
            "Projections": True,
            "Profiles": primitive["Profiles"], "ClosedPrefixes": len(primitive["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in primitive["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        for mode in ("iterator","reads","imports"):
            child=out / ("native-"+mode)
            command=[sys.executable,"tools/check_stdlib_array_primitive_admission.py","--"+mode,"--out",str(child)]
            if args.sanitize: command += ["--sanitize"]
            run(child.name,command,timeout=360)
            evidence=json.loads((child/"results.json").read_text(encoding="utf-8"))
            if not evidence["Complete"] or not evidence["InputsUnchanged"] or evidence["Profiles"]!=11:
                raise RuntimeError("Incomplete native Array "+mode+" evidence")
            report[mode.title()+"Report"]={"Directory":str(child),"Checks":len(evidence["Rows"]),
                "ClosedPrefixes":len(evidence["Sweeps"]),"Complete":True,"InputsUnchanged":True}
        child=out / "admitted-import"
        command=[sys.executable,"tools/check_stdlib_array_import.py","--out",str(child)]
        if args.sanitize: command += ["--sanitize"]
        run(child.name,command,timeout=360)
        evidence=json.loads((child/"results.json").read_text(encoding="utf-8"))
        if not evidence["Complete"] or not evidence["InputsUnchanged"] or evidence["Profiles"]!=9 or len(evidence["Sweeps"])!=36:
            raise RuntimeError("Incomplete profiled Array import evidence")
        report["AdmittedImportReport"]={"Directory":str(child),"Checks":len(evidence["Rows"]),
            "ClosedPrefixes":len(evidence["Sweeps"]),"Complete":True,"InputsUnchanged":True}
        child=out / "recursive-import"
        command=[sys.executable,"tools/check_stdlib_array_import.py","--chain","--out",str(child)]
        if args.sanitize: command += ["--sanitize"]
        run(child.name,command,timeout=900)
        evidence=json.loads((child/"results.json").read_text(encoding="utf-8"))
        if not evidence["Complete"] or not evidence["InputsUnchanged"] or not evidence["Chain"] or len(evidence["Sweeps"])!=38:
            raise RuntimeError("Incomplete recursive Array import evidence")
        report["RecursiveImportReport"]={"Directory":str(child),"Checks":len(evidence["Rows"]),
            "ClosedPrefixes":len(evidence["Sweeps"]),"Complete":True,"InputsUnchanged":True}
        child=out / "resident-methods"
        command=[sys.executable,"tools/check_stdlib_array_methods.py","--out",str(child)]
        if args.sanitize: command += ["--sanitize"]
        run(child.name,command,timeout=360)
        evidence=json.loads((child/"results.json").read_text(encoding="utf-8"))
        if not evidence["Complete"] or not evidence["InputsUnchanged"] or evidence["Profiles"]!=9 or len(evidence["Sweeps"])!=252:
            raise RuntimeError("Incomplete profiled resident Array methods evidence")
        report["ResidentMethodsReport"]={"Directory":str(child),"Checks":len(evidence["Rows"]),
            "ClosedPrefixes":len(evidence["Sweeps"]),"Complete":True,"InputsUnchanged":True}
        child=out / "native-defaults"
        command=[sys.executable,"tools/check_stdlib_array_methods.py","--defaults","--out",str(child)]
        if args.sanitize: command += ["--sanitize"]
        run(child.name,command,timeout=360)
        evidence=json.loads((child/"results.json").read_text(encoding="utf-8"))
        if not evidence["Complete"] or not evidence["InputsUnchanged"] or evidence["Profiles"]!=12 or len(evidence["Sweeps"])!=48:
            raise RuntimeError("Incomplete profiled native Array defaults evidence")
        report["NativeDefaultsReport"]={"Directory":str(child),"Checks":len(evidence["Rows"]),
            "ClosedPrefixes":len(evidence["Sweeps"]),"Complete":True,"InputsUnchanged":True}
        child=out / "checked-extraction"
        command=[sys.executable,"tools/check_stdlib_array_methods.py","--extract","--out",str(child)]
        if args.sanitize: command += ["--sanitize"]
        run(child.name,command,timeout=360)
        evidence=json.loads((child/"results.json").read_text(encoding="utf-8"))
        if not evidence["Complete"] or not evidence["InputsUnchanged"] or evidence["Profiles"]!=9 or len(evidence["Sweeps"])!=172:
            raise RuntimeError("Incomplete checked native/wire Array extraction evidence")
        report["CheckedExtractionReport"]={"Directory":str(child),"Checks":len(evidence["Rows"]),
            "ClosedPrefixes":len(evidence["Sweeps"]),"Complete":True,"InputsUnchanged":True}
        child=out / "trivial-extraction"
        command=[sys.executable,"tools/check_stdlib_array_trivial_extract.py","--out",str(child)]
        if args.sanitize: command += ["--sanitize"]
        run(child.name,command,timeout=900)
        evidence=json.loads((child/"results.json").read_text(encoding="utf-8"))
        if not evidence["Complete"] or not evidence["InputsUnchanged"] or evidence["Profiles"]!=50 or sum(row["CompletePrefixes"] for row in evidence["Sweeps"])!=600:
            raise RuntimeError("Incomplete closed trivial Array extraction evidence")
        report["TrivialExtractionReport"]={"Directory":str(child),"Checks":len(evidence["Rows"]),
            "ClosedPrefixes":600,"Complete":True,"InputsUnchanged":True}
        child = out / "admission-c-scalar"
        command = [sys.executable, "tools/check_stdlib_array_c_scalar.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=180)
        c_scalar = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not c_scalar["Complete"] or not c_scalar["InputsUnchanged"] or c_scalar["NativeMappings"]!=21:
            raise RuntimeError("Incomplete exact native C scalar/type-id admission evidence")
        report["CScalarReport"] = {"Directory": str(child), "Checks": len(c_scalar["Rows"]),
            "Profiles": c_scalar["Profiles"], "NativeMappings": c_scalar["NativeMappings"],
            "ClosedPrefixes": len(c_scalar["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in c_scalar["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "stack-batch"
        command = [sys.executable, "tools/check_stdlib_stack_batch.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=180)
        batch = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not batch["Complete"] or not batch["InputsUnchanged"] or batch["Profiles"]!=8:
            raise RuntimeError("Incomplete native Stack batch/Array publication evidence")
        report["StackBatchReport"] = {"Directory": str(child), "Checks": len(batch["Rows"]),
            "Profiles": batch["Profiles"], "ClosedPrefixes": len(batch["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in batch["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "admission-sequence"
        command = [sys.executable, "tools/check_stdlib_array_sequence_admission.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=180)
        sequence = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not sequence["Complete"] or not sequence["InputsUnchanged"] or sequence["Profiles"]!=16 or len(sequence["Sweeps"])!=370:
            raise RuntimeError("Incomplete sealed nested Array domain evidence")
        report["SequenceAdmissionReport"] = {"Directory": str(child), "Checks": len(sequence["Rows"]),
            "Profiles": sequence["Profiles"], "ClosedPrefixes": len(sequence["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in sequence["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "admission-nullable-c-scalar"
        command = [sys.executable, "tools/check_stdlib_array_nullable_c_scalar.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        # Eleven independently compiled storage-only closures keep every
        # descriptor separate from converters. A finite compiler budget does
        # not change the strict flags or individual execution limits.
        run(child.name, command, timeout=600)
        optional = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not optional["Complete"] or not optional["InputsUnchanged"] or optional["NativeMappings"]!=21 or len(optional["Sweeps"])!=288:
            raise RuntimeError("Incomplete nullable native C scalar/type-id evidence")
        report["NullableCScalarReport"] = {"Directory": str(child), "Checks": len(optional["Rows"]),
            "Profiles": optional["Profiles"], "NativeMappings": optional["NativeMappings"],
            "ClosedPrefixes": len(optional["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in optional["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "admission-numeric-enum"
        command = [sys.executable, "tools/check_stdlib_array_numeric_enum.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        enums = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not enums["Complete"] or not enums["InputsUnchanged"] or enums["Profiles"]!=16 or len(enums["Sweeps"])!=384:
            raise RuntimeError("Incomplete numeric enum native Array evidence")
        report["NumericEnumReport"] = {"Directory": str(child), "Checks": len(enums["Rows"]),
            "Profiles": enums["Profiles"], "ClosedPrefixes": len(enums["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in enums["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "admission-callable"
        command = [sys.executable, "tools/check_stdlib_array_callable.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        callables = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not callables["Complete"] or not callables["InputsUnchanged"] or callables["Profiles"]!=4 or len(callables["Sweeps"])!=108:
            raise RuntimeError("Incomplete callable native Array evidence")
        report["CallableReport"] = {"Directory": str(child), "Checks": len(callables["Rows"]),
            "Profiles": callables["Profiles"], "ClosedPrefixes": len(callables["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in callables["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "admission-future"
        command = [sys.executable, "tools/check_stdlib_array_future.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        futures = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not futures["Complete"] or not futures["InputsUnchanged"] or len(futures["Sweeps"])!=54:
            raise RuntimeError("Incomplete native Future Array evidence")
        report["FutureReport"] = {"Directory": str(child), "Checks": len(futures["Rows"]),
            "ClosedPrefixes": len(futures["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in futures["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "shuffle"
        command = [sys.executable, "tools/check_stdlib_array_shuffle.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        shuffle = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not shuffle["Complete"] or not shuffle["InputsUnchanged"] or len(shuffle["Sweeps"])!=8:
            raise RuntimeError("Incomplete native Array shuffle consumer evidence")
        report["ShuffleReport"] = {"Directory": str(child), "Checks": len(shuffle["Rows"]),
            "ClosedPrefixes": len(shuffle["Sweeps"]),
            "AllocationPositions": sum(row["FirstNoHit"]+1 for row in shuffle["Sweeps"]),
            "Complete": True, "InputsUnchanged": True}
        child = out / "count"
        command = [sys.executable, "tools/check_stdlib_collection_count.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        count = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not count["Complete"] or not count["InputsUnchanged"] or len(count["Sweeps"])!=4:
            raise RuntimeError("Incomplete native/wire collection count evidence")
        report["CountReport"] = {"Directory": str(child), "Checks": len(count["Rows"]),
            "ClosedPrefixes": len(count["Sweeps"]), "Complete": True, "InputsUnchanged": True}
        child = out / "scalar-read"
        command = [sys.executable, "tools/check_stdlib_array_scalar_read.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        read = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not read["Complete"] or not read["InputsUnchanged"]:
            raise RuntimeError("Incomplete native/wire scalar Array read evidence")
        report["ScalarReadReport"] = {"Directory": str(child), "Checks": len(read["Rows"]),
            "Complete": True, "InputsUnchanged": True}
        child=out/'value-boundaries'
        command=[sys.executable,'tools/check_stdlib_array_scalar_read.py','--out',str(child),'--boundaries']
        if args.sanitize: command += ['--sanitize']
        run(child.name,command,timeout=600)
        boundaries=json.loads((child/'results.json').read_text(encoding='utf-8'))
        if not boundaries['Complete'] or not boundaries['InputsUnchanged'] or len(boundaries['Sweeps'])!=20:
            raise RuntimeError('Incomplete native/wire Array value boundaries evidence')
        report['ValueBoundariesReport']={'Directory':str(child),'Checks':len(boundaries['Rows']),
            'ClosedPrefixes':len(boundaries['Sweeps']),'Complete':True,'InputsUnchanged':True}
        child = out / "sequence-publication"
        command = [sys.executable, "tools/check_stdlib_sequence_publication.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        publication = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not publication["Complete"] or not publication["InputsUnchanged"] or len(publication["Sweeps"])!=50:
            raise RuntimeError("Incomplete Sequence publication/Stack domain evidence")
        report["SequencePublicationReport"] = {"Directory": str(child), "Checks": len(publication["Rows"]),
            "ClosedPrefixes": len(publication["Sweeps"]), "Complete": True, "InputsUnchanged": True}
        child = out / "sequence-factories"
        command = [sys.executable, "tools/check_stdlib_sequence_factories.py", "--out", str(child)]
        if args.sanitize:
            command += ["--sanitize"]
        run(child.name, command, timeout=600)
        factories = json.loads((child / "results.json").read_text(encoding="utf-8"))
        if not factories["Complete"] or not factories["InputsUnchanged"] or factories["Profiles"]!=9:
            raise RuntimeError("Incomplete construction-only native closure evidence")
        report["SequenceFactoriesReport"] = {"Directory": str(child), "Checks": len(factories["Rows"]),
            "Complete": True, "InputsUnchanged": True}
        report["InputsUnchanged"] = all(digest(Path(p["Path"])) == p["Sha256"] for p in inputs)
        if not report["InputsUnchanged"]:
            raise RuntimeError("Source or tool changed during acceptance")
        report["Complete"] = True
    except (RuntimeError, AssertionError, subprocess.TimeoutExpired) as exc:
        report["Failure"] = str(exc)
        print(str(exc), file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["Complete"]:
        return 1
    print(f"native Array: {len(rows)} checks, {len(sweeps)} closed fault prefixes; selected native closure balanced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
