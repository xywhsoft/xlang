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
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
    args = parser.parse_args()
    frontend, output = args.frontend_root.resolve(), args.output.resolve()
    cc = Path(args.cc).resolve()
    if not cc.is_file():
        parser.error(f"C compiler does not exist: {cc}")
    sources = [(frontend / source, False) for source in FRONTEND_SOURCES]
    sources += [(ide / "src/analysis/runtime.c", False)]
    sources += [(ide / path, True) for path in (
        "src/analysis/text.c", "src/analysis/session.c", "src/analysis/worker.c", "src/protocol/document_sync.c",
        "tests/analysis_worker_test.c", "tests/analysis_session_test.c", "tests/analysis_text_test.c", "tests/document_sync_test.c",
    )]
    if any(not path.is_file() for path, _ in sources):
        parser.error("frontend root must contain the shared xlang lexer, parser, AST and XRT assets")
    output.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ)
    env["PATH"] = str(cc.parent) + os.pathsep + env.get("PATH", "")
    common = ["-std=c11", "-O2", "-ffunction-sections", "-fdata-sections", "-DXRT_MODULE_ALL=",
              "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
              "-I" + str(ide / "src"), "-I" + str(frontend),
              "-I" + str(frontend / "release"), "-I" + str(frontend / "release/include"),
              "-I" + str(frontend / "lib/xrt"), "-include", str(frontend / "lib/xrt/xrt.h")]
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
    commands, inputs, outputs, runs = [], {str(Path(__file__).resolve()): sha(Path(__file__))}, {}, []
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
        members = [target for name, target in objects.items() if not name.endswith("_test") and name != "document_sync"]
        run([str(ar), "rcs", str(core), *map(str, members)], "archive")
        suffix = ".exe" if os.name == "nt" else ""
        for name in ("analysis_worker_test", "analysis_session_test", "analysis_text_test", "document_sync_test"):
            program = output / (name + suffix)
            extra = [str(objects["document_sync"])] if name == "document_sync_test" else []
            run([str(cc), str(objects[name]), *extra, str(core), "-Wl,--gc-sections", *libraries, "-o", str(program)], name + "-link")
            result = run([str(program)], name + "-run", 30)
            if b"passed:" not in result.stdout:
                raise RuntimeError(f"missing test completion receipt: {name}")
            runs.append(dict(name=name,stdout=result.stdout.decode("utf-8")))
            outputs[program.name] = sha(program)
        if any(sha(Path(path)) != digest for path, digest in inputs.items()):
            raise RuntimeError("source or toolchain changed during the build")
        outputs[core.name] = sha(core)
        status = "passed"
    finally:
        report = dict(schema=1,status=status,scope="standalone retained syntax core; not shipped LSP or semantic incrementality",
                      frontendRoot=str(frontend),compiler=str(cc),compilerSha256=sha(cc),inputs=inputs,outputs=outputs,
                      commands=commands,runs=runs,elapsedSeconds=time.monotonic()-started)
        (output / "build-result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(dict(status=status,report=str(output / "build-result.json")),ensure_ascii=False))


if __name__ == "__main__":
    main()
