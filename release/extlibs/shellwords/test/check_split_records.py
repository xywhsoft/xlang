"""Independent direct-input lexical oracle; no package encoder supplies the inputs."""

import argparse
import hashlib
import itertools
from pathlib import Path
import shlex


def corpus():
    alphabet = b"a '\"\\"
    for length in range(7):
        # C uses least significant digit first; keep coverage order explicit.
        for word in itertools.product(alphabet, repeat=length):
            value = bytes(reversed(word))
            try:
                expected = [text.encode("latin1") for text in shlex.split(value.decode("latin1"), comments=False, posix=True)]
            except ValueError:
                expected = None
            yield value, expected
    for byte in range(1, 256):
        value = bytes((byte,))
        expected = None if byte in (34, 39, 92) else [] if byte in (9, 10, 13, 32) else [value]
        yield value, expected
    # shlex intentionally differs on shell line continuations and escaped $/` in double quotes.
    yield from (
        (b"\\\n", []), (b"a\\\nb", [b"ab"]), (b"a \\\n b", [b"a", b"b"]),
        (b'"\\\n"', [b""]), (b'"a\\\nb"', [b"ab"]), (b"'a\\\nb'", [b"a\\\nb"]),
        (b'"a\\$b\\`c"', [b"a$b`c"]), (b'"a\\qb"', [b"a\\qb"]),
        (b"a\r\nb", [b"a", b"b"]), (b"a\vb\fc", [b"a\vb\fc"]),
        (b"a' b'\"c\"", [b"a bc"]), (b"''\"\"", [b""]),
        (b"head '' tail 'broken", None), (b"head " + bytes((92,)), None), (b'"a\\"', None),
        ("你好 '世界'".encode(), ["你好".encode(), "世界".encode()]),
        (b"#not-a-comment ; | $HOME", [b"#not-a-comment", b";", b"|", b"$HOME"]),
    )
    for mode, size in itertools.product(range(6), (0, 1, 31, 32, 33, 255, 4096, 16384)):
        pattern = b"a\\ b\"c\"'d'" if mode == 3 else b"x " if mode == 4 else b"a"
        value = (pattern * ((size + len(pattern) - 1) // len(pattern)))[:size]
        if mode in (1, 2):
            quote = b"'" if mode == 1 else b'"'
            value = quote + value + quote
        if mode == 5:
            value += b"'"
        try:
            expected = [text.encode("latin1") for text in shlex.split(value.decode("latin1"), comments=False, posix=True)]
        except ValueError:
            expected = None
        yield value, expected


def verify(lines):
    expected = list(corpus())
    if len(lines) != len(expected) or len(expected) != 19851:
        raise ValueError("must include all 19851 direct-input records")
    digest = hashlib.sha256()
    invalid = 0
    for index, (line, (source, words)) in enumerate(zip(lines, expected, strict=True)):
        fields = line.split("\t")
        if len(fields) < 3 or fields[0] != str(index) or bytes.fromhex(fields[1]) != source:
            raise ValueError(f"split corpus/order differs at {index}")
        if words is None:
            invalid += 1
            if fields[2:] != ["E"]:
                raise ValueError(f"split accepted malformed text at {index}")
        elif fields[2] != "O" or [bytes.fromhex(word) for word in fields[3:]] != words:
            raise ValueError(f"split words differ at {index}")
        digest.update(line.encode("ascii") + b"\n")
    return invalid, digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    args = parser.parse_args()
    lines = args.records.read_text(encoding="ascii").splitlines()
    invalid, digest = verify(lines)
    # 防止独立参照只检查输入顺序或遗漏空参数/失败状态。
    for index, replacement in ((0, "0\t\tO\t"), (1, "1\t61\tO\t62"), (3, "3\t27\tO")):
        changed = lines.copy()
        changed[index] = replacement
        try:
            verify(changed)
        except ValueError:
            continue
        raise ValueError("split oracle accepted a negative control")
    print(f"shellwords independent split oracle: {len(lines)} records, {invalid} malformed, 3 controls, PASS ({digest})")


if __name__ == "__main__":
    main()
