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
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc = shutil.which("gcc")
    if not cc or out.exists() or (os.name == "nt" and not args.public_directory) or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc, a fresh output, Windows current-source driver and Linux for sanitizers")
    if sum((args.defaults, args.nominal, args.layouts)) > 1:
        parser.error("Select one fixture family")
    fixtures = ["stdlib_array_layouts"] if args.layouts else ["stdlib_array_defaults", "stdlib_array_defaults_continuation"] if args.defaults else ["stdlib_collection_nominal_admission", "protocol_container_matrix"] if args.nominal else [
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
                        "elementType", "baseElementType", "slotAbi", "transferAbi", "admissionAbi", "nullable", "defaultAbi"))
                    continue
                array_types.append(row)
                if not row["params"]:
                    assert plan["abiName"] == "dynamic-value" and not plan["elementType"]
                    continue
                element = types_by_id[row["params"][0]]
                assert plan["elementType"] == element["id"] and plan["slotAbi"] == element["nativeSlotAbi"]
                assert plan["admissionAbi"] == element["xvalueAdmissionAbi"]
                if plan["nullable"]:
                    assert plan["baseElementType"] == element["params"][0]
                else:
                    assert plan["baseElementType"] == element["id"]
            if args.layouts:
                plans = [row["arrayLayout"] for row in array_types]
                assert {"primitive-v2", "reference-v2", "nominal-v2", "c-scalar-v1", "pending-slot", "pending-domain"} <= {p["abiName"] for p in plans}
                c_scalars=[p for p in plans if p["abiName"]=="c-scalar-v1"]
                assert len(c_scalars)==21 and all(not p["nullable"] and not p["admissionAbi"] and p["defaultAbiName"]=="zero" for p in c_scalars)
                assert set(range(1, 7)) <= {p["defaultAbi"] for p in plans}
                assert {"empty-bytes", "absent", "forbidden"} <= {p["defaultAbiName"] for p in plans}
                assert not any(p["defaultAbiName"] == "pending-bytes" for p in plans)
                assert {"nullable-bool", "nullable-i64", "nullable-u64", "nullable-f32", "nullable-f64", "nullable-time", "nullable-char", "nullable-string"} <= {p["slotAbiName"] for p in plans}
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
            contract_log = run(f"{fixture}.contract", [str(contract), f"test/cases/{fixture}.xl", str(contract_c)])
            recipe_count = sum(row["arrayLayout"]["abiName"] in
                ("primitive-v2", "reference-v2", "nominal-v2", "c-scalar-v1") for row in array_types)
            assert f"array layout: {len(array_types)} arrays; {recipe_count} real resident recipes; {len(lowered['types']) * 11} carrier/layout mutations" in contract_log
            assert f"collection contract: {len(checked)} checked rebinds;" in contract_log
            if defaults:
                assert f"array defaults: {len(defaults)} resize contracts;" in contract_log
            assert generated.read_bytes() == contract_c.read_bytes(), "Mutation contract changed canonical C"
            for level in (0, 2):
                binary = prefix.with_suffix(f".O{level}" + (".exe" if os.name == "nt" else ""))
                run(f"{fixture}.O{level}.build", [cc, *strict, f"-O{level}", str(generated),
                    "test/cases/stdlib_collection_admission_host.c", *links, "-o", str(binary)],
                    # The rich default/layout fixtures exceed 180 s in
                    # full O2 ASan/UBSan compilation. Only that compiler step
                    # has a larger finite budget, with every strict flag kept;
                    # executions, JIT, AOT and ordinary C still use 60 s.
                    timeout=(360 if args.defaults or args.layouts else 180) if args.sanitize and level == 2 else 60)
                run(f"{fixture}.O{level}.run", [str(binary)])
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
