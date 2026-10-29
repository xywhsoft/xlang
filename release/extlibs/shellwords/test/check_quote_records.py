"""Validate the complete byte corpus with shlex and optionally the actual POSIX shell."""

import argparse
import hashlib
import itertools
from pathlib import Path
import shlex
import subprocess
import tempfile


def corpus():
    lengths = (0, 1, 2, 3, 7, 15, 31, 32, 33, 63, 64, 65, 127, 128, 129,
               255, 256, 257, 1023, 4096, 16384)
    pattern = b'\xe4\xbd\xa0 hello \\ "\n'
    for mode, size in itertools.product(range(6), lengths):
        if mode == 0:
            word = b" " * size
        elif mode == 1:
            word = b"'" * size
        elif mode == 2:
            word = bytes(pattern[i % len(pattern)] for i in range(size))
        elif mode == 3:
            word = bytes(39 if i % 97 == 0 else 97 for i in range(size))
        elif mode == 4:
            word = bytes(i % 255 + 1 for i in range(size))
        else:
            word = bytes(92 if i % 2 == 0 else 39 for i in range(size))
        yield b" " + word[1:] if size else b""
    yield from (bytes((value,)) for value in range(1, 256))
    yield from (b"", b"plain", b"if", b"A=B", b"A+=B", b"label:", b"raw\x80\xff",
                b"a-b=e", b"a:b", b"two words", b"$(exit 7)", b"line\nnext")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    parser.add_argument("--shell", action="store_true")
    args = parser.parse_args()
    lines = args.records.read_text(encoding="ascii").splitlines()
    expected = list(corpus())
    if len(expected) != 393 or len(lines) != len(expected):
        raise ValueError("quote records must contain all 393 cases")
    encoded = []
    digest = hashlib.sha256()
    for index, (line, word) in enumerate(zip(lines, expected, strict=True)):
        fields = line.split("\t")
        if len(fields) != 3 or fields[0] != str(index) or bytes.fromhex(fields[1]) != word:
            raise ValueError(f"quote corpus/order differs at {index}")
        quoted = bytes.fromhex(fields[2])
        words = [value.encode("latin1") for value in shlex.split(quoted.decode("latin1"), posix=True)]
        if words != [word]:
            raise ValueError(f"independent quote parse differs at {index}")
        encoded.append(quoted)
        digest.update(len(word).to_bytes(4, "little") + word)
    if args.shell:
        # 分组限制单次命令大小；工作目录与父门禁的临时目录一致。
        with tempfile.TemporaryDirectory(prefix="shellwords-oracle-") as work:
            for start in range(0, len(expected), 8):
                command = b"set -- " + b" ".join(encoded[start:start + 8]) + b"; printf '%s\\0' \"$@\""
                result = subprocess.run([b"/bin/sh", b"-c", command], cwd=work,
                                        capture_output=True, timeout=10, check=True)
                if result.stdout != b"".join(word + b"\0" for word in expected[start:start + 8]):
                    raise ValueError(f"actual shell argv differs at batch {start}")
    print(f"shellwords independent quote oracle: 393 records, PASS ({digest.hexdigest()})")


if __name__ == "__main__":
    main()
