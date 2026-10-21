"""用独立 CBOR 实现验证双语写出，并生成带时间标签的读取样例。"""

from datetime import datetime, timezone
from pathlib import Path
import sys

import cbor2


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: check_external.py C.bin XL.bin PYTHON.bin")
    c_path, xl_path, python_path = map(Path, sys.argv[1:])
    if c_path.read_bytes() != xl_path.read_bytes():
        raise ValueError("C and XLang CBOR example bytes differ")
    for path in (c_path, xl_path):
        if cbor2.loads(path.read_bytes()) != {
            "name": "xlang", "maximum": 18446744073709551615
        }:
            raise ValueError(f"{path}: decoded values differ from the example")
    python_path.write_bytes(cbor2.dumps({
        "name": "Python 你好",
        "count": 1099511627776,
        "maximum": 18446744073709551615,
        "nested": {"ok": True},
        "items": [1, 2, 3],
        "raw": b"\x00\xff",
        "when": datetime(2026, 8, 17, 1, 30, 0, 123456, tzinfo=timezone.utc),
        "epoch": cbor2.CBORTag(1, 1786930200.25),
    }))
    print("cbor2 write/read fixture: PASS")


if __name__ == "__main__":
    main()
