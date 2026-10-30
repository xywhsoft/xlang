"""独立验证标准 .xz 文件格式的双向互通，不依赖包内解码器自证。"""
import lzma
import random
import subprocess
import sys


def require(condition, message):
    # 独立校验不能被 python -O 删除。
    if not condition:
        raise AssertionError(message)


def execute(executable, mode, data):
    return subprocess.run([executable, mode], input=data, capture_output=True,
                          timeout=30, check=False)


def call(executable, mode, data):
    result = execute(executable, mode, data)
    require(result.returncode == 0 and not result.stderr,
            f"{mode}: exit={result.returncode}: {result.stderr.decode(errors='replace')}")
    return result.stdout


def reject(executable, data):
    result = execute(executable, "decode", data)
    require(result.returncode == 1 and result.stdout == b"" and
            result.stderr.startswith(b"lzma: "),
            f"invalid XZ: exit={result.returncode}, stdout={len(result.stdout)}: {result.stderr!r}")


def corrupt_block_check(stream):
    # XZ footer 的 backward size 指向 Index，块校验字段紧贴 Index 之前。
    index_size = (int.from_bytes(stream[-8:-4], "little") + 1) * 4
    offset = len(stream) - 12 - index_size - 1
    require(offset >= 12, "XZ check offset outside block")
    damaged = bytearray(stream)
    damaged[offset] ^= 1
    return bytes(damaged)


def main(executable):
    samples = [b"", b"hello xlang", bytes(range(256)), b"abc\0" * 30000,
               random.Random(12345).randbytes(300000)]
    for plain in samples:
        native = call(executable, "encode", plain)
        require(lzma.decompress(native, format=lzma.FORMAT_XZ) == plain, "native encode differs")
        external = lzma.compress(plain, format=lzma.FORMAT_XZ)
        require(call(executable, "decode", external) == plain, "external XZ decode differs")
        crc_broken = bytearray(external)
        crc_broken[-12] ^= 1
        old_format = lzma.compress(plain, format=lzma.FORMAT_ALONE)
        for invalid in (external[:-1], external + b"\0", external + b"junk",
                        b"\xff" + external[1:],
                        bytes(crc_broken), old_format):
            reject(executable, invalid)
        joined = external + b"\0" * 4 + lzma.compress(b"next", format=lzma.FORMAT_XZ)
        require(call(executable, "decode", joined) == plain + b"next", "padded members differ")
        require(call(executable, "decode", external + b"\0" * 4) == plain, "padded tail differs")

    # 读取其他标准 .xz 检验方式与前置滤镜，避免只验证本包默认编码器。
    filtered = bytes(range(256)) * 32
    variants = {
        "crc32": {"check": lzma.CHECK_CRC32},
        "sha256": {"check": lzma.CHECK_SHA256},
        "no-check": {"check": lzma.CHECK_NONE},
        "delta": {"filters": [
            {"id": lzma.FILTER_DELTA, "dist": 1},
            {"id": lzma.FILTER_LZMA2},
        ]},
        "x86": {"filters": [
            {"id": lzma.FILTER_X86},
            {"id": lzma.FILTER_LZMA2},
        ]},
    }
    for name, options in variants.items():
        external = lzma.compress(filtered, format=lzma.FORMAT_XZ, **options)
        require(call(executable, "decode", external) == filtered, name)
        if name != "no-check":
            damaged = corrupt_block_check(external)
            try:
                lzma.decompress(damaged, format=lzma.FORMAT_XZ)
            except lzma.LZMAError:
                pass
            else:
                raise AssertionError(f"{name}: independent decoder accepted bad check")
            reject(executable, damaged)

    # 串联流各自声明校验算法；第二条损坏时不得交出第一条的部分结果。
    first = lzma.compress(b"first", format=lzma.FORMAT_XZ,
                          check=lzma.CHECK_CRC32)
    second = lzma.compress(b"second", format=lzma.FORMAT_XZ,
                           check=lzma.CHECK_SHA256)
    # Python 标准库只继续读取相邻流；流间填充由包内 C 边界单独验证。
    joined = first + second
    require(lzma.decompress(joined, format=lzma.FORMAT_XZ) == b"firstsecond", "external members differ")
    require(call(executable, "decode", joined) == b"firstsecond", "native members differ")
    damaged_second = corrupt_block_check(second)
    damaged = first + damaged_second
    try:
        # lzma.decompress 对损坏的尾随流可能只返回首流，不能作整段拒绝判据。
        lzma.decompress(damaged_second, format=lzma.FORMAT_XZ)
    except lzma.LZMAError:
        pass
    else:
        raise AssertionError("independent decoder accepted the bad second stream")
    reject(executable, damaged)
    print("lzma standard .xz interop: PASS")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: interop.py executable")
    main(sys.argv[1])
