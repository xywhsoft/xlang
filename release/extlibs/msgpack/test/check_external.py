"""用独立 MessagePack 实现验证双语写出，并生成反向读取样例。"""

from pathlib import Path
import sys

import msgpack


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: check_external.py C.bin XL.bin PYTHON.bin")
    c_path, xl_path, python_path = map(Path, sys.argv[1:])
    expected = {"name": "xlang", "count": 7,
                "maximum": 18446744073709551615,
                "when": msgpack.Timestamp(0, 1000000)}
    if c_path.read_bytes() != xl_path.read_bytes():
        raise ValueError("C and XLang MessagePack bytes differ")
    for path in (c_path, xl_path):
        if msgpack.unpackb(path.read_bytes(), raw=False) != expected:
            raise ValueError(f"{path}: decoded values differ from the example")
    python_path.write_bytes(msgpack.packb({
        "name": "Python 你好",
        "count": 1099511627776,
        "maximum": 18446744073709551615,
        "nested": {"ok": True},
        "items": [1, 2, 3],
        "raw": b"\x00\xff",
        "when": msgpack.Timestamp(-1, 500000000),
    }, use_bin_type=True))
    print("Python MessagePack write/read fixture: PASS")


if __name__ == "__main__":
    main()
