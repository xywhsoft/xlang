"""Build the standalone analysis core directly from shared xlang source assets.

No compiler/SDK files are copied or changed. This target owns compiler XRT and
must not be linked into the XGE-owned IDE. It does not replace the shipped LS.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

FRONTEND_SOURCES = (
    "src/lexer/xl6_token.c", "src/lexer/xl6_lexer_api.c", "src/lexer/xl6_lexer_gen.c",
    "src/parser/xl6_parser.c", "src/parser/xl6_parser_api.c", "src/ast/xl6_ast.c",
    "src/diag/xl6_diag.c", "src/common/xl6_output.c", "src/common/xl6_console.c",
    "src/sem/xl6_completion.c", "src/compile/xl6_source_report.c",
    "src/compile/xl6_imports.c", "src/compile/xl6_source_overlay.c", "src/compile/xl6_build_inputs.c",
    "src/package/xl6_package.c", "src/stdlib/xl6_stdlib.c", "src/stdlib/xl6_stdlib_data.c",
    "src/type/xl6_type_model.c", "src/sem/xl6_protocol.c", "src/sem/xl6_type_registry.c",
    "src/sem/xl6_sem_docs.c", "src/sem/xl6_sem_field_attrs.c", "src/sem/xl6_sem.c", "src/abi/xl6_callable.c",
    "src/sem/xl6_sem_index.c", "src/ir/xl6_ir.c", "src/ir/xl6_ir_class_identity.c",
)
SERVER_SOURCES = (
    "src/protocol/lsp_wire.c", "src/services/language_check.c", "src/services/language_snapshot.c",
    "src/services/semantic_definition.c", "src/services/semantic_completion.c", "src/services/c_bridge_map.c",
    "src/services/toolchain.c", "src/platform/file_rename.c", "src/platform/file_io.c",
    "src/platform/text_file.c", "src/platform/process.c", "src/platform/path.c", "src/editor/coordinates.c",
    "src/server/xlang_ls.c",
)
CORE_TESTS = ("analysis_check_test", "analysis_check_session_test", "analysis_check_worker_test", "analysis_index_test", "analysis_index_limit_test", "analysis_semantic_test", "analysis_folding_test", "analysis_worker_test", "analysis_session_test", "analysis_incremental_syntax_test", "analysis_text_test", "document_sync_test")
HOST_TESTS = ("analysis_index_view_test",)
NATIVE_INDEX_BYTES = 32 * 1024 * 1024


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare_native_index(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Expose the shared collector and parameterize its limit, keeping upstream immutable."""
    original = frontend / "src/sem/xl6_sem_index.c"
    body = original.read_bytes()
    source = body.decode("utf-8").replace("\r\n", "\n")
    definition = "#define XL6_INDEX_BYTES (8u * 1024u * 1024u)"
    if source.count(definition) != 1:
        raise ValueError("shared index budget declaration changed; audit its native build overlay")
    generated_root = output / "frontend"
    generated_root.mkdir()
    compiled = generated_root / "xl6_sem_index.c"
    compiled.write_bytes((source.replace(definition,
        "#ifndef XL6_INDEX_BYTES\n" + definition + "\n#endif") +
        '\n#include "analysis/frontend_index.inc"\n').encode("utf-8"))
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        change="configure existing index budget and include typed collector views", indexBytes=NATIVE_INDEX_BYTES)


def dependencies(body: str) -> list[Path]:
    """Read a GCC -MM rule with a fixed, drive-independent -MT target."""
    body = body.replace("\r\n", "\n").replace("\\\n", "").split(":", 1)[1]
    words, current, at = [], [], 0
    while at < len(body):
        char = body[at]
        if char == "\\" and at + 1 < len(body) and body[at + 1] in " \t\\#:":
            at += 1
            current.append(body[at])
        elif char.isspace():
            if current:
                words.append("".join(current))
                current = []
        else:
            current.append(char)
        at += 1
    if current:
        words.append("".join(current))
    return [Path(word).resolve() for word in words]


