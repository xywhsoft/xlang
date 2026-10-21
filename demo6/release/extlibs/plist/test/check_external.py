"""使用 Python 标准库独立读取 C/XLang 生成的 XML 与 binary plist。"""

from pathlib import Path
import plistlib
import sys


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: check_external.py C.xml XL.xml C.bplist XL.bplist")
    paths = [Path(name) for name in sys.argv[1:]]
    expected = {
        "application": "xlang",
        "version": 6,
        "enabled": True,
        "icon": b"\x00XL",
        "tags": ["compiler", "script"],
    }
    binary_expected = {**expected, "uid": plistlib.UID(42)}
    for index, path in enumerate(paths):
        reference = expected if index < 2 else binary_expected
        if plistlib.loads(path.read_bytes()) != reference:
            raise ValueError(f"{path}: values differ from the example")
    if paths[0].read_bytes() != paths[1].read_bytes():
        raise ValueError("C and XLang XML bytes differ")
    print("plistlib four-format interop: PASS")


if __name__ == "__main__":
    main()
