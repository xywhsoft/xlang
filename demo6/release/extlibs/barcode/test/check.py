"""条码包的 Windows C/XLang 对照与裁剪 AOT 门禁。"""

import argparse
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[4]
XL = ROOT / "release" / "xl.exe"
C_RUNNER = ROOT / "tools" / "run_extlib_c.ps1"


def run(name, *command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", check=False)
    if result.returncode:
        raise RuntimeError(f"{name}: exit {result.returncode}\n{result.stderr}\n{result.stdout}")
    print(f"[barcode] {name}: PASS")
    return result.stdout.replace("\r\n", "\n")


def c_source(name, source):
    return run(name, "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
               "-File", str(C_RUNNER), "-Package", "barcode", "-Source", source)


def main(skip_aot):
    run("manifest C syntax", "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-DXRT_MODULE_VALUE_CONTAINER", "-DXRT_MODULE_BUFFER",
        "-DXRT_MODULE_STRING", "-Ilib/xrt", "-Irelease/extlibs/barcode/include",
        "-fsyntax-only", "release/extlibs/barcode/barcode_native.c")
    run("XLang cases", str(XL), "run", "test/cases/extlib_barcode.xl")
    run("XLang boundaries", str(XL), "run", "test/cases/extlib_barcode_boundaries.xl")
    run("XLang recovery", str(XL), "run", "release/extlibs/barcode/test/recovery.xl")
    run("XLang allocation", str(XL), "run", "release/extlibs/barcode/test/allocations.xl")
    c_source("C boundaries", "release/extlibs/barcode/test/test_barcode.c")

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
        build_root = ROOT / "build" / "test"
        build_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="barcode_", dir=build_root) as work:
            core = Path(work) / "aot_core.exe"
            run("AOT core build", str(XL), "build",
                "release/extlibs/barcode/test/aot_core.xl", "-o", str(core))
            if run("AOT core", str(core)):
                raise AssertionError("barcode AOT core produced unexpected output")
            example = Path(work) / "aot_example.exe"
            run("AOT example build", str(XL), "build",
                "release/examples/extlibs/barcode.xl", "-o", str(example))
            if run("AOT example", str(example)) != expected:
                raise AssertionError("barcode AOT example output differs from fixed output")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-aot", action="store_true",
                        help="run only the C and XLang dynamic checks")
    main(parser.parse_args().skip_aot)
