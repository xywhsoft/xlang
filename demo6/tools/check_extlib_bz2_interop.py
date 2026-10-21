"""Check the package wrapper independently through Python's bz2 interface."""
import argparse
import bz2
import json
from pathlib import Path
import random
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    parser.add_argument("work", type=Path)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    driver = str(args.driver.resolve())
    source, packed, result = (args.work / name for name in ("source.bin", "input.bz2", "result.bin"))
    checks = []

    def run(mode, src, dst, option, succeeds=True):
        process = subprocess.run([driver, mode, str(src), str(dst), str(option)],
                                 capture_output=True, timeout=30)
        if (process.returncode == 0) != succeeds:
            raise AssertionError((mode, option, process.returncode,
                                  process.stderr.decode(errors="replace")))

    payloads = [b"", b"a\0\xff", "你好，bz2".encode(), bytes(range(256)) * 517,
                b"a" * 200003, random.Random(817).randbytes(1100003)]
    for index, data in enumerate(payloads):
        source.write_bytes(data)
        levels = (1, 9) if index == 5 else range(1, 10)
        for level in levels:
            run("compress", source, packed, level)
            assert bz2.decompress(packed.read_bytes()) == data
            checks.append(f"c-to-python-{index}-level-{level}")
            packed.write_bytes(bz2.compress(data, compresslevel=level))
            for small in (0, 1):
                run("decompress", packed, result, small)
                assert result.read_bytes() == data
                checks.append(f"python-to-c-{index}-level-{level}-small-{small}")

    # 小流逐字节截断；块 CRC、文件头和后续成员的失败不能被首成员成功掩盖。
    normal = bz2.compress(b"checksum and truncation")
    for cut in range(len(normal)):
        packed.write_bytes(normal[:cut])
        for small in (0, 1):
            run("decompress", packed, result, small, False)
            checks.append(f"truncated-{cut}-small-{small}")
    damaged = bytearray(normal)
    damaged[10] ^= 1
    for name, wire in (("crc", damaged), ("magic", b"bad!" + normal[4:]),
                       ("level", normal[:3] + b"0" + normal[4:])):
        packed.write_bytes(wire)
        for small in (0, 1):
            run("decompress", packed, result, small, False)
            checks.append(f"invalid-{name}-small-{small}")
    joined = b"".join(bz2.compress(p) for p in payloads[:5])
    for small in (0, 1):
        packed.write_bytes(joined)
        run("decompress", packed, result, small)
        assert result.read_bytes() == b"".join(payloads[:5])
        checks.append(f"concatenated-small-{small}")
        # 本包拒绝尾部垃圾，不能套用 Python 忽略部分非 bzip2 尾部的宽松行为。
        for index, suffix in enumerate((b"junk", b"B", normal[:-1])):
            packed.write_bytes(joined + suffix)
            run("decompress", packed, result, small, False)
            checks.append(f"invalid-final-member-{index}-small-{small}")
    report = {"passed": len(checks), "checks": checks}
    (args.work / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"bz2 independent interoperability: {len(checks)} checks passed")


if __name__ == "__main__":
    main()
