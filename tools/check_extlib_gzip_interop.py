"""gzip interoperability with Python/zlib; no third-party test dependencies."""
import argparse
import gzip
import json
from pathlib import Path
import struct
import subprocess
import zlib


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def check_process(process, succeeds):
    expected = 0 if succeeds else 1
    require(process.returncode == expected,
            ("unexpected codec exit", expected, process.returncode, process.stderr))
    require(not process.stdout, "file codec driver unexpectedly wrote stdout")
    require(not process.stderr if succeeds else bool(process.stderr),
            ("unexpected codec diagnostic", process.stderr))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    parser.add_argument("work", type=Path)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    driver = str(args.driver.resolve())
    source = args.work / "source.bin"
    packed = args.work / "input.gz"
    result = args.work / "result.bin"
    checks = []

    def run(mode, src, dst, succeeds=True):
        process = subprocess.run([driver, mode, str(src), str(dst)], capture_output=True, timeout=30)
        check_process(process, succeeds)

    payloads = [b"", b"a\0\xff", "你好，gzip".encode(), bytes(range(256)) * 517, b"a" * 150001]
    for index, data in enumerate(payloads):
        source.write_bytes(data)
        run("compress", source, packed)
        require(gzip.decompress(packed.read_bytes()) == data, "XRT/Python decompression mismatch")
        checks.append(f"xrt-to-python-{index}")
        for level in range(10):
            packed.write_bytes(gzip.compress(data, compresslevel=level, mtime=0))
            run("decompress", packed, result)
            require(result.read_bytes() == data, "Python/XRT decompression mismatch")
            checks.append(f"python-to-xrt-{index}-level-{level}")

    # RFC 1952 optional header fields + FHCRC, produced independently of XRT.
    data = b"optional fields and header crc"
    normal = gzip.compress(data, mtime=0)
    header = b"\x1f\x8b\x08\x1e" + b"\0" * 4 + b"\0\xff"
    header += struct.pack("<H", 4) + b"abcd" + b"sample.txt\0" + b"comment\0"
    header += struct.pack("<H", zlib.crc32(header) & 0xffff)
    optional = header + normal[10:]
    packed.write_bytes(optional)
    run("decompress", packed, result)
    require(result.read_bytes() == data, "optional-header decompression mismatch")
    checks.append("optional-fields-and-fhcrc")
    packed.write_bytes(optional[:-8] + bytes([optional[-8] ^ 1]) + optional[-7:])
    run("decompress", packed, result, False)
    checks.append("bad-crc")
    damaged = bytearray(optional)
    damaged[len(header) - 1] ^= 1
    packed.write_bytes(damaged)
    run("decompress", packed, result, False)
    checks.append("bad-header-crc")

    for cut in range(len(optional)):
        packed.write_bytes(optional[:cut])
        run("decompress", packed, result, False)
        checks.append(f"optional-truncated-{cut}")
    joined = b"".join(gzip.compress(data, mtime=0) for data in payloads)
    packed.write_bytes(joined)
    run("decompress", packed, result)
    require(result.read_bytes() == b"".join(payloads), "concatenated-member decompression mismatch")
    checks.append("mixed-concatenated-members")
    for suffix in (b"junk", b"\x1f", gzip.compress(b"unfinished", mtime=0)[:-1]):
        packed.write_bytes(joined + suffix)
        run("decompress", packed, result, False)
        checks.append("invalid-final-member-" + suffix[:4].hex())
    report = {"passed": len(checks), "checks": checks, "zlib": zlib.ZLIB_RUNTIME_VERSION}
    (args.work / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"gzip independent interoperability: {len(checks)} checks passed")


if __name__ == "__main__":
    main()
