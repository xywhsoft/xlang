"""用独立 ZXing 扫码器读回 EAN-13 / UPC-A 的公开模块输出。"""

import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    for module in ("numpy", "zxingcpp"):
        if importlib.util.find_spec(module) is None:
            raise SystemExit(f"missing test dependency: {module}")

    import numpy as np
    import zxingcpp

    root = Path(__file__).resolve().parents[4]
    package = root / "release/extlibs/barcode"
    flags = ["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             f"-I{root / 'lib/xrt'}", f"-I{package / 'include'}"]
    links = ["-lm"]
    if sys.platform == "win32":
        links = ["-lWs2_32", "-lAdvapi32", "-lIphlpapi", *links]

    samples = [
        ("ean", "400638133393", "4006381333931"),
        ("ean", "4006381333931", "4006381333931"),
        ("upc", "03600029145", "036000291452"),
        ("upc", "036000291452", "036000291452"),
    ]
    with tempfile.TemporaryDirectory(prefix="xl_barcode_decode_") as work:
        runtime = Path(work) / "xrt_runtime.o"
        executable = Path(work) / ("dump_modules.exe" if sys.platform == "win32" else "dump_modules")
        subprocess.run(flags + ["-c", str(package / "test/xrt_runtime.c"),
                                "-o", str(runtime)], cwd=root, check=True)
        subprocess.run(flags + [str(package / "test/dump_modules.c"),
                                str(runtime), *links, "-o", str(executable)],
                       cwd=root, check=True)
        for kind, given, expected in samples:
            modules = subprocess.run([str(executable), kind, given], cwd=root,
                                     capture_output=True, check=True).stdout.strip()
            assert len(modules) == 95 and set(modules) <= {ord("0"), ord("1")}
            quiet = 11 if kind == "ean" else 9
            row = np.array([255] * quiet +
                           [0 if bit == ord("1") else 255 for bit in modules] +
                           [255] * (18 - quiet), dtype=np.uint8)
            image = np.tile(np.repeat(row, 4), (120, 1))
            scanned = zxingcpp.read_barcode(image)
            assert scanned is not None, (kind, given)
            accepted = (expected, "0" + expected) if kind == "upc" else (expected,)
            assert scanned.text in accepted, (
                kind, given, scanned.text)
            print(kind, given, scanned.text)
    print("barcode independent decode: PASS")


if __name__ == "__main__":
    main()
