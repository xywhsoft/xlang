"""Offline signed C result bounds, single calls, error precedence and OOM.

A partial frontend or public source cohort qualifies development behavior only.
Full compiler/TCC/VFS, native SDKs and shipping installation are separate gates.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time

from compiler_response import command_with_response
from foreign_compiler_cohort import load_compiler_cohort
from foreign_validation_build import load_build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    selected = parser.add_mutually_exclusive_group(required=True)
    selected.add_argument("--compiler", type=Path)
    selected.add_argument("--source-build", type=Path)
    parser.add_argument("--compiler-report", type=Path)
    parser.add_argument("--partial-frontend", action="store_true")
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    win = os.name == "nt"
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    objects = []
    if args.compiler:
        assert args.compiler_report
        driver = args.compiler.resolve()
        report_path = args.compiler_report.resolve()
        sdk, _ = load_compiler_cohort(root, driver, report_path, args.partial_frontend)
        metadata = json.loads(report_path.read_text())
        if args.partial_frontend:
            metadata = json.loads(Path(metadata["parent"]).read_text())
        cohort = Path(metadata["source"])
        runtime = driver.parent / ("xlc.dll" if win else "libxlc.so")
        pinned = {str(report_path): sha(report_path)}
    else:
        assert not args.compiler_report and not args.partial_frontend
        build = load_build(build=args.source_build)
        driver, runtime, cohort = build["driver"], build["runtime"], build["cohort"]
        sdk = Path(build["base"]["SdkDirectory"]) / "include"
        objects = build["objects"]
        pinned = dict(build["inputs"])
    cc = Path(shutil.which("gcc")).resolve()
    case = root / "test/cases/foreign_integer_min.xl"
    native = case.with_name("foreign_integer_min_native.c")
    errors = case.with_name("foreign_integer_min_errors.c")
    out.mkdir(parents=True, exist_ok=False)
    include = out / "include"
    shutil.copytree(sdk, include / "__xl_runtime")
    for header in (cohort / "src/runtime").rglob("*.h"):
        target = include / "__xl_runtime" / header.relative_to(cohort / "src/runtime")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(header, target)
    error_header = (root if args.partial_frontend else cohort) / "src/api/xl_foreign_error.h"
    shutil.copy2(error_header, include / "__xl_runtime/xl_foreign_error.h")
    if win:
        shutil.copy2(runtime, out / runtime.name)
    files = [Path(__file__), cc, driver, runtime, case, native, errors,
             case.with_suffix(".h"), root / "test/cases/foreign_decl_module_host.c",
             root / "lib/xrt/xrt.h", error_header]
    files += [p for p in include.rglob("*") if p.is_file()]
    pinned.update((str(p), sha(p)) for p in files)
    report = dict(complete=False, passed=False, scope=__doc__, inputs=pinned,
                  fullCompilerQualification=False, shippingDelivery=False, rows=[])
    env = dict(os.environ, TMP=str(out), TEMP=str(out), TMPDIR=str(out),
               XL6_SOURCE_NATIVE_ROOT=str(cohort))
    key = "PATH" if win else "LD_LIBRARY_PATH"
    env[key] = str(runtime.parent) + os.pathsep + env.get(key, "")

    def save():
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")

    def run(name, command, refused=False):
        command = list(map(str, command))
        actual = command_with_response(command, str(cc), out / (name + ".rsp"))
        start = time.monotonic()
        result = subprocess.run(actual, cwd=root, env=env, capture_output=True, timeout=360)
        raw = result.stdout + result.stderr
        log = out / (name + ".log")
        log.write_bytes(raw)
        report["rows"].append(dict(name=name, command=command, exitCode=result.returncode,
                                   refused=refused, seconds=time.monotonic()-start,
                                   logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert result.returncode == (1 if refused else 0), log
        if refused:
            assert b"[sem]" in raw, log

    generated = out / "generated.c"
    header = out / "generated.h"
    if args.compiler:
        run("cgen", [driver, "cgen", case, "--emit", generated, "--emit-header", header])
        run("ir", [driver, "ir", case, "--lowered", "--format", "json", "--emit", out/"ir.json"])
        run("jit", [driver, "run", case, native, "--inc-path", root])
    else:
        run("cgen", [driver, case, "cgen-default", generated])
        run("ir", [driver, case, "ir-lowered", out / "ir.json"])
        run("emit-header", [driver, case, "emit", out / "public-header", native])
        header = out / "public-header.h"
    assert "xl6ForeignIntegerFailureV1" in generated.read_text()
    includes = ["-I"+str(p) for p in (include, root, sdk)]
    warnings = ["-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-label",
                "-Wno-unused-but-set-variable", "-Wno-unused-const-variable"]
    systems = [] if win else ["-Wl,--export-dynamic", "-pthread", "-ldl", "-lm",
                              "-Wl,-rpath," + str(runtime.parent)]
    for standard, language in (("c11", "c"), ("c++11", "c++"), ("c++17", "c++")):
        run("header-"+standard, [cc, "-std="+standard, "-Wall", "-Wextra", "-Werror",
                                "-x", language, "-fsyntax-only", *includes, header])
    for opt in ("O0", "O2"):
        binary = out / (opt + (".exe" if win else ""))
        flags = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-"+opt,
                 "-DXL_STATIC_EMBED", *warnings, *includes]
        run(opt+"-build", [*flags, generated, native, root/"test/cases/foreign_decl_module_host.c",
                           *objects, runtime, *systems, "-o", binary])
        run(opt+"-run", [binary])
        diagnostic = out / ("errors-"+opt + (".exe" if win else ""))
        run(opt+"-errors-build", [*flags, errors, *objects, runtime, *systems, "-o", diagnostic])
        run(opt+"-errors", [diagnostic])
    for name, result_type in (("unsigned", "c_uint"), ("floating", "c_double"),
                              ("boolean", "bool"), ("nullable", "c_int?"), ("language-int", "int")):
        negative = out / (name + ".xl")
        negative.write_text('extern "probe" @c_result("integer_min",0) func probe() as '+result_type+"\n")
        command = ([driver, "cgen", negative, "--emit", out/(name+".c")] if args.compiler else
                   [driver, negative, "cgen-default", out/(name+".c")])
        run("reject-"+name, command, refused=True)
    report.update(complete=True, inputsUnchanged=all(sha(p)==h for p,h in pinned.items()))
    report["passed"] = report["inputsUnchanged"]
    save()
    assert report["passed"]


if __name__ == "__main__":
    main()
