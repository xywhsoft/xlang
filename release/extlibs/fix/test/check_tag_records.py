"""按公开 TagNum 语法独立组帧，核对产品 parse/validate；不加载 XRT。"""
import argparse
from pathlib import Path


def corpus():
    tags = (b"8", b"9", b"35", b"10")
    for padding in range(33):
        for role, tag in enumerate(tags):
            for style in range(2):
                yield role, b"0" * padding + tag, style, padding == 0
    for tag in (b"", b"0", b"00", b"-1", b"+8", b" 8", b"8 ", b"8a", b"8\x01",
                b"9223372036854775808", b"18446744073709551615", b"\xff"):
        for role in range(4):
            for style in range(2):
                yield role, tag, style, False
    for tag in (b"1", b"11", b"35", b"58", b"9999", b"2147483648", b"9223372036854775807"):
        for style in range(2):
            yield 2, tag, style, True


def frame(role, tag, style):
    tags = [b"8", b"9", b"35", b"10"]
    tags[role] = tag
    body = tags[2] + b"=" + (b"abc=xyz" if style else b"0007") + b"\x01"
    prefix = tags[0] + b"=FIX.4.4\x01" + tags[1] + b"="
    prefix += (b"00" if style else b"") + str(len(body)).encode() + b"\x01" + body
    return prefix + tags[3] + b"=" + f"{sum(prefix) % 256:03d}".encode() + b"\x01"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    lines = parser.parse_args().records.read_text(encoding="ascii").splitlines()
    expected = list(corpus())
    if len(lines) != len(expected):
        raise ValueError("incomplete tag corpus")
    for index, (line, (role, tag, style, valid)) in enumerate(zip(lines, expected, strict=True)):
        fields = line.split("\t")
        status = [str(index), str(int(valid)), "none" if valid else "value", str(int(valid)), "none"]
        if len(fields) != 6 or fields[:5] != status or bytes.fromhex(fields[5]) != frame(role, tag, style):
            raise ValueError(f"tag grammar/frame differs at {index}")
    print("FIX independent tag grammar: 374 records, PASS")


if __name__ == "__main__":
    main()
