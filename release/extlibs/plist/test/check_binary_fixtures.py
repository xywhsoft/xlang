"""核对 C 产生的全部新 binary 节点夹具，不依赖 C 的读回结果。"""
from datetime import datetime
from pathlib import Path
import plistlib
import sys

expected = [
    None, True, -42, 1.25, "ascii", "你好🎶", b"\x00\x80\xff",
    datetime(2001, 1, 1, 0, 0, 1), plistlib.UID(2**64 - 1), [1, True],
    {"alpha": 1, "beta": "你好"}, {"items": ["a", {"n": 2}]},
]
root = Path(sys.argv[1])
for index, value in enumerate(expected):
    data = (root / f"vector-{index}.bplist").read_bytes()
    actual = plistlib.loads(data)
    if not data.startswith(b"bplist00") or actual != value or type(actual) is not type(value):
        raise AssertionError(f"binary fixture {index}: {actual!r} != {value!r}")
print("plistlib binary fixtures: 12 PASS")
