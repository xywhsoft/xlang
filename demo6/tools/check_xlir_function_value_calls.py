#!/usr/bin/env python3
"""Exact expression-call signatures: semantic negatives, strict C/JIT/AOT and real OOM.

Linux sanitizers instrument generated code/hosts, not the shared runtime. Each
OOM prefix runs the real main entry and verifies the actual runtime heap ledger.
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
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc = shutil.which("gcc")
    if not cc or out.exists() or (os.name == "nt" and not args.public_directory) or (args.sanitize and os.name == "nt"):
        parser.error("Require gcc, fresh output, Windows source driver and Linux for sanitizers")
    public = json.loads((args.public_directory / "build.json").read_text(encoding="utf-8-sig")) if args.public_directory else None
    if public and (public["RuntimeOnly"] or not public["SourceXrt"]):
        parser.error("Require a full current-source compiler")
    staged = json.loads((args.public_directory / "artifact_sources.json").read_text(encoding="utf-8-sig")) if public else None
    cli = Path(public["Driver"]) if public else root / "release/xl"
    fixture = "test/cases/xlir_function_value_calls.xl"
    regressions = ["xlir_callable_contextual_conversion", "xlir_named_callable_owner_lifetime",
                   "xlir_callable_coalesce_expression", "stdlib_callable_scalars", "stdlib_callable_integer_domains"]
    files = [Path(__file__), Path(cc), Path(sys.executable), cli, root / fixture,
             root / "test/cases/xlir_function_value_calls_contract.c",
             root / "test/cases/stdlib_collection_admission_host.c",
             root / "test/cases/xlir_module_product_oom_host.c",
             root / "test/support/module_product_memory.c", root / "test/support/module_product_memory.h"]
    files += [root / f"test/cases/{name}.xl" for name in regressions]
    for directory in ("src/sem", "src/lexer", "src/parser", "src/ir", "src/cgen", "src/compile", "src/runtime", "release/include"):
        files += [p for p in (root / directory).rglob("*") if p.suffix in (".c", ".h", ".inc")]
    if public:
        if digest(cli) != public["DriverSha256"].lower():
            parser.error("Source compiler binary changed")
        for manifest in (public, staged):
            for row in (manifest.get("Inputs", []) + manifest.get("Headers", []) +
                        manifest.get("Objects", []) + manifest.get("Files", [])):
                if "Sha256" in row and digest(Path(row["Path"])) != row["Sha256"].lower():
                    parser.error("Stale source/staged manifest: " + row["Path"])
        for row in staged["Files"]:
            if digest(Path(row["Source"])) != row["Sha256"].lower():
                parser.error("Staged source changed: " + row["Source"])
        files += [args.public_directory / "build.json", args.public_directory / "artifact_sources.json",
                  root / "release/xl.dll", args.public_directory / "xl.dll"]
        files += [Path(row["Path"]) for row in public["Objects"] + public["Inputs"] + staged["Files"]]
    else:
        files += [root / "release/libxl.so", root / "release/libxlc.so"]
    inputs = [{"Path": str(p.resolve()), "Sha256": digest(p)} for p in sorted(set(files))]
    out.mkdir(parents=True)
    private = out / "include/__xl_runtime"
    private.mkdir(parents=True)
    for directory in (root / "src/runtime", root / "release/include"):
        for header in directory.glob("*.h"):
            shutil.copy2(header, private / header.name)
    if public:
        shutil.copy2(root / "release/xl.dll", out / "xl.dll")
    rows: list[dict] = []
    report = {"Scope": __doc__, "Complete": False, "InputsUnchanged": False,
              "Platform": sys.platform, "Sanitized": args.sanitize, "Inputs": inputs,
              "Rows": rows, "Sweeps": []}
    env = dict(os.environ)
    if args.sanitize:
        env["ASAN_OPTIONS"] = "detect_leaks=1:abort_on_error=1"
        env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    def run(name: str, command: list[str], timeout: int = 60) -> str:
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out / (name + ".log")).write_bytes((error.stdout or b"") + (error.stderr or b""))
            rows.append({"Name": name, "Command": command, "Exit": None, "Complete": False, "TimedOut": True})
            raise
        text = result.stdout + result.stderr
        (out / (name + ".log")).write_bytes(text)
        rows.append({"Name": name, "Command": command, "Exit": result.returncode,
                     "Complete": result.returncode == 0, "Seconds": time.monotonic() - start})
        if result.returncode:
            raise RuntimeError(name + ": " + text[-5000:].decode(errors="replace"))
        return text.decode(errors="replace")

    def emit(mode: str, path: Path) -> list[str]:
        if public:
            return [str(cli), fixture, mode, str(path)]
        if mode == "ir-lowered":
            return [str(cli), "ir", fixture, "--lowered", "--format", "json", "--emit", str(path)]
        return [str(cli), "cgen", fixture, "--emit", str(path)]

    strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-unused-label",
              "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-unused-const-variable", "-Wno-missing-field-initializers",
              "-I.", "-Irelease", "-Irelease/include", "-I" + str(out / "include")]
    links = ["release/xl.dll"] if public else ["-Lrelease", "-lxl", "-Wl,-rpath," + str(root / "release"), "-pthread", "-ldl", "-lm"]
    if not public:
        strict += ["-D_GNU_SOURCE"]
    if args.sanitize:
        strict += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    suffix = ".exe" if public else ""
    try:
        compiler_links = ([row["Path"] for row in public["Objects"] if Path(row["Path"]).name != "xlir_public_source_driver.o"] +
                          [*links, "-lws2_32", "-liphlpapi", "-lshell32", "-lole32", "-ladvapi32"] if public else ["-lxlc", *links])
        contract = out / ("contract" + suffix)
        run("contract.build", [cc, "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror", "-DXL_STATIC_EMBED", "-I.",
            "-Irelease", "-Irelease/include", "-Ilib/tcc", "test/cases/xlir_function_value_calls_contract.c", *compiler_links, "-o", str(contract)])
        prefix = out / "function-value-calls"
        generated, ir = prefix.with_suffix(".c"), prefix.with_suffix(".json")
        for repeat in (False, True):
            run(f"emit.{repeat}", emit("cgen-default", prefix.with_suffix(".repeat.c") if repeat else generated))
            run(f"ir.{repeat}", emit("ir-lowered", prefix.with_suffix(".repeat.json") if repeat else ir))
        assert generated.read_bytes() == prefix.with_suffix(".repeat.c").read_bytes()
        assert ir.read_bytes() == prefix.with_suffix(".repeat.json").read_bytes()
        proof = run("contract", [str(contract), str(prefix.with_suffix(".contract.c")),
            str(prefix.with_suffix(".h")), str(prefix.with_suffix(".contract.json"))])
        match = re.search(r"function-value calls: (\d+) semantic rejections, (\d+) exact calls, (\d+) products", proof)
        assert match and int(match[1]) == 64 and int(match[2]) >= 15 and int(match[3]) >= 14
        report["Contract"] = {"SemanticRejections": int(match[1]), "ExactCalls": int(match[2]), "Products": int(match[3])}
        assert generated.read_bytes() == prefix.with_suffix(".contract.c").read_bytes()
        assert ir.read_bytes() == prefix.with_suffix(".contract.json").read_bytes()
        for level in (0, 2):
            binary = prefix.with_suffix(f".O{level}" + suffix)
            run(f"O{level}.build", [cc, *strict, f"-O{level}", str(generated),
                "test/cases/stdlib_collection_admission_host.c", *links, "-o", str(binary)], timeout=180)
            run(f"O{level}.run", [str(binary)])
            memory = out / f"O{level}.memory.o"
            run(f"O{level}.memory.build", [cc, *strict, f"-O{level}", "-c", "test/support/module_product_memory.c", "-o", str(memory)])
            module = out / (f"O{level}.module" + suffix)
            run(f"O{level}.module.build", [cc, *strict, f"-O{level}", "-DXL6_PRODUCT_USER_ENTRY", "-include", str(generated),
                "test/cases/xlir_module_product_oom_host.c", str(memory), *links, "-o", str(module)], timeout=180)
            for point in range(4097):
                proof = run(f"O{level}.oom.{point}", [str(module), str(point)])
                assert re.search(r"hit=[01] balanced", proof)
                if "hit=0 balanced" in proof:
                    report["Sweeps"].append({"Lane": f"O{level}", "FirstNoHit": point})
                    break
            else:
                raise AssertionError("Allocation sweep did not close")
        run("jit", [str(cli), fixture, "run", str(prefix)] if public else [str(cli), "run", fixture])
        product = prefix.with_suffix(".exe" if public else ".product")
        run("aot.build", [str(cli), fixture, "exe", str(prefix)] if public else [str(cli), "build", fixture, "--inc-path", str(out / "include"), "-o", str(product)])
        assert product.is_file()
        run("aot.run", [str(product)])
        for name in regressions:
            source, prefix = f"test/cases/{name}.xl", out / name
            run(name + ".jit", [str(cli), source, "run", str(prefix)] if public else [str(cli), "run", source])
            product = prefix.with_suffix(".exe" if public else ".product")
            run(name + ".aot.build", [str(cli), source, "exe", str(prefix)] if public else
                [str(cli), "build", source, "--inc-path", str(out / "include"), "-o", str(product)])
            assert product.is_file()
            run(name + ".aot.run", [str(product)])
        report["InputsUnchanged"] = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
        report["Complete"] = report["InputsUnchanged"]
        assert report["Complete"], "Frozen inputs changed"
    except Exception as error:
        report["Error"] = f"{type(error).__name__}: {error}"
        print(error, file=sys.stderr)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Function-value calls: {len(rows)} checks, complete={report['Complete']}")
    return 0 if report["Complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
