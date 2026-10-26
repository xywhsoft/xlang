"""直接测试包内响应源码，不依赖尚未修复的网络导入链。"""

from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[4]
PACKAGE = Path(__file__).resolve().parents[1]
XL = ROOT / "release" / ("xl.exe" if sys.platform == "win32" else "xl")
EXPECTED = "volc.sts response: PASS\n"


def run(*args):
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", check=False)
    if result.returncode != 0 or result.stdout.replace("\r\n", "\n") != EXPECTED:
        raise RuntimeError(f"response test failed: {args}\n{result.stdout}\n{result.stderr}")


def main():
    source = (PACKAGE / "src/response.xl").read_text(encoding="utf-8")
    cases = (PACKAGE / "test/response_cases.xl").read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="xlang-volc-sts-") as directory:
        script = Path(directory) / "response_test.xl"
        output = Path(directory) / ("response_test.exe" if sys.platform == "win32" else "response_test")
        script.write_text(source + "\n" + cases, encoding="utf-8")
        run(str(XL), "run", str(script))
        build = subprocess.run((str(XL), "build", str(script), "-o", str(output)),
                               cwd=ROOT, capture_output=True, text=True,
                               encoding="utf-8", errors="replace", check=False)
        if build.returncode != 0:
            raise RuntimeError(f"response build failed:\n{build.stdout}\n{build.stderr}")
        run(str(output))
    print("volc.sts response JIT/AOT: PASS")


if __name__ == "__main__":
    main()
