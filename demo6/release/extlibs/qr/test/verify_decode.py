"""使用独立 ZXing 解码器验证二维码数据和纠错路径。"""

import os
import subprocess
import sys

# 测试只处理小图，避免 NumPy 在资源紧张的构建机上启动多线程。
os.environ["OPENBLAS_NUM_THREADS"] = "1"

import numpy as np
import zxingcpp


def decode(executable, text, level):
    result = subprocess.run(
        [executable, level], input=text.encode("utf-8"), capture_output=True, check=True
    )
    rows = result.stdout.decode("ascii").splitlines()
    size = int(rows[0])
    assert len(rows) == size + 1 and all(len(row) == size for row in rows[1:])
    pixels = np.array(
        [[0 if cell == "1" else 255 for cell in row] for row in rows[1:]],
        dtype=np.uint8,
    )
    pixels = np.kron(
        np.pad(pixels, 4, constant_values=255), np.ones((4, 4), dtype=np.uint8)
    )
    scanned = zxingcpp.read_barcode(pixels)
    assert scanned is not None and scanned.text == text, (level, len(text), scanned)
    return size


def main():
    executable = sys.argv[1]
    samples = [
        ("HELLO WORLD", "M"),
        ("1234567890" * 5, "L"),
        ("https://xlang.dev/blog/semver-extension-released", "Q"),
        ("hello-world", "H"),
        ("你好，世界", "M"),
        ("xlang 你好 🧪", "Q"),
        ("1234567890" * 4 + "a" + "1234567890" * 4, "M"),
        ("1234567890" * 4 + "你好" + "1234567890" * 4, "M"),
        ("A" * 500, "H"),
        ("A" * 1000, "H"),
        ("A" * 1800, "H"),
        ("a" * 2953, "L"),
        ("123456789" * 787 + "123456", "L"),
    ]
    for text, level in samples:
        print(level, len(text.encode("utf-8")), decode(executable, text, level))
    over_limit = subprocess.run(
        [executable, "L"], input=b"a" * 2954, capture_output=True, check=False
    )
    assert over_limit.returncode != 0 and not over_limit.stdout
    print("qr independent decode: PASS")


if __name__ == "__main__":
    main()
