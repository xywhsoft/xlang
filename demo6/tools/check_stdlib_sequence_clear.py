#!/usr/bin/env python3
"""Explicit Sequence clear: first-error native cleanup and language void/status IR.

Native O0/O2 closures are fully instrumented on Linux with --sanitize. Strict
language C uses the real shared runtime (host-only instrumentation), with JIT,
AOT, deterministic emission, and pre-output contract mutation refusal. This
gate is not Array language migration or whole-task/pruned-language acceptance.
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


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--public-directory", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which("gcc"), shutil.which("nm")
    if out.exists() or not cc or not nm or (os.name == "nt" and not args.public_directory) or (os.name == "nt" and args.sanitize):
        parser.error("Require gcc/nm, fresh evidence, a Windows current-source driver, and Linux for sanitizers")
    public = json.loads((args.public_directory / "build.json").read_text(encoding="utf-8-sig")) if args.public_directory else None
    if public and (public["RuntimeOnly"] or not public["SourceXrt"]):
        parser.error("Full current-source compiler/runtime required")
    cli = Path(public["Driver"]) if public else root / "release/xl"
    staged = None
    if public:
        records = [row for key in ("Inputs", "Headers", "Objects") for row in public[key]]
        if (any(digest(Path(row["Path"])).upper() != row["Sha256"].upper() for row in records)
                or digest(cli).upper() != public["DriverSha256"].upper()
                or digest(root / "release/xl.dll").upper() != public["RuntimeSha256"].upper()):
            parser.error("Stale current-source compiler/runtime evidence")
        staged_path = args.public_directory / "artifact_sources.json"
        if not staged_path.is_file():
            parser.error("Run stage_xlir_artifact_runtime_sources.ps1 for this public directory first")
        staged = json.loads(staged_path.read_text(encoding="utf-8-sig"))
        if (staged["DriverSha256"].upper() != public["DriverSha256"].upper()
                or any(digest(Path(row[field])).upper() != row["Sha256"].upper()
                       for row in staged["Files"] for field in ("Source", "Path"))):
            parser.error("Stale native artifact source package")
    fixtures = ["stdlib_stack_clear_minimal", "stdlib_stack_clear_error", "stdlib_stack_clear_error_continuation"]
    sidecar = "test/support/sequence_clear_probe.c"
    native_sources = ["test/cases/stdlib_sequence_clear_native_host.c", sidecar, "test/support/array_value_native.c",
                      "src/runtime/xl6_sequence.c", "src/runtime/xl6_stack.c", "src/runtime/xl6_array.c",
                      "src/runtime/xl6_array_erased.c", "src/runtime/xl6_array_reference_value.c",
                      "src/runtime/xl6_ownership_plan.c", "src/api/xl_foreign_error.c"]
    files = [Path(__file__), Path(cc), Path(nm), Path(sys.executable), cli,
             root / "test/cases/xlir_stack_native_contract.c", root / "test/cases/xlir_time_text_closure_host.c",
             root / "lib/xrt/xrt.h", root / "lib/xruntime/xruntime.h"]
    files.append(root / 'src/api/xl_foreign_error.h')
    files += [root / name for name in native_sources] + [root / f"test/cases/{name}.xl" for name in fixtures]
    files += [root / "release/xl.dll"] if public else [root / "release/libxl.so", root / "release/libxlc.so"]
    if public:
        files += [args.public_directory / "build.json", args.public_directory / "xl.dll"]
        files += [Path(row["Path"]) for key in ("Inputs", "Headers", "Objects") for row in public[key]]
        files += [args.public_directory / "artifact_sources.json", root / staged["Mapping"],
                  root / "tools/stage_xlir_artifact_runtime_sources.ps1"]
        files += [Path(row[field]) for row in staged["Files"] for field in ("Source", "Path")]
    else:
        for directory in ("src/ir", "src/cgen", "src/compile", "src/runtime", "include"):
            files += sorted(p for p in (root / directory).rglob("*") if p.suffix in (".c", ".h", ".inc"))
    for directory in ("src/runtime", "release/include"):
        files += sorted((root / directory).rglob("*.h"))
    inputs = [{"Path": str(path.resolve()), "Sha256": digest(path)} for path in sorted(set(files))]
    out.mkdir(parents=True)
    private = out / "include/__xl_runtime"
    private.mkdir(parents=True)
    for directory in (root / "src/runtime", root / "release/include"):
        for header in directory.glob("*.h"):
            shutil.copy2(header, private / header.name)
    if os.name == "nt":
        shutil.copy2(root / "release/xl.dll", out / "xl.dll")
    rows = []
    report = {"Scope": __doc__, "Complete": False, "InputsUnchanged": False, "Inputs": inputs,
              "Rows": rows, "Fixtures": fixtures, "Sanitized": args.sanitize,
              "NativeRuntimeInstrumented": args.sanitize, "SharedRuntimeInstrumented": False}
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_stack_use_after_return=1:detect_leaks=1:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str], timeout: int = 60) -> str:
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out / (name + ".log")).write_bytes((error.stdout or b"") + (error.stderr or b""))
            rows.append({"Name": name, "Command": command, "Complete": False, "TimedOut": True})
            raise
        data = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode, "Complete": result.returncode == 0})
        if result.returncode:
            raise RuntimeError(f"{name}: {data[-4000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I.", "-Irelease", "-Irelease/include"]
    native_links = [] if os.name == "nt" else ["-pthread", "-lm"]
    links = ["release/xl.dll"] if public else ["-Lrelease", "-lxl", "-Wl,-rpath,"+str(root/"release"), "-pthread", "-ldl", "-lm"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    instrument = ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"] if args.sanitize else []
    suffix = ".exe" if os.name == "nt" else ""
    language_flags = strict + instrument + ["-Wno-unused-function", "-Wno-unused-label", "-Wno-unused-variable",
        "-Wno-unused-parameter", "-Wno-unused-const-variable", "-Wno-unused-but-set-variable",
        "-Wno-missing-field-initializers", "-I"+str(out/"include")]
    try:
        for level in (0, 2):
            binary = out / (f"native.O{level}"+suffix)
            run(f"native.O{level}.build", [cc, *strict, *instrument, f"-O{level}",
                "-DXL6_SEQUENCE_CLEAR_NATIVE", "-DXL6_TEST_ERASED_STACK", *native_sources, *native_links, "-o", str(binary)])
            symbols = run(f"native.O{level}.symbols", [nm, "--defined-only", str(binary)])
            names = {line.split()[-1].lstrip("_") for line in symbols.splitlines() if line.split()}
            assert {"xl6StackClearV1", "xl6ArrayClearV1", "xl6ArrayErasedClearV1", "xrtTypedArrayClear"} <= names
            assert not any(re.match(r"xl6(?:Process|Log|Regex)|xrtTypedQueue", name) for name in names)
            log = run(f"native.O{level}.vectors", [str(binary)])
            assert "96 error/fault/pending vectors, 3 last-owner cases, balanced" in log
        contract = out / ("contract"+suffix)
        objects = [row["Path"] for row in public["Objects"] if Path(row["Path"]).name != "xlir_public_source_driver.o"] if public else []
        contract_links = [*objects, "release/xl.dll", "-lWs2_32", "-lIPHLPAPI", "-lShell32", "-lOle32", "-lAdvapi32"] if public else ["-Lrelease", "-lxlc", *links]
        run("contract.build", [cc, *language_flags, "-DXL_STATIC_EMBED", "-Ilib/tcc",
            "test/cases/xlir_stack_native_contract.c", *contract_links, "-o", str(contract)])
        for fixture in fixtures:
            source = f"test/cases/{fixture}.xl"
            prefix = out / fixture
            generated = prefix.with_suffix(".c")

            def emit(mode: str, target: Path) -> list[str]:
                if public:
                    return [str(cli), source, mode, str(target)]
                if mode in ("ir-typed", "ir-lowered"):
                    return [str(cli), "ir", source, *(["--lowered"] if mode == "ir-lowered" else []), "--format", "json", "--emit", str(target)]
                return [str(cli), "cgen", source, "--inc-path", str(out/"include"), "--emit", str(target)]

            run(fixture+".emit", emit("emit", prefix if public else generated))
            repeat = out / (fixture+".repeat")
            run(fixture+".repeat", emit("emit", repeat if public else repeat.with_suffix(".c")))
            assert generated.read_bytes() == repeat.with_suffix(".c").read_bytes()
            code = generated.read_text(encoding="utf-8-sig")
            status_calls = re.findall(r"v(\d+) = xl6StackClearV1\([^\n]+;", code)
            status_tests = re.findall(r"v(\d+) = xl6StackClearV1\([^\n]+;\n\s+if \(v\1\)", code)
            assert status_calls and status_calls == status_tests, "Clear must branch on its verified status, not TLS presence"
            for mode in ("ir-typed", "ir-lowered"):
                run(fixture+"."+mode, emit(mode, out/(fixture+"."+mode+".json")))
            typed = json.loads((out/(fixture+".ir-typed.json")).read_text(encoding="utf-8-sig"))
            lowered = json.loads((out/(fixture+".ir-lowered.json")).read_text(encoding="utf-8-sig"))
            clears = [row for row in typed["intrinsicCalls"] if row.get("runtimeResourceOpName")=="stack-clear"]
            assert clears and all(row["resultType"]==0 and row["errorAbiName"]=="may-raise" for row in clears)
            definitions = {row["result"]: row for row in lowered["instructions"] if row["result"]}
            operations = [row for row in lowered["instructions"] if row.get("runtimeOpName")=="stack-clear"]
            tests = [row for row in lowered["instructions"] if row["kindName"]=="error-test"]
            types = {row["id"]: row for row in lowered["types"]}
            assert operations and all(row["result"] and types[row["type"]]["cAbi"]==2 for row in operations)
            tested = [definitions[row["operands"][0]] for row in tests if row["operands"] and row["operands"][0] in definitions]
            assert len([row for row in tested if row.get("runtimeOpName")=="stack-clear"])==len(operations)
            # The fixture's native producer does not invent a generic identity.
            var = next(row for row in typed["types"] if row["name"]=="var")
            value = 1469598103934665603
            for byte in b"type:var":
                value = ((value ^ byte)*1099511628211)&((1<<64)-1)
            assert int(var["typeValueId"],16)==value
            canonical = out/(fixture+".contract.c")
            run(fixture+".contract", [str(contract), source, str(canonical)], timeout=90)
            assert generated.read_bytes()==canonical.read_bytes()
            for level in (0,2):
                binary = out/(fixture+f".O{level}"+suffix)
                probe = out/(fixture+f".O{level}.probe.o")
                run(fixture+f".O{level}.probe", [cc,*language_flags,f"-O{level}","-c",sidecar,"-o",str(probe)])
                entry = [] if fixture.endswith("continuation") else ["-DXL6_TIME_USER_ENTRY"]
                run(fixture+f".O{level}.build", [cc,*language_flags,f"-O{level}",*entry,"-include",str(generated),
                    "test/cases/xlir_time_text_closure_host.c",str(probe),*links,"-o",str(binary)], timeout=180)
                run(fixture+f".O{level}.run", [str(binary)])
            jit = [str(cli),source,"run",str(prefix),sidecar] if public else [str(cli),"run",source,sidecar,"--inc-path",str(out/"include")]
            run(fixture+".jit",jit)
            product = prefix.with_suffix(".exe" if public else ".product")
            command = [str(cli),source,"exe",str(prefix),sidecar] if public else [str(cli),"build",source,sidecar,"--inc-path",str(out/"include"),"-o",str(product)]
            run(fixture+".aot.build",command)
            assert product.is_file()
            run(fixture+".aot.run",[str(product)])
        report["InputsUnchanged"] = all(digest(Path(row["Path"]))==row["Sha256"] for row in inputs)
        report["Complete"] = report["InputsUnchanged"]
        if not report["Complete"]:
            raise RuntimeError("Frozen acceptance inputs changed")
    except Exception as error:
        report["Error"] = f"{type(error).__name__}: {error}"
        print(error,file=sys.stderr)
    finally:
        (out/"results.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"Sequence clear: {len(rows)} checks, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__=="__main__":
    raise SystemExit(main())