def main() -> None:
    ide = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend-root", type=Path, default=ide.parent / "demo6")
    parser.add_argument("--output", type=Path, required=True, help="fresh build directory")
    parser.add_argument("--cc", default=shutil.which("gcc") or "E:/software/w64devkit/bin/gcc.exe")
    parser.add_argument("--server", action="store_true", help="also build the external language-server host")
    parser.add_argument("--skip-tests", action="store_true", help="build only, without compiling or running core fixtures")
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    args = parser.parse_args()
    tests = CORE_TESTS + (HOST_TESTS if args.server else ())
    frontend, output = args.frontend_root.resolve(), args.output.resolve()
    cc = Path(args.cc).resolve()
    if not cc.is_file():
        parser.error(f"C compiler does not exist: {cc}")
    sources = [(frontend / source, False) for source in FRONTEND_SOURCES]
    sources += [(ide / "src/analysis/runtime.c", False)]
    sources += [(ide / path, True) for path in (
        "src/analysis/text.c", "src/analysis/syntax.c", "src/analysis/session.c", "src/analysis/worker.c", "src/analysis/folding.c", "src/analysis/semantic.c", "src/analysis/check.c", "src/analysis/check_session.c",
    )]
    if not args.skip_tests:
        sources += [(ide / "src/protocol/document_sync.c", True)]
        sources += [(ide / "tests" / (name + ".c"), True) for name in tests]
    if args.server:
        if os.name != "nt":
            parser.error("the current host still uses Windows transport/process adapters; the standalone core is separate")
        sources += [(ide / path, True) for path in SERVER_SOURCES]
    if any(not path.is_file() for path, _ in sources):
        parser.error("frontend root must contain the shared xlang lexer, parser, AST and XRT assets")
    output.mkdir(parents=True, exist_ok=False)
    # Keep the pinned compiler assets immutable. This checked build overlay
    # makes the existing private serializer budget configurable; no grammar,
    # fact collection or index algorithm is duplicated in the IDE.
    try:
        original_index, generated_index, index_overlay = prepare_native_index(frontend, output)
    except ValueError as error:
        parser.error(str(error))
    sources = [(generated_index if path == original_index else path, owned) for path, owned in sources]
    env = dict(os.environ)
    env["PATH"] = str(cc.parent) + os.pathsep + env.get("PATH", "")
    common = ["-std=c11", "-ffunction-sections", "-fdata-sections", "-DXRT_MODULE_ALL=", "-DXLS_COMPILER_XRT=1",
              "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
              "-I" + str(ide / "src"), "-I" + str(frontend),
              "-I" + str(frontend / "release"), "-I" + str(frontend / "release/include"),
              "-I" + str(frontend / "lib/xrt"), "-include", str(frontend / "lib/xrt/xrt.h")]
    # MODULE_ALL also selects memory-debug pointer validation. Its release
    # path scans heap ownership when freeing large JSON trees. Select the
    # library's existing release policy consistently in EVERY translation unit;
    # Debug keeps the diagnostic allocator. This does not change analysis or
    # source/provenance validation and does not touch the XGE-owned GUI runtime.
    common += ["-O2", "-DXRT_EXCLUDE_MEMORY_DEBUG="] if args.configuration == "Release" else ["-O0", "-g"]
    index_budget_flag = "-DXL6_INDEX_BYTES=" + str(NATIVE_INDEX_BYTES) + "u"
    common += [index_budget_flag]
    strict = ["-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter"]
    shared = ["-Wall", "-Wno-pointer-sign", "-Wno-unused-function", "-Wno-unused-variable",
              "-Wno-unused-but-set-variable", "-Wno-format-truncation", "-Wno-dangling-pointer"]
    libraries = ["-lm"]
    if os.name == "nt":
        libraries += ["-lws2_32", "-lwinhttp", "-liphlpapi", "-lgdi32", "-luser32", "-lshell32",
                      "-lole32", "-lwinmm", "-lavrt", "-lversion", "-lbcrypt", "-ladvapi32"]
    else:
        common += ["-D_GNU_SOURCE=1"]
        libraries += ["-pthread", "-ldl"]
    commands, inputs, outputs, runs, imports = [], {str(Path(__file__).resolve()): sha(Path(__file__)),
        str(original_index): index_overlay["sourceSha256"]}, {}, [], {}
    source_overlays = [index_overlay]
    runtime_profiles = []
    started = time.monotonic()
    status = "failed"

    def run(command: list[str], name: str, timeout: int = 180) -> subprocess.CompletedProcess:
        commands.append(command)
        result = subprocess.run(command, env=env, cwd=ide, capture_output=True, timeout=timeout)
        (output / (name + ".log")).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{name} failed ({result.returncode}); see {output / (name + '.log')}")
        return result

    try:
        probe_sources = [ide / "src/analysis/runtime.c", frontend / "src/ast/xl6_ast.c"]
        if args.server:
            probe_sources.append(ide / "src/server/xlang_ls.c")
        for source in probe_sources:
            result = run([str(cc), *common, "-dM", "-E", str(source)], source.stem + "-runtime-features")
            features = {line.split()[1] for line in result.stdout.decode("utf-8").splitlines()
                        if line.startswith("#define ") and len(line.split()) > 1}
            debug_memory = "XRT_FEATURE_MEMORY_DEBUG" in features
            if debug_memory != (args.configuration == "Debug") or not {"XRT_MODULE_ALL", "XRT_FEATURE_VALUE"} <= features:
                raise RuntimeError("inconsistent native XRT runtime profile: " + str(source))
            runtime_profiles.append(dict(source=str(source), memoryDebug=debug_memory,
                macroSha256=hashlib.sha256(result.stdout).hexdigest()))
        for source, owned in sources:
            flags = common + (strict if owned else shared)
            result = run([str(cc), *flags, "-MM", "-MT", "xlsObject", str(source)], source.stem + "-deps")
            for path in dependencies(result.stdout.decode("utf-8")):
                digest = sha(path)
                if str(path) in inputs and inputs[str(path)] != digest:
                    raise RuntimeError(f"source changed during dependency discovery: {path}")
                inputs[str(path)] = digest
        inputs[str(cc)] = sha(cc)
        objects = {}
        for source, owned in sources:
            target = output / (source.stem + ".o")
            run([str(cc), *common, *(strict if owned else shared), "-c", str(source), "-o", str(target)], source.stem)
            objects[source.stem] = target
        ar = cc.with_name("ar.exe" if os.name == "nt" else "ar")
        core = output / "xlang-analysis-core.a"
        core_names = {Path(source).stem for source in FRONTEND_SOURCES} | {"runtime", "text", "syntax", "session", "worker", "folding", "semantic", "check", "check_session"}
        members = [target for name, target in objects.items() if name in core_names]
        run([str(ar), "rcs", str(core), *map(str, members)], "archive")
        limited_index = output / "xl6_sem_index_limit.o"
        if not args.skip_tests:
            limited_flags = [flag for flag in common if flag != index_budget_flag]
            run([str(cc), *limited_flags, "-DXL6_INDEX_BYTES=1024u", *shared, "-c", str(generated_index),
                 "-o", str(limited_index)], "index-limit-fixture")
        suffix = ".exe" if os.name == "nt" else ""
        for name in (() if args.skip_tests else tests):
            program = output / (name + suffix)
            extra = [str(objects["document_sync"])] if name == "document_sync_test" else []
            if name == "analysis_index_limit_test":
                extra.append(str(limited_index))
            if name in HOST_TESTS:
                extra += [str(objects[Path(source).stem]) for source in SERVER_SOURCES
                          if Path(source).stem != "xlang_ls"]
            run([str(cc), str(objects[name]), *extra, str(core), "-Wl,--gc-sections", *libraries, "-o", str(program)], name + "-link")
            result = run([str(program)], name + "-run", 30)
            if b"passed:" not in result.stdout:
                raise RuntimeError(f"missing test completion receipt: {name}")
            runs.append(dict(name=name,stdout=result.stdout.decode("utf-8")))
            outputs[program.name] = sha(program)
        if args.server:
            program = output / ("xlang-ls" + suffix)
            server_objects = [str(objects[Path(source).stem]) for source in SERVER_SOURCES]
            run([str(cc), *server_objects, str(core), "-Wl,--gc-sections", *libraries, "-o", str(program)], "server-link")
            outputs[program.name] = sha(program)
        if os.name == "nt":
            for name in outputs:
                result = run([str(cc.with_name("objdump.exe")), "-p", str(output / name)], name + "-imports")
                names = [line.split("DLL Name:",1)[1].strip() for line in result.stdout.decode().splitlines() if "DLL Name:" in line]
                if not names or any(name.lower() in ("xge.dll", "xlc.dll") for name in names):
                    raise RuntimeError("standalone runtime ownership failed: " + name)
                imports[name] = names
        if any(sha(Path(path)) != digest for path, digest in inputs.items()):
            raise RuntimeError("source or toolchain changed during the build")
        outputs[core.name] = sha(core)
        status = "passed"
    finally:
        report = dict(schema=1,status=status,scope="standalone syntax and native semantic core; not shipped LSP or semantic incrementality",
                      frontendRoot=str(frontend),compiler=str(cc),compilerSha256=sha(cc),inputs=inputs,outputs=outputs,
                      configuration=args.configuration,serverBuilt=args.server,testsBuilt=not args.skip_tests,
                      commands=commands,runs=runs,imports=imports,elapsedSeconds=time.monotonic()-started)
        report["sourceOverlays"] = source_overlays
        report["runtimeProfiles"] = runtime_profiles
        (output / "build-result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(dict(status=status,report=str(output / "build-result.json")),ensure_ascii=False))


if __name__ == "__main__":
    main()
