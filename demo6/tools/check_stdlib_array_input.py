#!/usr/bin/env python3
"""B85 native Array inputs and split Future aggregates/metadata with complete OOM prefixes.

Language regressions link the current full source runtime; separate artifact
closure gates prove pruning. They are not proof of the complete carrier cutover.
Native host creates sealed resident Arrays and calls every changed consumer.
Linux sanitizer instruments input/text/process/regex/pattern, aggregate
kernel/Array adapter, metadata/legacy bridge and hosts; Sequence/XRT stay shared.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
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
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc = shutil.which("gcc")
    if not cc or out.exists() or (os.name == "nt" and not args.public_directory) or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc, fresh evidence, current Windows source compiler, Linux sanitizer")
    public = json.loads((args.public_directory / "build.json").read_text(encoding="utf-8-sig")) if args.public_directory else None
    if public and (public["RuntimeOnly"] or not public["SourceXrt"]):
        parser.error("Require full source compiler")
    cli = Path(public["Driver"]) if public else root / "release/xl"
    fixtures = ["stdlib_text_join_only", "stdlib_text_join", "stdlib_regex_set_only", "stdlib_pattern_builder",
                "stdlib_pattern_builder_lifetime", "xlir_task_aggregate_array_abi", "xlir_task_when_any_array_abi",
                "stdlib_future_aggregate_variadic_only", "stdlib_future_aggregate_array_only", "stdlib_future_metadata_only"]
    resident = ["src/runtime/xl6_array_input.c", "src/runtime/xl6_text_join.c", "src/runtime/xl6_process_command.c",
                "src/runtime/xl6_regex_set.c", "src/runtime/xl6_pattern_builder.c",
                "src/runtime/xl6_future_aggregate.c", "src/runtime/xl6_future_aggregate_array.c"]
    host = "test/cases/stdlib_array_input_host.c"
    files = [Path(__file__), Path(cc), Path(sys.executable), cli, root / host,
             root / "test/support/module_product_memory.c", root / "test/support/module_product_memory.h",
             root / "test/cases/xlir_module_product_oom_host.c"]
    files += [root / f"test/cases/{name}.xl" for name in fixtures]
    files += [root / "test/cases/xlir_future_metadata_contract.c",root / "test/cases/xlir_continuation_owner_test.h",
              root / "test/cases/xlir_synthetic_owner_edit.h",
              root / "test/cases/stdlib_future_metadata_host.c",root / "src/runtime/xl6_future_metadata.c",
              root / "src/api/xl_future_bridge.c",root / "test/cases/xlir_future_aggregate_code_owner_runtime.c",
              root / "test/cases/xlir_future_aggregate_code_owner_box.c",
              root / "test/cases/xlir_future_aggregate_code_owner_box.h"]
    if public:
        stage = args.public_directory / "artifact_sources.json"
        staged = json.loads(stage.read_text(encoding="utf-8-sig"))
        for row in public["Inputs"] + public["Headers"] + public["Objects"] + staged["Files"]:
            if digest(Path(row["Path"])) != row["Sha256"].lower():
                raise RuntimeError(f"Stale build/stage input: {row['Path']}")
        files += [args.public_directory / "build.json", stage, args.public_directory / "xl.dll", root / "release/xl.dll"]
        files += [Path(row["Path"]) for row in public["Inputs"] + public["Headers"] + public["Objects"] + staged["Files"]]
    else:
        files += [root / "release/libxl.so", root / "release/libxlc.so"]
    for folder in ("src", "release/include", "lib/xruntime", "lib/xrt"):
        files += [p for p in (root / folder).rglob("*") if p.suffix in (".c", ".h", ".inc")]
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    private = out / "include/__xl_runtime"
    private.mkdir(parents=True)
    for directory in (root / "src/runtime", root / "release/include"):
        for header in directory.glob("*.h"):
            shutil.copy2(header, private / header.name)
    if os.name == "nt":
        shutil.copy2(root / "release/xl.dll", out / "xl.dll")
    rows: list[dict] = []
    report = {"Complete": False, "InputsUnchanged": False, "Scope": __doc__, "Platform": sys.platform,
              "Inputs": inputs, "Rows": rows, "Sweeps": [], "MetadataSweeps": [], "Fixtures": fixtures}
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(name: str, command: list[str], timeout: int = 120) -> str:
        started = time.monotonic()
        try:
            process = subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       start_new_session=os.name != "nt",
                                       creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            stdout, stderr = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired as error:
            if os.name == "nt":
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, timeout=15)
            else:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            stdout, stderr = process.communicate(timeout=15)
            (out / (name + ".log")).write_bytes(stdout + stderr)
            rows.append({"Name": name, "Command": command, "Exit": None, "Complete": False, "Timeout": timeout})
            raise
        data = stdout + stderr
        (out / (name + ".log")).write_bytes(data)
        rows.append({"Name": name, "Command": command, "Exit": process.returncode,
                     "Complete": process.returncode == 0, "Seconds": time.monotonic() - started})
        if process.returncode:
            raise RuntimeError(f"{name}: {data[-5000:].decode(errors='replace')}")
        return data.decode(errors="replace")

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-DXL_STATIC_EMBED", "-Wno-unused-function",
              "-Wno-unused-parameter", "-Wno-unused-variable", "-Wno-unused-label", "-Wno-unused-const-variable",
              "-Wno-unused-but-set-variable", "-Wno-missing-field-initializers", "-I.", "-Irelease",
              "-Irelease/include", "-I" + str(out / "include")]
    links = ["release/xl.dll"] if os.name == "nt" else ["-Lrelease", "-lxl", "-Wl,-rpath," + str(root / "release"), "-pthread", "-ldl", "-lm"]
    if os.name != "nt":
        strict += ["-D_GNU_SOURCE"]
    sanitized = ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"] if args.sanitize else []

    def emit(name: str, mode: str, target: Path) -> list[str]:
        source = f"test/cases/{name}.xl"
        if public:
            return [str(cli), source, mode, str(target)]
        if mode == "ir-lowered":
            return [str(cli), "ir", source, "--lowered", "--format", "json", "--emit", str(target)]
        return [str(cli), "cgen", source, "--inc-path", str(out / "include"), "--emit", str(target)]

    try:
        contract = out / ("metadata-contract" + (".exe" if os.name == "nt" else ""))
        if public:
            objects = [row["Path"] for row in public["Objects"]
                       if Path(row["Path"]).name != "xlir_public_source_driver.o"]
            assert len(objects) == len(public["Objects"]) - 1
            contract_links = objects + links + ["-lWs2_32", "-lIPHLPAPI", "-lShell32", "-lOle32", "-lAdvapi32"]
        else:
            contract_links = ["-Lrelease", "-lxlc"] + links
        run("metadata-contract-build", [cc] + strict + ["-O0", "test/cases/xlir_future_metadata_contract.c"] +
            contract_links + ["-o", str(contract)], 240)
        log = run("metadata-contract", [str(contract)])
        assert "15 fallible operations, 105 typed/lowered/C/header rejected mutations" in log
        memory_objects = {}
        for level in ("O0", "O2"):
            memory = out / ("memory-" + level + ".o")
            run("memory-build-" + level, [cc] + strict + ["-" + level] + sanitized + ["-c",
                "test/support/module_product_memory.c", "-o", str(memory)])
            memory_objects[level] = memory
            metadata = out / ("metadata-" + level + (".exe" if os.name == "nt" else ""))
            run("metadata-build-" + level, [cc] + strict + ["-" + level] + sanitized +
                ["test/cases/stdlib_future_metadata_host.c", "src/runtime/xl6_future_metadata.c",
                 "src/api/xl_future_bridge.c", str(memory)] + links + ["-o", str(metadata)], 240)
            log = run("metadata-" + level, [str(metadata)])
            assert "1000 owned exact-name lifetimes" in log and "100 cooperative accepted tasks balanced" in log
            for mode in range(7):
                for point in range(64):
                    log = run(f"metadata-{level}.mode{mode}.oom{point}", [str(metadata), str(mode), str(point)])
                    assert f"mode={mode} point={point}" in log and "balanced" in log
                    if "hit=0 balanced" in log:
                        report["MetadataSweeps"].append({"Lane": level, "Mode": mode, "FirstNoHit": point, "Complete": True})
                        break
                else:
                    raise RuntimeError("Incomplete metadata OOM prefix")
            binary = out / ("native-" + level + (".exe" if os.name == "nt" else ""))
            run("native-build-" + level, [cc] + strict + ["-" + level] + sanitized + [host,
                str(memory)] + resident + links + ["-o", str(binary)], 240)
            log = run("native-" + level, [str(binary)])
            assert "5 consumer families" in log and "guards balanced" in log
            for mode in range(13):
                for point in range(256):
                    log = run(f"native-{level}.mode{mode}.oom{point}", [str(binary), str(mode), str(point)])
                    assert f"mode={mode} point={point}" in log and "balanced" in log
                    if "hit=0 balanced" in log:
                        report["Sweeps"].append({"Lane": level, "Mode": mode, "FirstNoHit": point, "Complete": True})
                        break
                else:
                    raise RuntimeError("Incomplete Array input prefix")
            if os.name != "nt":
                module = out / ("owner-box-" + level + ".so")
                run("owner-module-build-" + level, [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-" + level,
                    "-g", "-fPIC", "-shared"] + (["-fsanitize=address,undefined"] if args.sanitize else []) +
                    ["test/cases/xlir_future_aggregate_code_owner_box.c", "-o", str(module)])
                owner = out / ("owner-" + level)
                run("owner-build-" + level, [cc] + strict + ["-" + level] + sanitized +
                    ["test/cases/xlir_future_aggregate_code_owner_runtime.c", "src/runtime/xl6_future_aggregate.c",
                     "src/runtime/xl6_future_aggregate_array.c", "src/runtime/xl6_array_input.c"] + links + ["-o", str(owner)], 240)
                log = run("owner-" + level, [str(owner), str(module)])
                assert "200 exact context-to-lease-to-owner chains" in log and "300 fully admitted physical graphs" in log
        for name in fixtures:
            target = out / (name + ".c")
            run(name + ".emit", emit(name, "cgen-default", target))
            assert "xl6FutureAggregateRuntimeV1Get" not in target.read_text(encoding="utf-8")
            if name.startswith("stdlib_future_aggregate_"):
                suffix = "ArrayV2" if "array_only" in name else "V2"
                code = target.read_text(encoding="utf-8")
                for stem in ("All", "Race", "WhenAnyIndex"):
                    assert f"xl6FutureAggregate{stem}{suffix}(" in code
            repeat = out / (name + ".repeat.c")
            run(name + ".repeat", emit(name, "cgen-default", repeat))
            assert target.read_bytes() == repeat.read_bytes()
            ir = out / (name + ".json")
            run(name + ".ir", emit(name, "ir-lowered", ir))
            assert "value-v1" in ir.read_text(encoding="utf-8")
            if name.startswith("stdlib_future_"):
                assert json.loads(ir.read_text(encoding="utf-8"))["futureAggregateAbi"] == (0 if "metadata_only" in name else 2)
            for level in ("O0", "O2"):
                binary = out / (name + "." + level + (".exe" if os.name == "nt" else ""))
                run(name + ".build-" + level, [cc] + strict + ["-" + level] + sanitized + ["-include", str(target),
                    "test/cases/xlir_module_product_oom_host.c", str(memory_objects[level])] + contract_links + ["-o", str(binary)], 600)
                run(name + ".run-" + level, [str(binary)])
            prefix = out / (name + ".product")
            if public:
                run(name + ".jit", [str(cli), f"test/cases/{name}.xl", "run", str(prefix)])
                run(name + ".aot", [str(cli), f"test/cases/{name}.xl", "exe", str(prefix)], 180)
            else:
                run(name + ".jit", [str(cli), "run", f"test/cases/{name}.xl", "--inc-path", str(out / "include")])
                run(name + ".aot-build", [str(cli), "build", f"test/cases/{name}.xl", "--inc-path", str(out / "include"), "-o", str(prefix)], 180)
                run(name + ".aot", [str(prefix)])
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        assert report["InputsUnchanged"] and len(report["Sweeps"]) == 26 and len(report["MetadataSweeps"]) == 14
        report["Complete"] = True
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Array input: {len(rows)} checks, 26 complete prefixes, 5 consumer families, frozen inputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
