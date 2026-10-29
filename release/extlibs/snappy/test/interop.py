"""Optional cross-implementation check; requires python-snappy 0.7.3."""

import random
import subprocess
import sys

import snappy


def c_call(executable: str, mode: str, data: bytes) -> bytes:
    return subprocess.run(
        [executable, mode], input=data, capture_output=True, check=True
    ).stdout


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: python interop.py path/to/interop.exe")
    if snappy.__version__ != "0.7.3":
        raise SystemExit("interop.py requires python-snappy 0.7.3")
    payloads = (
        b"abc",
        bytes(range(256)) + b"\0\xff\0",
        b"snappy-block" * 9000,
        random.Random(1234).randbytes(100000),
    )
    for plain in payloads:
        python_frame = snappy.StreamCompressor().add_chunk(plain)
        if c_call(sys.argv[1], "decode", python_frame) != plain:
            raise AssertionError("XRT failed to decode a Python Snappy frame")
        c_frame = c_call(sys.argv[1], "encode", plain)
        if snappy.StreamDecompressor().decompress(c_frame) != plain:
            raise AssertionError("Python Snappy failed to decode an XRT frame")
    print("snappy Python/C framed interoperability: PASS")


if __name__ == "__main__":
    main()
