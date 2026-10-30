"""Optional independent frame check; requires zstandard 0.25.0."""

import random
import subprocess
import sys

import zstandard


def c_call(executable: str, mode: str, data: bytes) -> bytes:
    result = subprocess.run(
        [executable, mode], input=data, capture_output=True, check=True, timeout=30
    )
    if result.stderr:
        raise AssertionError(f"unexpected C diagnostic: {result.stderr!r}")
    return result.stdout


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: python interop.py path/to/interop.exe")
    if zstandard.__version__ != "0.25.0":
        raise SystemExit("interop.py requires zstandard 0.25.0")
    payloads = (
        b"abc",
        bytes(range(256)) + b"\0\xff\0",
        b"zstd-block" * 9000,
        random.Random(1234).randbytes(100000),
    )
    for plain in payloads:
        python_frame = zstandard.ZstdCompressor(level=3).compress(plain)
        if c_call(sys.argv[1], "decode", python_frame) != plain:
            raise AssertionError("XRT failed to decode a Python Zstandard frame")
        c_frame = c_call(sys.argv[1], "encode", plain)
        if zstandard.ZstdDecompressor().decompress(c_frame, max_output_size=200000) != plain:
            raise AssertionError("Python Zstandard failed to decode an XRT frame")
    print("zstd Python/C frame interoperability: PASS")


if __name__ == "__main__":
    main()
