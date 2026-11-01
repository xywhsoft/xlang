"""用 MongoDB 官方 BSON 实现验证双语写出，并生成反向读取样例。"""

from pathlib import Path
import sys
from datetime import datetime, timezone

from bson import decode, encode
from bson.codec_options import CodecOptions, DatetimeConversion
from bson.datetime_ms import DatetimeMS


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit("usage: check_external.py C.bson XL.bson PYTHON.bson [DATES.tsv]")
    c_path, xl_path, python_path = map(Path, sys.argv[1:4])
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
    if len(sys.argv) == 5:
        options = CodecOptions(datetime_conversion=DatetimeConversion.DATETIME_MS)
        seen = set()
        for line in Path(sys.argv[4]).read_text(encoding="utf-8-sig").splitlines():
            if not line.startswith("date\t0\t"):
                continue
            _, _, unix, _, hex_wire = line.split("\t")
            milliseconds = int(unix)
            wire = bytes.fromhex(hex_wire)
            document = decode(wire, codec_options=options)
            if int(document["when"]) != milliseconds or encode({"when": DatetimeMS(milliseconds)}) != wire:
                raise ValueError("PyMongo Date wire roundtrip differs")
            if milliseconds in seen:
                raise ValueError("Repeated PyMongo Date vector")
            seen.add(milliseconds)
        if len(seen) != 14:
            raise ValueError("Incomplete PyMongo Date vectors")
        print("PyMongo Date wire interop: 14 vectors, PASS")
    print("PyMongo BSON write/read fixture: PASS")


if __name__ == "__main__":
    main()
