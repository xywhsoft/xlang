"""用 Python 标准库独立检查 C/XLang 写出的 TOML。"""

from datetime import date, datetime, time, timedelta, timezone
from pathlib import Path
import sys
import tomllib


def check_version_10_syntax():
    invalid = (
        "a = { b = 1, }\n",
        "a = { b = 1,\n c = 2 }\n",
        "a = 07:32\n",
        "a = 1979-05-27T07:32\n",
        "中文 = 1\n",
        'a = "\\x33"\n',
        'a = "\\e"\n',
    )
    for source in invalid:
        try:
            tomllib.loads(source)
        except tomllib.TOMLDecodeError:
            continue
        raise ValueError(f"tomllib accepted syntax outside TOML 1.0: {source!r}")
    if tomllib.loads('"中文" = 1\n') != {"中文": 1}:
        raise ValueError("tomllib rejected a quoted Unicode key")


def check_nested(path: Path):
    expected = {
        "fruit": [
            {"name": "apple", "physical": {"color": "red"},
             "variety": [{"name": "red delicious"}]},
            {"name": "banana", "physical": {"color": "yellow"}},
        ]
    }
    if tomllib.loads(path.read_text(encoding="utf-8")) != expected:
        raise ValueError("nested array-of-tables scope changed on encode")


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--nested":
        check_nested(Path(sys.argv[2]))
        print("tomllib nested tables: PASS")
        return
    if len(sys.argv) != 3:
        raise SystemExit("usage: check_external.py C.toml XL.toml")
    c_path, xl_path = map(Path, sys.argv[1:])
    check_version_10_syntax()
    expected = {
        "title": "xlang service",
        "started": datetime(2026, 7, 16, 8, 30, tzinfo=timezone.utc),
        "released": date(2026, 7, 16),
        "window": time(9, 15, 30, 123456),
        "updated": datetime(2026, 7, 16, 9, 45, 30, 250000),
        "review": datetime(2026, 7, 16, 8, 30,
                           tzinfo=timezone(timedelta(minutes=330))),
        "server": {"host": "127.0.0.1", "port": 8080},
    }
    if c_path.read_bytes() != xl_path.read_bytes():
        raise ValueError("C and XLang TOML bytes differ")
    for path in (c_path, xl_path):
        if tomllib.loads(path.read_text(encoding="utf-8")) != expected:
            raise ValueError(f"{path}: decoded values differ from the example")
    print("tomllib external parse: PASS")


if __name__ == "__main__":
    main()
