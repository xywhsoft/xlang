"""用独立 ZXing 解码器核对当前 QR 编码器。"""

import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    for module in ("numpy", "zxingcpp"):
        if importlib.util.find_spec(module) is None:
            raise SystemExit(f"missing test dependency: {module}")

    root = Path(__file__).resolve().parents[4]
    package = root / "release/extlibs/qr"
    flags = [
        "gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        f"-I{root / 'lib/xrt'}", f"-I{package / 'include'}",
    ]
    links = ["-lm"]
    if sys.platform == "win32":
        links = ["-lWs2_32", "-lAdvapi32", "-lIphlpapi", *links]

    # 编译产物只存在于本次临时目录，避免污染发行目录。
    with tempfile.TemporaryDirectory(prefix="xl_qr_decode_") as work:
        work = Path(work)
        runtime = work / "xrt_runtime.o"
        executable = work / ("dump_matrix.exe" if sys.platform == "win32" else "dump_matrix")
        subprocess.run(flags + ["-c", str(package / "test/xrt_runtime.c"),
                              "-o", str(runtime)], cwd=root, check=True)
        subprocess.run(flags + [str(package / "test/dump_matrix.c"),
                              str(runtime), *links, "-o", str(executable)],
                       cwd=root, check=True)
        subprocess.run([sys.executable, str(package / "test/verify_decode.py"),
                        str(executable)], cwd=root, check=True)


if __name__ == "__main__":
    main()
