#!/usr/bin/env python3
"""Frozen real native class V3 runtime, not a compiler or JIT substitute.

Build the actual selected XRT single implementation plus resident Class,
CodeLease and error-boundary sources. Check unloadable schema DLLs/SOs at O0/O2;
Linux additionally instruments that entire selected closure with ASan/UBSan/LSan.
Generated-language V3 lowering/checked CFG is deliberately a separate gate.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--compiler", default="gcc")
    args = parser.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc = shutil.which(args.compiler)
    if not cc or out.exists():
        parser.error("Require a compiler and a fresh evidence directory")
    files = {Path(__file__), Path(cc), Path(sys.executable)}
    sources = ["test/cases/xlir_class_identity_runtime_contract.c", "src/runtime/xl6_code_finalizer.c",
               "src/runtime/xl6_code_lease.c", "src/api/xl_foreign_error.c"]
    files.update(root / p for p in sources + ["test/cases/xlir_class_identity_schema_dll.c",
        "src/api/xl_foreign_error.h", "src/runtime/xl6_code_owner.h", "src/runtime/xl6_code_lease.h",
        "src/runtime/xl6_ownership_scope.h", "lib/xrt/xrt.h"])
    inputs = [{"Path": str(p), "Sha256": digest(p)} for p in sorted(files)]
    out.mkdir(parents=True)
    runtime = out / "runtime.c"
    runtime.write_text("#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_VALUE_GRAPH\n"
                       "#define XRT_MODULE_ATOMIC\n#define XRT_MODULE_THREAD\n#define XRT_MODULE_ONCE\n"
                       "#define XRT_IMPLEMENTATION\n#include \"lib/xrt/xrt.h\"\n", encoding="utf-8")
    levels = [("O0", ["-O0"]), ("O2", ["-O2"])]
    if "tcc" in Path(cc).name.lower():
        levels = [("TCC", [])]
    if os.name != "nt":
        levels.append(("san", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]))
    rows = []
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    common = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I."] if "tcc" not in Path(cc).name.lower() else [cc, "-I."]
    libs = [] if os.name == "nt" else ["-pthread", "-lm", "-ldl"]
    def execute(name: str, command: list[str]) -> bool:
        process = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        (out / (name + ".log")).write_bytes(process.stdout)
        marker = "Resident class identity V3: 32 real module lifetimes/unmaps,"
        complete = process.returncode == 0 and (not name.endswith(".run") or marker.encode() in process.stdout)
        rows.append({"Name": name, "Command": command, "Exit": process.returncode, "Complete": complete})
        print(f"{name}: {process.returncode}", flush=True)
        if not complete:
            print(process.stdout.decode("utf-8", errors="replace")[-4000:], flush=True)
        return complete
    for name, flags in levels:
        libraries = []
        for number, tag in enumerate((17, 29)):
            library = out / (name + f".schema{number}" + (".dll" if os.name == "nt" else ".so"))
            command = common + flags + (["-fPIC"] if os.name != "nt" else []) + ["-shared",
                f"-DCLASS_SCHEMA_TAG={tag}", "test/cases/xlir_class_identity_schema_dll.c", "-o", str(library)]
            if not execute(name + f".schema{number}", command): break
            libraries.append(str(library))
        if len(libraries) != 2: break
        executable = out / (name + (".exe" if os.name == "nt" else ".host"))
        pie = ["-fno-pie", "-no-pie"] if name == "san" else []
        if not execute(name + ".build", common + flags + pie + sources + [str(runtime)] + libs + ["-o", str(executable)]): break
        if not execute(name + ".run", [str(executable)] + libraries): break
    unchanged = all(digest(Path(row["Path"])) == row["Sha256"] for row in inputs)
    complete = unchanged and len(rows) == 4 * len(levels) and all(row["Complete"] for row in rows)
    artifacts = [{"Path": str(p), "Sha256": digest(p)} for p in sorted(out.iterdir())
                 if p.suffix in (".exe", ".host", ".dll", ".so", ".c")]
    report = {"Complete": complete, "InputsUnchanged": unchanged, "Inputs": inputs, "Rows": rows,
              "Artifacts": artifacts, "Scope": __doc__, "GeneratedLanguageV3": False}
    (out / "results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return 0 if complete else 1


if __name__ == "__main__":
    raise SystemExit(main())
