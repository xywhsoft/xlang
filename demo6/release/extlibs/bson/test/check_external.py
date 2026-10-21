"""用 MongoDB 官方 BSON 实现验证双语写出，并生成反向读取样例。"""

from pathlib import Path
import sys
from datetime import datetime, timezone

from bson import decode, encode


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: check_external.py C.bson XL.bson PYTHON.bson")
    c_path, xl_path, python_path = map(Path, sys.argv[1:])
    expected = {"name": "xlang", "count": 7}
    if c_path.read_bytes() != xl_path.read_bytes():
        raise ValueError("C and XLang BSON bytes differ")
    for path in (c_path, xl_path):
        if decode(path.read_bytes()) != expected:
            raise ValueError(f"{path}: decoded values differ from the example")
    python_path.write_bytes(encode({
        "name": "Python 你好",
        "count": 1099511627776,
        "nested": {"ok": True},
        "items": [1, 2, 3],
        "raw": b"\x00\xff",
        "when": datetime.fromtimestamp(-0.001, tz=timezone.utc),
    }))
    print("PyMongo BSON write/read fixture: PASS")


if __name__ == "__main__":
    main()
