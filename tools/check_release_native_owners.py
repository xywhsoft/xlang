"""Release gate for builtin native owners; all probes are local and offline.

Inspect the compiler's actual lowered contracts, then exercise global owner
retirement and the public SDK's bounded drain. No unknown handle is certified
as a leaf by this test, and known failures are never converted into passes.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

HOST = r'''
#include "xl.h"
#include <stdio.h>
int main(int argc, char** argv) {
    if (argc!=3 || !xlRuntimeInit()) return 2;
    XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir=argv[2];
    for (unsigned iteration=0;iteration<20;++iteration) {
    XL_Context* context=xlCreateContext(&options);
    if (!context) return 2;
    int ok=xlRunScriptFile(context,argv[1]) && xlGetLastExitCode(context)==0;
    if (!ok) fprintf(stderr,"%s\n",xlGetLastError(context));
    xlDestroyContext(context);
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(5000);
    printf("{\"run\":%d,\"ready\":%d,\"ok\":%d}\n",ok,drained.ready,drained.ok);
    if (!ok || !drained.ready || !drained.ok) return 1;
    }
    xlRuntimeUnit(); return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--skip-sdk", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    products = (args.products or root / "release").resolve()
    xl = (args.compiler or products / ("xl.exe" if os.name == "nt" else "xl")).resolve()
    directory = (args.directory or root / "build/test/release-native-owners").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (root / "build/test").mkdir(parents=True, exist_ok=True)
    # The handler ownership fixture loads this child through a nested compiler.
    # It is an executed dependency, rather than an independently run script.
    handler_child = root / "test/cases/stdlib_xlang_handler_child.xl"
    if not handler_child.is_file():
        raise FileNotFoundError(handler_child)
    env = dict(os.environ)
    env["PATH"] = str(products) + os.pathsep + env.get("PATH", "")
    env["XLANG_PACKAGE_PATH"] = str(root / "release/extlibs")
    report = {"complete": False, "ready": False, "compilerSha256": hashlib.sha256(xl.read_bytes()).hexdigest(),
              "missingPolicies": [], "probes": []}

    def run(name, command, expected_finalizers=0):
        marker = root / "build/test/log-owner-retired.txt"
        before = marker.read_bytes() if expected_finalizers and marker.exists() else b""
        try:
            result = subprocess.run(list(map(str, command)), cwd=root, env=env,
                                    capture_output=True, timeout=120)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, data = "timeout", (error.stdout or b"") + (error.stderr or b"")
        if code == 0 and expected_finalizers:
            after = marker.read_bytes() if marker.exists() else b""
            if after != before + b"retired\n" * expected_finalizers:
                code = "missing-finalizer"
                data += f"\nExpected exactly {expected_finalizers} LogOwner finalizer records.\n".encode("utf-8")
        (directory / (name + ".log")).write_bytes(data)
        report["probes"].append({"name": name, "exit": code, "command": list(map(str, command)),
                                "expectedFinalizers": expected_finalizers})
        print(name, code, flush=True)
        return code == 0

    types = ["ConsoleSession", "ConsoleEvent", "JsonWriter", "XlonWriter", "XLangHandler",
             "TreeStats", "WalkStats", "LogRecord", "LogFileStats", "LogQueueStats", "LogStats", "LogSink", "Logger",
             "PathParts", "PathIterator", "SipHasher64", "AsyncFile"]
    source = directory / "owner_contracts.xl"
    source.write_text("import xlang\n" + "".join(f"{name}? owner{i} = null\n" for i, name in enumerate(types)), encoding="utf-8")
    lowered = directory / "owner_contracts.json"
    if run("lowered-owner-contracts", [xl, "ir", source, "--lowered", "--json", "--emit", lowered]):
        ir = json.loads(lowered.read_text(encoding="utf-8"))
        names = {row["id"]: row.get("name", "") for row in ir["types"]}
        targets = {row["symbol"]: row for row in ir["callTargets"]}
        seen = set()
        for field in ir["classFields"]:
            symbol = field.get("nativeDestroySymbol", 0)
            if not symbol:
                continue
            target = targets.get(symbol)
            if target is None:
                raise RuntimeError("native field has no lowered destructor target")
            if target.get("nativeHandlePolicyAbi") != 1 or not target.get("nativeHandlePolicyCName"):
                key = target["cName"]
                if key not in seen:
                    seen.add(key)
                    report["missingPolicies"].append({"type": names.get(field["ownerType"], ""), "destructor": key})
    for name, program in {
        "json-writer": 'JsonWriter writer = JsonWriter()\nassert writer.beginArray()\nassert writer.endArray()\nassert writer.finish() == "[]"\n',
        "xlon-writer": 'XlonWriter writer = XlonWriter()\nassert writer.beginArray()\nassert writer.endArray()\nassert writer.finish() == "[]"\n',
        "logger": 'auto logger = log.create("preflight")\nassert logger.name == "preflight"\n',
        "log-record": 'auto record = log.record(LogLevel.info, "preflight")\nassert record.message == "preflight"\n',
        "log-cycle": (root / "test/cases/stdlib_log_ownership_cycle.xl").read_text(encoding="utf-8-sig"),
        "xlang-handler": (root / "test/cases/stdlib_xlang_handler_ownership.xl").read_text(encoding="utf-8-sig"),
        "file-async": (root / "test/cases/stdlib_file_async.xl").read_text(encoding="utf-8-sig"),
    }.items():
        source = directory / (name + ".xl")
        source.write_text(program + 'println("body completed")\n', encoding="utf-8")
        binary = directory / (name + (".exe" if os.name == "nt" else ""))
        if name in ("file-async", "log-cycle", "xlang-handler"):
            run(name + "-jit", [xl, "run", source], 1 if name == "log-cycle" else 0)
        if run(name + "-build", [xl, "build", source, "-o", binary]):
            run(name + "-exit", [binary], 1 if name == "log-cycle" else 0)
    if not args.skip_sdk:
        host_source = directory / "owner_drain.c"
        host_source.write_text(HOST, encoding="utf-8")
        for library_name in (["xl.dll", "xlc.dll"] if os.name == "nt" else ["libxl.so", "libxlc.so"]):
            library = products / library_name
            binary = directory / (library.stem + "_drain" + (".exe" if os.name == "nt" else ""))
            link = library
            command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "release/include"),
                       host_source, link, "-o", binary]
            if os.name != "nt":
                command += ["-Wl,-rpath," + str(products)]
            if not run(library.stem + "-drain-build", command):
                continue
            catalog_binary=directory/(library.stem+"_boundary_catalog"+(".exe" if os.name=="nt" else ""))
            catalog_command=[args.cc,"-std=c11","-Wall","-Wextra","-Werror","-I"+str(root),"-I"+str(root/"release"),
                             root/"test/cases/xlir_ownership_boundary_catalog.c",link,"-o",catalog_binary]
            if os.name!="nt":catalog_command += ["-Wl,-rpath,"+str(products)]
            if run(library.stem+"-boundary-catalog-build",catalog_command):run(library.stem+"-boundary-catalog",[catalog_binary])
            self_binary=directory/(library.stem+"_self_retirement"+(".exe" if os.name=="nt" else ""))
            self_command=[args.cc,"-std=c11","-Wall","-Wextra","-Werror","-I"+str(root),"-I"+str(root/"release"),"-I"+str(root/"release/include"),
                          root/"test/cases/xlir_task_pool_self_retirement.c",link,"-o",self_binary]
            if os.name!="nt":self_command += ["-Wl,-rpath,"+str(products)]
            if run(library.stem+"-self-retirement-build",self_command):run(library.stem+"-self-retirement",[self_binary])
            for label,fixture in [("log-cycle","stdlib_log_ownership_cycle.xl"),("xlang-handler","stdlib_xlang_handler_ownership.xl")]:
                run(library.stem+"-"+label+"-drain-20",[binary,root/"test/cases"/fixture,root],20 if label=="log-cycle" else 0)
            for explicit in [False, True]:
                name = library.stem + ("-explicit-close" if explicit else "-automatic-close")
                script = directory / (name + ".xl")
                path = json.dumps((directory / (name + ".bin")).as_posix())
                text = f'auto alive = file.openAsync({path}, 15)\nauto writing = alive.writeAt(uint64(0), b"a\\0b")\nassert await writing == uint64(3)\n'
                if explicit:
                    text += "auto closing = alive.close()\nassert await closing\n"
                script.write_text(text, encoding="utf-8")
                run(name, [binary, script, root])
    report["complete"] = True
    report["ready"] = not report["missingPolicies"] and all(row["exit"] == 0 for row in report["probes"])
    (directory / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for row in report["missingPolicies"]:
        print("missing policy:", row["type"], row["destructor"], flush=True)
    raise SystemExit(0 if report["ready"] else 1)


if __name__ == "__main__":
    main()
