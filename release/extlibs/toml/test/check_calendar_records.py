"""用 Python 公历和 TOML 解析器独立核对 XRT 公元 UTC 毫秒。"""

import argparse
from datetime import datetime, timedelta, timezone
from pathlib import Path
import tomllib


def corpus():
    epoch = 62135596800000
    yield from (0, 1, epoch-1001, epoch-1, epoch, epoch+1,
                315537897599998, 315537897599999)
    seed = 0x9E3779B97F4A7C15
    for _ in range(512):
        seed = (seed * 6364136223846793005 + 1) & ((1 << 64) - 1)
        yield seed % 315537897600000


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    args = parser.parse_args()
    lines = args.records.read_text(encoding="ascii").splitlines()
    times = list(corpus())
    if len(lines) != len(times):
        raise ValueError(f"incomplete calendar corpus: {len(lines)}/{len(times)}")
    origin = datetime(1, 1, 1, tzinfo=timezone.utc)
    for index, (line, time) in enumerate(zip(lines, times, strict=True)):
        fields = line.split("\t")
        if len(fields) != 3 or fields[:2] != [str(index), str(time)]:
            raise ValueError(f"calendar corpus/order differs at {index}")
        expected = origin + timedelta(milliseconds=time)
        text = bytes.fromhex(fields[2]).decode("ascii")
        canonical = "value = " + expected.isoformat(timespec="milliseconds").replace("+00:00", "Z") + "\n"
        if text != canonical or tomllib.loads(text) != {"value": expected}:
            raise ValueError(f"UTC value/format differs at {index}")
    print("toml calendar oracle: 520 records, PASS (years 0001..9999)")


if __name__ == "__main__":
    main()
