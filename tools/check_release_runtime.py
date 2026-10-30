"""Focused release regressions: CRT formatting and OS argv ownership."""
from pathlib import Path
import argparse
import ctypes
import json
import os
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    args = parser.parse_args()
    root = args.root.resolve()
    products = (args.products or root / "release").resolve()
    directory = (args.directory or root / "build/test/release-runtime").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    xl = products / ("xl.exe" if os.name == "nt" else "xl")
    env = dict(os.environ, XL6_OS_ARGS_TOKEN="preflight-你")
    env["PATH"] = str(products) + os.pathsep + env.get("PATH", "")
    env["XLANG_PACKAGE_PATH"] = str(root / "release/extlibs")
    report = {"complete": False, "checks": []}
    symbols = re.findall(r"XL6_TEXT_EXPORT\(\s*(\w+)\s*,",
                         (root / "src/runtime/xl6_text_symbols.inc").read_text(encoding="utf-8"))
    if not symbols or len(symbols) != len(set(symbols)):
        raise RuntimeError("empty or duplicate resident export inventory")
    for name in (["xl.dll", "xlc.dll"] if os.name == "nt" else ["libxl.so", "libxlc.so"]):
        library = ctypes.CDLL(str(products / name))
        missing = [symbol for symbol in symbols if not hasattr(library, symbol)]
        report.setdefault("residentExports", []).append({"library": name, "checked": len(symbols), "missing": missing})
        (directory / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if missing:
            raise RuntimeError(name + " missing resident exports: " + ", ".join(missing))

    def run(name, command):
        result = subprocess.run([x if isinstance(x, bytes) else str(x) for x in command], cwd=root, env=env,
                                capture_output=True, timeout=120)
        (directory / (name + ".log")).write_bytes(result.stdout + result.stderr)
        report["checks"].append({"name": name, "command": list(map(str, command)), "exit": result.returncode})
        (directory / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if result.returncode:
            raise RuntimeError(name + " failed: " + (result.stdout + result.stderr).decode("utf-8", "replace"))
        print(name, flush=True)

    suffix = ".exe" if os.name == "nt" else ""
    for relative in ["test/cases/tcc_c99_vsnprintf.c", "test/cases/stdlib_os_args.xl"]:
        source = root / relative
        name = source.stem
        extra = ["preflight-你", "", "embedded space"] if name == "stdlib_os_args" else []
        run(name + "-jit", [xl, "run", source, "--", *extra])
        binary = directory / (name + suffix)
        run(name + "-build", [xl, "build", source, "-o", binary])
        run(name + "-aot", [binary, *extra])

    for name in (["xl.dll", "xlc.dll"] if os.name == "nt" else ["libxl.so", "libxlc.so"]):
        library = products / name
        binary = directory / (library.stem + "_argv" + suffix)
        link = library
        command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "release/include"),
                   root / "test/cases/stdlib_os_args_native_host.c", link, "-o", binary]
        if os.name != "nt":
            command += ["-Wl,-rpath," + str(products)]
        run(library.stem + "-native-build", command)
        run(library.stem + "-native-oom", [binary, "preflight-你", "", "embedded space"])
        if os.name != "nt":
            run(library.stem + "-invalid-utf8", [os.fsencode(binary), b"expect-invalid-utf8", b"bad-\xff"])
    report["complete"] = True
    (directory / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
