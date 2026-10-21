"""Compare actual old/new scalar metric reuse and text shaping in one engine executable."""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import time
import uuid

from native_desktop import NativeDesktopProcess
from prepare_large_source_sdk import ancestors, name, plain, read, sha

IDE = Path(__file__).resolve().parents[1]
BUNDLE = IDE / "third_party/sdk_patches/large-source-experiment-v6"


def run(build_report, output, gcc, latin_font, cjk_font):
    build_report = build_report.absolute()
    output = output.absolute()
    build = read(build_report)
    if build["status"] != "sdk-build-finished" or build["exitCode"] != 0:
        raise ValueError("A completed private SDK build is required")
    source = Path(build["source"]).absolute()
    ancestors(source)
    ancestors(BUNDLE)
    ancestors(output.parent)
    if output.exists() or output.is_relative_to(source) or output.is_relative_to(BUNDLE):
        raise ValueError("Use a new output directory outside both input trees")
    manifest = read(BUNDLE / "patch-manifest.json")
    descriptor = BUNDLE / "source-materials-before.json"
    if sha(descriptor) != manifest["beforeDescriptorSha256"]:
        raise ValueError("Original source descriptor differs")
    materials = {row["path"]: row["sha256"] for row in read(descriptor)["files"]}
    materials.update({row["path"]: row["afterSha256"] for row in manifest["changes"]})
    tests = manifest["measureTestMaterials"]
    required_tests = {"tests/old_measure_shape.c", "tests/measure_test.c"}
    if {row["path"] for row in tests} != required_tests:
        raise ValueError("Unexpected comparison fixture inventory")

    def verify():
        for relative, digest in materials.items():
            path = source / name(relative)
            plain(path)
            if sha(path) != digest:
                raise ValueError(f"Private source differs: {relative}")
        for row in tests:
            path = BUNDLE / name(row["path"])
            plain(path)
            if sha(path) != row["sha256"]:
                raise ValueError(f"Test material differs: {row['path']}")
        for row in build["outputs"]:
            path = Path(row["path"])
            plain(path)
            if sha(path) != row["sha256"]:
                raise ValueError(f"Private build output differs: {path}")

    if latin_font is None:
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        buffer = ctypes.create_unicode_buffer(32768)
        count = kernel32.GetWindowsDirectoryW(buffer, len(buffer))
        if not 0 < count < len(buffer):
            raise ctypes.WinError(ctypes.get_last_error())
        latin_font = Path(buffer.value) / "Fonts/consola.ttf"
    font_inputs = [(Path(latin_font).absolute(), "latin.ttf"),
                   (Path(cjk_font).absolute(), "cjk.otf")]
    for path, _ in font_inputs:
        ancestors(path.parent)
        info = path.lstat()
        # Windows-installed fonts commonly have WinSxS hard links. We only read
        # and copy these runtime inputs; SDK materials retain the stricter rule.
        if not stat.S_ISREG(info.st_mode) or info.st_file_attributes & 0x400:
            raise ValueError(f"Unexpected runtime font path: {path}")
    font_hashes = {path: sha(path) for path, _ in font_inputs}
    verify()
    output.mkdir()
    fonts = []
    for path, filename in font_inputs:
        target = output / filename
        shutil.copy2(path, target)
        if sha(target) != font_hashes[path]:
            raise ValueError("Font changed while copying")
        fonts.append({"original": str(path), "copy": str(target), "sha256": font_hashes[path]})
    for row in tests:
        shutil.copy2(BUNDLE / row["path"], output / Path(row["path"]).name)
    fixture = output / "engine_comparison.c"
    fixture.write_text('#include "xge.c"\n#include "old_measure_shape.c"\n'
                       '#include "measure_test.c"\n', encoding="utf8")
    executable = output / "measure_test.exe"
    arguments = [str(gcc), "-O2", "-fstack-usage", "-Wall", "-Wextra", "-Werror",
                 "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-cast-function-type",
                 "-DXGE_DEBUGMODE=0", "-DXGE_XRT_PROFILE_IDE", "-I" + str(source),
                 str(fixture), "-lm", "-lws2_32", "-liphlpapi", "-lgdi32", "-luser32",
                 "-lshell32", "-lole32", "-loleaut32", "-luuid", "-limm32", "-lwinmm",
                 "-lavrt", "-o", str(executable)]
    report = {"status": "running", "buildReport": str(build_report),
              "buildReportSha256": sha(build_report), "patchManifestSha256": sha(BUNDLE / "patch-manifest.json"),
              "arguments": arguments, "root": str(output), "sourceFiles": len(materials),
              "testMaterials": tests, "fixtureSha256": sha(fixture), "fonts": fonts,
              "oneXrtOwner": "comparison executable; no SDK DLL linked",
              "sdkInstalled": False, "productionChanged": False}
    pointer = output / "result.json"
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.CreateDesktopW.restype = wintypes.HANDLE
    user32.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p,
                                     wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    user32.CloseDesktop.argtypes = [wintypes.HANDLE]
    desktop = None

    def save():
        pointer.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")

    save()
    try:
        link_log = output / "strict-link.log"
        with link_log.open("xb") as stream:
            linked = subprocess.run(arguments, stdout=stream, stderr=stream, timeout=180)
        report.update(linkExitCode=linked.returncode, linkLogSha256=sha(link_log))
        save()
        if linked.returncode != 0:
            raise RuntimeError("Strict whole-engine fixture build failed; see strict-link.log")
        desktop_name = "XlangMeasureReview-" + uuid.uuid4().hex
        desktop = user32.CreateDesktopW(desktop_name, None, None, 0, 0x10000000, None)
        if not desktop:
            raise ctypes.WinError(ctypes.get_last_error())
        report.update(desktop=desktop_name, neverSwitchedDesktop=True,
                      executableSha256=sha(executable))
        log = output / "actual-test.log"
        started = time.monotonic()
        with log.open("xb") as stream:
            process = NativeDesktopProcess([str(executable), str(output), fonts[0]["copy"], fonts[1]["copy"]], stream, stream,
                                           desktop_name, cwd=str(output))
            report["pid"] = process.pid
            save()
            try:
                code = process.wait(timeout=120)
            finally:
                process.close()
        report.update(exitCode=code, elapsedSeconds=round(time.monotonic() - started, 3),
                      logSha256=sha(log), status="shape-measure-equivalence-passed" if code == 0
                      else "shape-measure-equivalence-failed")
        if code != 0:
            raise RuntimeError("Drawing comparison failed; see actual-test.log")
        import re
        summary = re.search(r"shape measure comparison passed: (\d+) assertions; (\d+) complete shape cases; (\d+) scalar lookup checks; (\d+) old/new pixel cases; (\d+) bytes call-local table; (\d+) actual metric comparisons", log.read_text(encoding="utf8"))
        if not summary:
            raise RuntimeError("Incomplete fixture assertion summary")
        assertions, cases, lookups, pixels, table_bytes, metric_comparisons = map(int, summary.groups())
        expected = manifest["expectedMeasureFixtureCounters"]
        if assertions < 14000000 or (cases, lookups, pixels, table_bytes, metric_comparisons) != (
                expected["completeShapeCases"], expected["scalarLookupChecks"],
                expected["pixelCases"], expected["callLocalTableBytes"], expected["actualMetricComparisons"]):
            raise RuntimeError("Unexpected fixture counters")
        report.update(assertions=assertions, completeShapeCases=cases, scalarLookupChecks=lookups,
                      pixelCases=pixels, callLocalTableBytes=table_bytes, actualMetricComparisons=metric_comparisons)
        report["stackUsage"] = [{"path": str(path), "sha256": sha(path)}
                                for path in output.glob("*.su")]
        print(log.read_text(encoding="utf8"), end="")
        return report
    except Exception as error:
        report.update(status="shape-measure-equivalence-failed", error=str(error))
        raise
    finally:
        if desktop and not user32.CloseDesktop(desktop):
            raise ctypes.WinError(ctypes.get_last_error())
        verify()
        for path, _ in font_inputs:
            if sha(path) != font_hashes[path]:
                raise ValueError("Font input changed during comparison")
        report["inputsUnchanged"] = True
        save()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gcc", type=Path, default=Path(r"E:\software\w64devkit\bin\gcc.exe"))
    parser.add_argument("--latin-font", type=Path)
    parser.add_argument("--cjk-font", type=Path, default=IDE / "data/fonts/NotoSansMonoCJKsc-Regular.otf")
    options = parser.parse_args()
    run(options.build_report, options.output, options.gcc, options.latin_font, options.cjk_font)
