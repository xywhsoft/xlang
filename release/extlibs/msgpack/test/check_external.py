"""用独立 MessagePack 实现验证双语写出，并生成反向读取样例。"""

from datetime import datetime
from pathlib import Path
import sys

import msgpack


def timestamp_records(path):
    epoch = (datetime(1970, 1, 1) - datetime(1, 1, 1)).days * 86400000
    seen, corpus, heads = set(), {}, {}
    for line in Path(path).read_text(encoding='ascii').splitlines():
        index, ticks, head, wire, observed, canonical = line.split('\t')
        index, ticks, head = int(index), int(ticks), int(head, 16)
        if (index, head) in seen or index not in range(530):
            raise ValueError('Repeated or unknown timestamp record')
        if observed != f'time:{ticks}' or (index in corpus and corpus[index] != ticks):
            raise ValueError('Inconsistent native timestamp result')
        data = bytes.fromhex(wire)
        value = msgpack.unpackb(data, timestamp=0)
        if (not data or data[0] != head or not isinstance(value, msgpack.Timestamp) or
                value.nanoseconds % 1000000 or
                value.seconds * 1000 + value.nanoseconds // 1000000 + epoch != ticks):
            raise ValueError('Official codec timestamp differs')
        packed = msgpack.packb(value)
        if packed != bytes.fromhex(canonical):
            raise ValueError('Official codec shortest timestamp encoding differs')
        expected_heads = {0xc7, 0xc8, 0xc9, packed[0]}
        if index in heads and heads[index][0] != expected_heads:
            raise ValueError('Inconsistent timestamp framing set')
        heads.setdefault(index, (expected_heads, set()))[1].add(head)
        corpus[index] = ticks
        seen.add((index, head))
    if set(corpus) != set(range(530)) or len(seen) != 1599 or any(a != b for a, b in heads.values()):
        raise ValueError('Incomplete official codec timestamp matrix')
    print('Official MessagePack timestamp interop: 530 times, 1599 frames, PASS')


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit("usage: check_external.py C.bin XL.bin PYTHON.bin [TIMESTAMPS.tsv]")
    c_path, xl_path, python_path = map(Path, sys.argv[1:4])
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
    if len(sys.argv) == 5:
        timestamp_records(sys.argv[4])


if __name__ == "__main__":
    main()
