"""通过操作系统独立安装的 liblz4 验证扩展库的标准 Frame 互通。"""
import argparse
import ctypes as c
import ctypes.util
import hashlib
import itertools
import json
import os
from pathlib import Path
import random
import struct
import subprocess


class Frame(c.Structure):
    _fields_ = [("block_size", c.c_int), ("block_mode", c.c_int),
                ("content_checksum", c.c_int), ("frame_type", c.c_int),
                ("content_size", c.c_ulonglong), ("dictionary", c.c_uint),
                ("block_checksum", c.c_int)]


class Preferences(c.Structure):
    _fields_ = [("frame", Frame), ("level", c.c_int), ("auto_flush", c.c_uint),
                ("favor_decode", c.c_uint), ("reserved", c.c_uint * 3)]


def bind(library, name, result, *arguments):
    function = getattr(library, name)
    function.restype, function.argtypes = result, arguments
    return function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    parser.add_argument("work", type=Path)
    parser.add_argument("--library", default=ctypes.util.find_library("lz4"))
    args = parser.parse_args()
    if not args.library:
        raise RuntimeError("An independently installed liblz4 is required")
    library = c.CDLL(args.library)
    version = bind(library, "LZ4_versionString", c.c_char_p)().decode()
    size, pointer = c.c_size_t, c.c_void_p
    bound = bind(library, "LZ4F_compressFrameBound", size, size, c.POINTER(Preferences))
    compress = bind(library, "LZ4F_compressFrame", size,
                    pointer, size, pointer, size, c.POINTER(Preferences))
    create = bind(library, "LZ4F_createDecompressionContext", size, c.POINTER(pointer), c.c_uint)
    release = bind(library, "LZ4F_freeDecompressionContext", size, pointer)
    decompress = bind(library, "LZ4F_decompress", size,
                      pointer, pointer, c.POINTER(size), pointer, c.POINTER(size), pointer)
    is_error = bind(library, "LZ4F_isError", c.c_uint, size)
    error_name = bind(library, "LZ4F_getErrorName", c.c_char_p, size)

    def checked(result):
        if is_error(result):
            raise AssertionError(error_name(result).decode())
        return result

    def reference_compress(data, prefs):
        source = c.create_string_buffer(data)
        target = c.create_string_buffer(checked(bound(len(data), c.byref(prefs))))
        n = checked(compress(target, len(target), source, len(data), c.byref(prefs)))
        return target.raw[:n]

    def reference_decompress(wire):
        context, offset, output = pointer(), 0, bytearray()
        checked(create(c.byref(context), 100))
        source = c.create_string_buffer(wire)
        target = c.create_string_buffer(65536)
        try:
            while True:
                consumed, produced = size(len(wire) - offset), size(len(target))
                result = checked(decompress(context, target, c.byref(produced),
                                           c.byref(source, offset), c.byref(consumed), None))
                offset += consumed.value
                output.extend(target.raw[:produced.value])
                if result == 0 and offset == len(wire):
                    return bytes(output)
                if consumed.value == produced.value == 0:
                    raise AssertionError("reference decoder made no progress")
        finally:
            release(context)

    args.work.mkdir(parents=True, exist_ok=True)
    source, packed, result = (args.work / n for n in ("source.bin", "input.lz4", "result.bin"))
    driver = args.driver.resolve()
    windows_driver = os.name != "nt" and driver.suffix.lower() == ".exe"
    # WSL 的参考库也可验 Windows 可执行程序；只转换被测程序接收的文件参数。
    def filename(path):
        value = str(path.resolve())
        if windows_driver:
            value = subprocess.check_output(["wslpath", "-w", value], text=True).strip()
        return value
    paths = {p: filename(p) for p in (source, packed, result)}
    checks = []

    def run(name, mode, src, dst, acceleration=1, success=True):
        process = subprocess.run([str(driver), mode, paths[src], paths[dst], str(acceleration)],
                                 capture_output=True, timeout=30)
        if (process.returncode == 0) != success:
            raise AssertionError((name, process.returncode, process.stderr.decode(errors="replace")))
        checks.append(name)

    payloads = [b"", b"a\0\xff", "你好，LZ4".encode(), bytes(range(256)) * 517,
                b"a" * 4194317, random.Random(817).randbytes(1100003)]
    for index, data in enumerate(payloads):
        source.write_bytes(data)
        for acceleration in (1, 8, 65537):
            run(f"package-to-system-{index}-{acceleration}", "compress", source, packed, acceleration)
            wire = packed.read_bytes()
            assert wire[:4] == b"\x04\x22\x4d\x18"
            assert reference_decompress(wire) == data
        # 4 种块大小、关联/独立块、两个校验开关、可选内容长度。
        modes = itertools.product(range(4, 8), (0, 1), (0, 1), (0, 1), (0, 1))
        for block, linked, block_sum, content_sum, known in modes:
            prefs = Preferences()
            prefs.frame.block_size = block
            prefs.frame.block_mode = linked
            prefs.frame.block_checksum = block_sum
            prefs.frame.content_checksum = content_sum
            prefs.frame.content_size = len(data) if known else 0
            packed.write_bytes(reference_compress(data, prefs))
            run(f"system-to-package-{index}-{block}-{linked}-{block_sum}-{content_sum}-{known}",
                "decompress", packed, result)
            assert result.read_bytes() == data

    prefs = Preferences()
    prefs.frame.content_checksum = prefs.frame.block_checksum = 1
    normal = reference_compress(b"checksum and truncation", prefs)
    for cut in range(len(normal)):
        packed.write_bytes(normal[:cut])
        run(f"truncated-{cut}", "decompress", packed, result, success=False)
    for index in range(len(normal)):
        bad = bytearray(normal)
        bad[index] ^= 1
        packed.write_bytes(bad)
        run(f"corrupted-{index}", "decompress", packed, result, success=False)

    frames = [reference_compress(p, prefs) for p in payloads[:4]]
    for variant in range(16):
        skip = struct.pack("<II", 0x184D2A50 + variant, variant) + b"z" * variant
        packed.write_bytes(skip + skip.join(frames) + skip)
        run(f"concatenated-skippable-{variant}", "decompress", packed, result)
        assert result.read_bytes() == b"".join(payloads[:4])
    for suffix in (b"junk", b"\x04", normal[:-1]):
        packed.write_bytes(normal + suffix)
        run(f"invalid-trailing-{len(suffix)}", "decompress", packed, result, success=False)
    prefs.frame.dictionary = 42
    packed.write_bytes(reference_compress(b"dictionary id requires an external dictionary", prefs))
    run("unsupported-dictionary", "decompress", packed, result, success=False)
    reference = Path(args.library)
    report = {"reference": args.library, "version": version,
              "referenceSha256": hashlib.sha256(reference.read_bytes()).hexdigest() if reference.is_file() else None,
              "driverSha256": hashlib.sha256(driver.read_bytes()).hexdigest(),
              "passed": len(checks), "checks": checks}
    (args.work / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"lz4 system-library interoperability: {len(checks)} checks passed (liblz4 {version})")


if __name__ == "__main__":
    main()
