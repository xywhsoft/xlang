"""条码包的 Windows C/XLang 对照与裁剪 AOT 门禁。"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[4]
XL = ROOT / "release" / "xl.exe"
C_RUNNER = ROOT / "tools" / "run_extlib_c.ps1"
WORK = None


def run(name, *command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", check=False)
    if WORK is not None:
        with (WORK / "commands.log").open("a", encoding="utf-8") as log:
            log.write(f"{name}: {subprocess.list2cmdline(command)}\nexit={result.returncode}\n")
            log.write(result.stdout + result.stderr + "\n")
    if result.returncode:
        raise RuntimeError(f"{name}: exit {result.returncode}\n{result.stderr}\n{result.stdout}")
    print(f"[barcode] {name}: PASS")
    return result.stdout.replace("\r\n", "\n")


def c_source(name, source):
    return run(name, "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
               "-File", str(C_RUNNER), "-Package", "barcode", "-Source", source)


def fingerprint(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main(skip_aot, work):
    inputs = [p for directory in ("release/extlibs/barcode", "release/include")
              for p in (ROOT / directory).rglob("*") if p.is_file()]
    inputs.extend(ROOT / p for p in (
        "release/xl.exe", "release/xl.dll", "release/xlc.dll", "lib/xrt/xrt.h",
        "lib/xruntime/xruntime.h", "tools/run_extlib_c.ps1", "test/cases/extlib_barcode.xl",
        "test/cases/extlib_barcode_boundaries.xl", "release/examples/extlibs/barcode.xl"))
    hashes = {str(p.relative_to(ROOT)): fingerprint(p) for p in inputs}
    (work / "inputs.before.json").write_text(json.dumps(hashes, indent=2), encoding="utf-8")
    run("GCC version", "gcc", "--version")
    run("manifest C syntax", "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-DXRT_MODULE_VALUE_CONTAINER", "-DXRT_MODULE_BUFFER",
        "-DXRT_MODULE_STRING", "-Ilib/xrt", "-Irelease/extlibs/barcode/include",
        "-fsyntax-only", "release/extlibs/barcode/barcode_native.c")
    flags = ["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-Ilib/xrt", "-Irelease/extlibs/barcode/include"]
    runtime = work / "runtime.o"
    native = work / "test_barcode.exe"
    run("selected XRT runtime", *flags, "-c", "release/extlibs/barcode/test/xrt_runtime.c",
        "-o", str(runtime))
    run("GCC C boundaries build", *flags, "release/extlibs/barcode/barcode_native.c",
        "release/extlibs/barcode/test/test_barcode.c", str(runtime), "-lWs2_32",
        "-lAdvapi32", "-lIphlpapi", "-lm", "-o", str(native))
    run("GCC C boundaries and result budgets", str(native))
    run("XLang cases", str(XL), "run", "test/cases/extlib_barcode.xl")
    run("XLang boundaries", str(XL), "run", "test/cases/extlib_barcode_boundaries.xl")
    run("XLang recovery", str(XL), "run", "release/extlibs/barcode/test/recovery.xl")
    run("XLang allocation", str(XL), "run", "release/extlibs/barcode/test/allocations.xl")
    run("XLang result budgets", str(XL), "run", "release/extlibs/barcode/test/owned_results.xl")
    c_source("C boundaries", "release/extlibs/barcode/test/test_barcode.c")

    generated = work / "owned_probe.c"
    run("whole-call ownership cgen", str(XL), "cgen",
        "release/extlibs/barcode/test/owned_probe.xl", "--emit", str(generated))
    text = generated.read_text(encoding="utf-8")
    for symbol in ("xb_svg", "xb_ascii"):
        if len(re.findall(rf"\b\w+\.Data\s*=\s*{symbol}\s*\(", text)) != 2:
            raise AssertionError(f"{symbol} must be adopted once in each EAN/UPC entry")
    # 允许共用失败函数保留消息快照；八组预算另约束四条成功结果不能复制。
    if len(re.findall(r"=\s*xlir_string_copy\s*\(", text)) > 1 or "xl6CStringCopyV1" in text:
        raise AssertionError("barcode copied adopted result text in the complete return chain")

    # 两种语言展示同一组操作，输出必须逐字节等价（仅统一换行）。
    xl_output = run("XLang example", str(XL), "run", "release/examples/extlibs/barcode.xl")
    c_output = c_source("C example", "release/extlibs/barcode/examples/barcode.c")
    direct_output = run("direct C example", str(XL), "run",
                        "release/extlibs/barcode/examples/barcode.c")
    expected = (ROOT / "release/extlibs/barcode/examples/barcode.expected.txt").read_text(
        encoding="utf-8")
    for name, output in (("C", c_output), ("direct C", direct_output),
                         ("XLang", xl_output)):
        if output != expected:
            raise AssertionError(f"{name} example differs from fixed output:\n{output}")
    print("[barcode] C/XLang fixed output: PASS")

    if not skip_aot:
        for name, source in (
            ("core", "release/extlibs/barcode/test/aot_core.xl"),
            ("central", "test/cases/extlib_barcode.xl"),
            ("boundaries", "test/cases/extlib_barcode_boundaries.xl"),
            ("recovery", "release/extlibs/barcode/test/recovery.xl"),
            ("allocations", "release/extlibs/barcode/test/allocations.xl"),
            ("owned", "release/extlibs/barcode/test/owned_results.xl"),
        ):
            executable = work / f"aot_{name}.exe"
            run(f"AOT {name} build", str(XL), "build", source, "-o", str(executable))
            output = run(f"AOT {name}", str(executable))
            if name == "core" and output:
                raise AssertionError("barcode AOT core produced unexpected output")
        for name, source in (("XLang", "release/examples/extlibs/barcode.xl"),
                             ("C", "release/extlibs/barcode/examples/barcode.c")):
            executable = work / f"aot_example_{name}.exe"
            run(f"{name} AOT example build", str(XL), "build", source, "-o", str(executable))
            if run(f"{name} AOT example", str(executable)) != expected:
                raise AssertionError(f"barcode {name} AOT output differs from fixed output")
    for path in inputs:
        if fingerprint(path) != hashes[str(path.relative_to(ROOT))]:
            raise AssertionError(f"barcode validation input changed: {path}")
    print("barcode Windows focused gate: PASS" if skip_aot else
          "barcode Windows complete AOT gate: PASS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-aot", action="store_true",
                        help="run only the C and XLang dynamic checks")
    build_root = (ROOT / "build" / "test").resolve()
    build_root.mkdir(parents=True, exist_ok=True)
    WORK = Path(tempfile.mkdtemp(prefix="barcode_guard_", dir=build_root)).resolve()
    try:
        main(parser.parse_args().skip_aot, WORK)
    except BaseException:
        print(f"barcode failure artifacts retained: {WORK}")
        raise
    else:
        if WORK.parent != build_root or not WORK.name.startswith("barcode_guard_"):
            raise RuntimeError(f"unsafe barcode cleanup path: {WORK}")
        shutil.rmtree(WORK)
