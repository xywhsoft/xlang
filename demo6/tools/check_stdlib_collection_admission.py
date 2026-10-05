#!/usr/bin/env python3
"""Collection admission and logical Array defaults: IR, strict C, JIT and AOT.

Strict C links the real shared resident runtime; this gate does NOT certify a
pruned native closure. check_xlir_artifact_runtime supplies that independent
Windows check. Linux --sanitize instruments generated C/host, not shared libs.
"""
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
import time


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--public-directory", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--nominal", action="store_true", help="Select class/protocol and nullable nominal imports")
    parser.add_argument("--defaults", action="store_true", help="Select boxed Array logical defaults and checked resize effects")
    parser.add_argument("--layouts", action="store_true", help="Select Array carrier/layout facts and actual resident recipe oracle")
    parser.add_argument("--numeric-enums", action="store_true", help="Select exact nullable numeric-enum Value boundaries, negative constants and closed language OOM sweeps")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc = shutil.which("gcc")
    if not cc or out.exists() or (os.name == "nt" and not args.public_directory) or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc, a fresh output, Windows current-source driver and Linux for sanitizers")
    if sum((args.defaults, args.nominal, args.layouts, args.numeric_enums)) > 1:
        parser.error("Select one fixture family")
    fixtures = ["stdlib_nullable_numeric_enum", "stdlib_nullable_numeric_enum_absent", "enum_numeric_runtime", "enum_match_matrix"] if args.numeric_enums else ["stdlib_array_layouts", "stdlib_stack_nullable_c_scalar", "stdlib_stack_nullable_c_i8", "stdlib_array_future_reference"] if args.layouts else ["stdlib_array_defaults", "stdlib_array_defaults_continuation"] if args.defaults else ["stdlib_collection_nominal_admission", "protocol_container_matrix"] if args.nominal else [
        "stdlib_collection_admission", "cgen_dynamic_to_typed_containers"]
    public = json.loads((args.public_directory / "build.json").read_text(encoding="utf-8-sig")) if args.public_directory else None
    if public and (public["RuntimeOnly"] or not public["SourceXrt"]):
        parser.error("Full current-source compiler required")
    staged = None
    if public:
        manifest = args.public_directory / "artifact_sources.json"
        if not manifest.is_file():
            parser.error("Stage the current-source driver with stage_xlir_artifact_runtime_sources.ps1 first")
        staged = json.loads(manifest.read_text(encoding="utf-8-sig"))
    cli = Path(public["Driver"]) if public else root / "release/xl"
    files = [Path(__file__), Path(cc), Path(sys.executable), cli,
             root / "test/cases/stdlib_collection_admission_host.c",
             root / "test/cases/xlir_collection_admission_contract.c"]
    files += [root / f"test/cases/{fixture}.xl" for fixture in fixtures]
    if args.layouts:
        files += [root / "tools/check_xlir_function_value_calls.py",
                  root / "test/cases/xlir_function_value_calls.xl",
                  root / "test/cases/xlir_function_value_calls_contract.c",
                  root / "tools/check_stdlib_callable_scalars.py",
                  root / "test/cases/stdlib_callable_scalars.xl",
                  root / "test/cases/stdlib_callable_scalars_unused.xl",
                  root / "test/cases/stdlib_callable_scalars_contract.c",
                  root / "test/cases/stdlib_callable_scalars_host.c",
                  root / "test/cases/stdlib_callable_integer_domains.xl",
                  root / "test/cases/stdlib_callable_integer_domains_host.c",
                  root / "test/cases/stdlib_callable_native_scalars.xl",
                  root / "test/cases/stdlib_callable_native_scalars_host.c",
                  root / "test/cases/xlir_callable_bridge_mutations.h",
                  root / "test/cases/xlir_module_product_oom_host.c",
                  root / "test/support/module_product_memory.c", root / "test/support/module_product_memory.h"]
    if args.numeric_enums:
        files += [root / "test/cases/xlir_module_product_oom_host.c",
                  root / "test/support/module_product_memory.c", root / "test/support/module_product_memory.h"]
    if public:
        files += [args.public_directory / "build.json"] + [Path(row["Path"]) for row in public["Objects"]]
        files += [Path(row["Path"]) for row in public["Inputs"]]
        files += [Path(row["Path"]) for row in staged["Files"]]
        files += [args.public_directory / "xl.dll"]
        files += [root / "release/xl.dll"]
    else:
        files += [root / "release/libxl.so", root / "release/libxlc.so"]
    for directory in ("src/ir", "src/cgen", "src/compile", "src/runtime", "release/include"):
        files += sorted(p for p in (root / directory).rglob("*") if p.suffix in (".c", ".h", ".inc"))
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    private = out / "include/__xl_runtime"
    private.mkdir(parents=True)
    for directory in (root / "src/runtime", root / "release/include"):
        for header in directory.glob("*.h"):
            shutil.copy2(header, private / header.name)
    if os.name == "nt":
        shutil.copy2(root / "release/xl.dll", out / "xl.dll")
    rows = []
    report = {"Scope": __doc__, "Complete": False, "InputsUnchanged": False, "Platform": sys.platform,
              "Sanitized": args.sanitize, "Nominal": args.nominal, "Layouts": args.layouts,
              "NumericEnums": args.numeric_enums, "Sweeps": [],
              "Inputs": inputs, "Rows": rows, "Fixtures": fixtures}
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str], timeout: int = 60) -> str:
        started = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out / (name + ".log")).write_bytes((error.stdout or b"") + (error.stderr or b""))
            rows.append({"Name": name, "Command": command, "Exit": None, "Complete": False,
                         "TimedOut": True, "TimeoutSeconds": timeout, "Seconds": time.monotonic() - started})
            raise
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode, "Complete": result.returncode == 0,
                     "TimeoutSeconds": timeout, "Seconds": time.monotonic() - started})
        if result.returncode:
            raise RuntimeError(f"{name}: {data[-4000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    def emit(fixture: str, mode: str, target: Path) -> list[str]:
        source = f"test/cases/{fixture}.xl"
        if public:
            return [str(cli), source, mode, str(target)]
        if mode == "ir-lowered":
            return [str(cli), "ir", source, "--lowered", "--format", "json", "--emit", str(target)]
        return [str(cli), "cgen", source, "--inc-path", str(out / "include"), "--emit", str(target)]

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-unused-label",
              "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-unused-const-variable", "-Wno-missing-field-initializers",
              "-I.", "-Irelease", "-Irelease/include", "-I" + str(out / "include")]
    links = ["release/xl.dll"] if os.name == "nt" else ["-Lrelease", "-lxl", "-Wl,-rpath," + str(root / "release"), "-pthread", "-ldl", "-lm"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fno-var-tracking", "-fno-var-tracking-assignments",
                   "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    try:
        contract = out / ("contract.exe" if os.name == "nt" else "contract")
        compiler_links = ([row["Path"] for row in public["Objects"]
                           if Path(row["Path"]).name != "xlir_public_source_driver.o"] +
                          ["release/xl.dll", "-lws2_32", "-liphlpapi", "-lshell32", "-lole32", "-ladvapi32"]
                          if public else ["-lxlc", *links])
        run("contract.build", [cc, "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror", "-DXL_STATIC_EMBED",
            "-I.", "-Irelease", "-Irelease/include", "-Ilib/tcc", "test/cases/xlir_collection_admission_contract.c",
            *compiler_links, "-o", str(contract)])
        for fixture in fixtures:
            prefix = out / fixture
            generated, ir = prefix.with_suffix(".c"), prefix.with_suffix(".json")
            for repeat in (False, True):
                run(f"{fixture}.emit.{repeat}", emit(fixture, "cgen-default", generated if not repeat else prefix.with_suffix(".repeat.c")))
                run(f"{fixture}.ir.{repeat}", emit(fixture, "ir-lowered", ir if not repeat else prefix.with_suffix(".repeat.json")))
            assert generated.read_bytes() == prefix.with_suffix(".repeat.c").read_bytes()
            assert ir.read_bytes() == prefix.with_suffix(".repeat.json").read_bytes()
            text = generated.read_text(encoding="utf-8")
            assert "xlir_xvalue_collection_checked_take" not in text
            assert "xlir_nominal_runtime_tag" not in text
            assert not re.search(r"static bool xlir_nominal_set_\d+_matches",text)
            lowered = json.loads(ir.read_text(encoding="utf-8"))
            # Current carrier and target native layout are orthogonal. A recipe
            # in this dump must never be mistaken for a source owner cutover.
            types_by_id = {row["id"]: row for row in lowered["types"]}
            array_types = []
            for row in lowered["types"]:
                assert row["collectionCarrierAbi"] == (1 if row["collectionAbi"] else 0)
                plan = row["arrayLayout"]
                if row["collectionAbi"] != 1:
                    assert plan["abi"] == 0 and not any(plan[key] for key in (
                        "elementType", "baseElementType", "slotAbi", "codecAbi", "transferAbi", "admissionAbi", "nullable", "defaultAbi"))
                    continue
                array_types.append(row)
                if not row["params"]:
                    assert plan["abiName"] == "dynamic-value" and not plan["elementType"]
                    continue
                element = types_by_id[row["params"][0]]
                assert plan["elementType"] == element["id"]
                if plan["abiName"] == "future-reference-v1":
                    base=types_by_id[plan["baseElementType"]]
                    assert not element["nativeSlotAbi"] and base["cAbi"]==30  # RUNTIME_FUTURE_PTR
                    assert plan["slotAbiName"]=="future-ptr" and plan["codecAbiName"]=="direct" and plan["transferAbi"]==4
                elif plan["abiName"] == "callable-reference-v1":
                    base=types_by_id[plan["baseElementType"]]
                    assert not element["nativeSlotAbi"] and base["cAbi"]==31  # XL6_IR_C_ABI_CLOSURE_PTR
                    assert plan["slotAbiName"]=="xvalue-carrier" and plan["codecAbiName"]=="closure-value-v1"
                else:
                    assert plan["slotAbi"] == element["nativeSlotAbi"]
                    assert plan["codecAbiName"] == ("direct" if plan["defaultAbi"] else "none")
                if plan["abiName"] == "numeric-enum-v1":
                    base=types_by_id[plan["baseElementType"]]
                    assert not element["xvalueAdmissionAbi"]
                    assert plan["admissionAbi"] == [0,3,7,4,8,5,9,6,10][base["integerTarget"]]
                else:
                    assert plan["admissionAbi"] == element["xvalueAdmissionAbi"]
                if plan["nullable"]:
                    assert plan["baseElementType"] == element["params"][0]
                else:
                    assert plan["baseElementType"] == element["id"]
            if args.layouts and fixture=="stdlib_array_layouts":
                plans = [row["arrayLayout"] for row in array_types]
                assert {"primitive-v2", "reference-v2", "nominal-v2", "c-scalar-v1", "nullable-c-scalar-v1", "sequence-reference-v1", "pending-slot", "pending-domain"} <= {p["abiName"] for p in plans}
                nested=[p for p in plans if p['abiName']=='sequence-reference-v1']
                assert len(nested)>=3 and {False,True}=={p['nullable'] for p in nested}
                assert all(p['slotAbiName']=='xvalue-carrier' and not p['admissionAbi'] and
                    p['defaultAbiName']==('absent' if p['nullable'] else 'forbidden') for p in nested)
                c_scalars=[p for p in plans if p["abiName"]=="c-scalar-v1"]
                assert len(c_scalars)==21 and all(not p["nullable"] and not p["admissionAbi"] and p["defaultAbiName"]=="zero" for p in c_scalars)
                optional_c=[p for p in plans if p["abiName"]=="nullable-c-scalar-v1"]
                numeric=[p for p in plans if p["abiName"]=="numeric-enum-v1"]
                assert len(numeric)==16 and set(range(3,11))=={p["admissionAbi"] for p in numeric}
                assert {False,True}=={p["nullable"] for p in numeric}
                assert all(p["defaultAbiName"]==("absent" if p["nullable"] else "zero") for p in numeric)
                callable_plans=[p for p in plans if p["abiName"]=="callable-reference-v1"]
                future_plans=[p for p in plans if p["abiName"]=="future-reference-v1"]
                assert len(future_plans)>=2 and {False,True}=={p["nullable"] for p in future_plans}
                assert all(p["slotAbiName"]=="future-ptr" and p["transferAbi"]==4 and
                    p["defaultAbiName"]==("absent" if p["nullable"] else "forbidden") for p in future_plans)
                assert len(callable_plans)>=2 and {False,True}=={p["nullable"] for p in callable_plans}
                assert all(p["codecAbiName"]=="closure-value-v1" and p["transferAbi"]==3 and
                    not p["admissionAbi"] and p["defaultAbiName"]==("absent" if p["nullable"] else "forbidden")
                    for p in callable_plans)
                assert len(optional_c)==21 and all(p["nullable"] and not p["admissionAbi"] and
                    p["defaultAbiName"]=="absent" for p in optional_c)
                assert {"nullable-native-i8", "nullable-native-u8", "nullable-native-i16", "nullable-native-u16",
                    "nullable-native-i32", "nullable-native-u32", "nullable-type-id"} <= {p["slotAbiName"] for p in optional_c}
                assert set(range(1, 7)) <= {p["defaultAbi"] for p in plans}
                assert {"empty-bytes", "absent", "forbidden"} <= {p["defaultAbiName"] for p in plans}
                assert not any(p["defaultAbiName"] == "pending-bytes" for p in plans)
                assert {"nullable-bool", "nullable-i64", "nullable-u64", "nullable-f32", "nullable-f64", "nullable-time", "nullable-char", "nullable-string"} <= {p["slotAbiName"] for p in plans}
            if fixture.startswith("stdlib_stack_nullable_c_"):
                assert "xl6StackCreateV1" in text and "xlir_xvalue_box_xlir_nullable_" not in text
                slots={row["nativeSlotAbiName"] for row in lowered["types"]}
                if fixture=="stdlib_stack_nullable_c_scalar":
                    assert {"nullable-native-i8", "nullable-native-u8", "nullable-native-i16", "nullable-native-u16",
                        "nullable-native-i32", "nullable-native-u32", "nullable-type-id"} <= slots
                else:
                    assert "xl6NullableNativeI8TypeV1()" in text
                    assert not re.search(r"xl6Nullable(?:Native(?:U8|I16|U16|I32|U32)|Bool|I64|U64|F32|F64|Time|Char)(?:Type|Converter)V1\(\)",text)
            checked = [row for row in lowered["instructions"] if row["runtimeOpName"] == "xvalue-collection-checked-rebind"]
            calls = re.findall(r"= xl6CollectionCheckedTakeV1\(v\d+, \d+, ([1-9]\d*), (true|false), 0x([0-9A-F]{16})ULL\);", text)
            nominal_calls = re.findall(r"= xl6CollectionCheckedNominalTakeV1\(v\d+, \d+, &xlir_nominal_set_(\d+), (true|false)\);",text)
            primitive = [row for row in checked if row["collectionAdmissionAbi"] == 1]
            nominal = [row for row in checked if row["collectionAdmissionAbi"] == 2]
            assert len(calls) == len(primitive) and len(nominal_calls) == len(nominal)
            assert len(checked) == len(primitive) + len(nominal)
            types = {row["id"]: row for row in lowered["types"]}
            defaults = [row for row in lowered["instructions"] if row["collectionDefaultAbi"]]
            default_calls = re.findall(r"v(\d+) = xl6ArrayResizeDefaultV1\(v(\d+), v(\d+), (\d+)\);", text)
            # Async/resume machines can emit one canonical SSA operation in
            # multiple execution shells. Verify exact tuples, not text counts.
            expected_defaults = {(row["result"], *row["operands"], row["collectionDefaultAbi"]) for row in defaults}
            assert {tuple(map(int, row)) for row in default_calls} == expected_defaults
            for operation in defaults:
                assert operation["typedCollectionOpName"] == "resize"
            if args.defaults:
                assert defaults and "xlir_xvalue_typed_array_resize" not in text
                assert "xlir_xvalue_typed_array_default" not in text
                if fixture == "stdlib_array_defaults":
                    assert set(range(1,11)) == {row["collectionDefaultAbi"] for row in defaults}
            for operation, (rule, nullable, identity) in zip(primitive, calls):
                payload = types[operation["payloadType"]]
                assert int(rule) == payload["xvalueAdmissionAbi"]
                assert (nullable == "true") == payload["xvalueAdmissionNullable"]
                assert int(identity, 16) == int(payload["xvalueAdmissionTypeValueId"], 16)
                assert operation["collectionAdmissionIdentitySet"] == 0
                assert operation["collectionAdmissionNullable"] == payload["xvalueAdmissionNullable"]
            for operation, (identity_set, nullable) in zip(nominal, nominal_calls):
                assert int(identity_set) == operation["collectionAdmissionIdentitySet"]
                assert (nullable == "true") == operation["collectionAdmissionNullable"]
                payload = types[operation["payloadType"]]
                target = payload["params"][0] if operation["collectionAdmissionNullable"] else payload["id"]
                assert lowered["nominalIdentitySets"][int(identity_set)-1]["targetType"] == target
            for match in re.finditer(r"static const XL6_NominalAdmissionV1 xlir_nominal_set_(\d+) = \{(\d+), 0x([0-9A-F]{16})ULL, (xlir_nominal_ids_\d+|NULL), (\d+)\};",text):
                identity_set,kind,non_object,span,count=match.groups()
                schema=lowered["nominalIdentitySets"][int(identity_set)-1]
                assert int(kind)==schema["kind"] and int(non_object,16)==schema["nonObjectTypeId"]
                assert int(count)==len(schema["identities"])
                if span == "NULL":
                    assert not schema["identities"]
                else:
                    data=re.search(r"static const uint64_t "+re.escape(span)+r"\[\] = \{(.*?)\};",text,re.S)
                    assert data
                    assert [int(value,16) for value in re.findall(r"0x([0-9A-F]{16})ULL",data[1])] == schema["identities"]
            if fixture == "stdlib_collection_nominal_admission":
                assert len(nominal)==15 and not primitive
                assert {False, True} == {row["collectionAdmissionNullable"] for row in nominal}
            if fixture == "stdlib_collection_admission":
                assert {3, 6, 7, 8, 11, 13, 14, 15, 16} <= {int(row[0]) for row in calls}
                assert len(checked) == 25
            contract_c = prefix.with_suffix(".contract.c")
            contract_log = run(f"{fixture}.contract", [str(contract), f"test/cases/{fixture}.xl", str(contract_c),
                *(["numeric-enums"] if args.numeric_enums else [])])
            assert "Future container semantics: 5 accepted; 7 independent copy/resource refusals" in contract_log
            if fixture == "stdlib_array_future_reference":
                projections = [row for row in lowered["instructions"] if row["runtimeOpName"] in (
                    "xvalue-project-future-copy", "xvalue-project-nullable-future-copy")]
                assert {"xvalue-project-future-copy", "xvalue-project-nullable-future-copy"} == {row["runtimeOpName"] for row in projections}
                for projection in projections:
                    result = types_by_id[projection["type"]]
                    future = types_by_id[result["params"][0]] if projection["runtimeOpName"] == "xvalue-project-nullable-future-copy" else result
                    assert projection["payloadType"] == future["params"][0]
                assert f"Future projection contracts: {len(projections)} projections; {len(projections)*5} payload/nullability/metadata mutations refused before C/header output" in contract_log
            if args.numeric_enums and fixture in ("stdlib_nullable_numeric_enum", "enum_numeric_runtime"):
                enum_proof = re.search(r"numeric enum bridge: (\d+) binds; (\d+) nullable boxes; (\d+) rejected mutations before C/header output", contract_log)
                assert enum_proof and int(enum_proof[1]) > 0 and int(enum_proof[3]) > 0
                if fixture == "stdlib_nullable_numeric_enum":
                    assert int(enum_proof[2]) >= 16
                    assert "(-9223372036854775807LL - 1LL)" in text and "18446744073709551615ULL" in text
            recipe_count = sum(row["arrayLayout"]["abiName"] in
                ("primitive-v2", "reference-v2", "nominal-v2", "c-scalar-v1", "nullable-c-scalar-v1", "sequence-reference-v1", "numeric-enum-v1", "callable-reference-v1", "future-reference-v1") for row in array_types)
            recursive_mutations=sum(row['arrayLayout']['abiName']=='sequence-reference-v1' and
                len(types[row['arrayLayout']['baseElementType']]['params'])==1 for row in array_types)
            codec_mutations=sum(row['arrayLayout']['abiName']=='callable-reference-v1' for row in array_types)
            signature_mutations=sum(row['arrayLayout']['abiName']=='callable-reference-v1' and
                bool(types_by_id[row['arrayLayout']['baseElementType']]['params']) for row in array_types)
            future_mutations=sum(row['arrayLayout']['abiName']=='future-reference-v1' for row in array_types)
            mutation_count=len(lowered['types'])*12+recursive_mutations+codec_mutations+signature_mutations+future_mutations
            if not args.numeric_enums:
                assert f"array layout: {len(array_types)} arrays; {recipe_count} real resident recipes; {mutation_count} carrier/layout mutations" in contract_log, 'Actual resident recipe/mutation totals differ from IR'
            assert f"collection contract: {len(checked)} checked rebinds;" in contract_log
            if defaults:
                assert f"array defaults: {len(defaults)} resize contracts;" in contract_log
            assert generated.read_bytes() == contract_c.read_bytes(), "Mutation contract changed canonical C"
            for level in (0, 2):
                binary = prefix.with_suffix(f".O{level}" + (".exe" if os.name == "nt" else ""))
                if fixture == "stdlib_array_future_reference":
                    memory = out / f"memory.O{level}.o"
                    run(f"future-memory.O{level}.build", [cc, *strict, f"-O{level}", "-c",
                        "test/support/module_product_memory.c", "-o", str(memory)])
                    source_arguments = ["-include", str(generated),
                        "test/cases/xlir_module_product_oom_host.c", str(memory)]
                else:
                    source_arguments = [str(generated), "test/cases/stdlib_collection_admission_host.c"]
                run(f"{fixture}.O{level}.build", [cc, *strict, f"-O{level}", *source_arguments, *links, "-o", str(binary)],
                    # The rich default/layout fixtures exceed 180 s in
                    # full O2 ASan/UBSan compilation. Only that compiler step
                    # has a larger finite budget, with every strict flag kept;
                    # executions, JIT, AOT and ordinary C still use 60 s.
                    timeout=(360 if args.defaults or args.layouts else 180) if args.sanitize and level == 2 else 60)
                language_proof = run(f"{fixture}.O{level}.run", [str(binary)])
                if fixture == "stdlib_array_future_reference":
                    assert "hit=0 balanced" in language_proof
                if args.numeric_enums and fixture.startswith("stdlib_nullable_numeric_enum"):
                    lane = f"{fixture}.O{level}.oom"
                    memory = out / f"memory.O{level}.o"
                    if not memory.exists():
                        run(f"memory.O{level}.build", [cc, *strict, f"-O{level}", "-c",
                            "test/support/module_product_memory.c", "-o", str(memory)])
                    oom = out / (lane + (".exe" if os.name == "nt" else ""))
                    run(lane + ".build", [cc, *strict, f"-O{level}", "-DXL6_PRODUCT_USER_ENTRY",
                        "-include", str(generated), "test/cases/xlir_module_product_oom_host.c",
                        str(memory), *links, "-o", str(oom)], timeout=360 if args.sanitize and level == 2 else 60)
                    assert "hit=0 balanced" in run(lane + ".normal", [str(oom)])
                    for point in range(4097):
                        proof = run(lane + f".{point}", [str(oom), str(point)])
                        assert re.search(r"hit=[01] balanced", proof)
                        if "hit=0 balanced" in proof:
                            report["Sweeps"].append({"Lane": lane, "FirstNoHit": point})
                            break
                    else:
                        raise AssertionError("Unclosed numeric-enum language allocation prefix")
            source = f"test/cases/{fixture}.xl"
            jit = [str(cli), source, "run", str(prefix)] if public else [str(cli), "run", source, "--inc-path", str(out / "include")]
            log = run(f"{fixture}.jit", jit)
            assert not re.search(r"\[(?:sem|checked|cgen)\]", log)
            if public:
                run(f"{fixture}.aot.build", [str(cli), source, "exe", str(prefix)])
                product = prefix.with_suffix(".exe")
            else:
                product = prefix.with_suffix(".product")
                run(f"{fixture}.aot.build", [str(cli), "build", source, "--inc-path", str(out / "include"), "-o", str(product)])
            assert product.is_file(), f"AOT output missing: {product}"
            run(f"{fixture}.aot.run", [str(product)])
        if args.layouts:
            child = out / "callable-scalars"
            command = [sys.executable, str(root / "tools/check_stdlib_callable_scalars.py"), "--out", str(child)]
            if args.public_directory:
                command += ["--public-directory", str(args.public_directory.resolve())]
            if args.sanitize:
                command += ["--sanitize"]
            run("callable-scalars", command, timeout=1800)
            proof = json.loads((child / "results.json").read_text(encoding="utf-8"))
            assert proof["Complete"] and proof["InputsUnchanged"] and len(proof["Sweeps"]) == 18
            assert all(digest(Path(row["Path"])) == row["Sha256"] for row in proof["Inputs"])
            report["CallableScalarsReport"] = str(child / "results.json")
            child = out / "function-value-calls"
            command = [sys.executable, str(root / "tools/check_xlir_function_value_calls.py"), "--out", str(child)]
            if args.public_directory:
                command += ["--public-directory", str(args.public_directory.resolve())]
            if args.sanitize:
                command += ["--sanitize"]
            run("function-value-calls", command, timeout=600)
            proof = json.loads((child / "results.json").read_text(encoding="utf-8"))
            assert proof["Complete"] and proof["InputsUnchanged"] and len(proof["Sweeps"]) == 2
            assert proof["Contract"]["SemanticRejections"] == 64
            assert all(digest(Path(row["Path"])) == row["Sha256"] for row in proof["Inputs"])
            report["FunctionValueCallsReport"] = str(child / "results.json")
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        report["Complete"] = report["InputsUnchanged"]
        if not report["Complete"]:
            raise RuntimeError("Frozen inputs changed during acceptance")
    except Exception as error:
        report["Error"] = f"{type(error).__name__}: {error}"
        print(error, file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Collection admission: {len(rows)} checks, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
