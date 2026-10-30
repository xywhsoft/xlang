"""标准 zlib 双向互通；固定样本/参数，不用包内编码器自证解码器。"""
import random
import subprocess
import sys
import zlib


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def execute(tool, mode, data, *args):
    return subprocess.run([tool, mode, *map(str, args)], input=data,
                          capture_output=True, timeout=30, check=False)


def accepted(tool, mode, data, *args):
    result = execute(tool, mode, data, *args)
    require(result.returncode == 0 and not result.stderr,
            f'{mode}: exit={result.returncode}, {result.stderr!r}')
    return result.stdout


def rejected(tool, data):
    result = execute(tool, 'decode', data)
    diagnostic = result.stderr.rstrip(b'\r\n')
    require(result.returncode == 1 and not result.stdout and diagnostic.startswith(b'miniz: ') and
            diagnostic.endswith((b'(code=3)', b'(code=4)')),
            f'invalid zlib: exit={result.returncode}, partial={len(result.stdout)}, {result.stderr!r}')


def packed(data, **options):
    codec = zlib.compressobj(**options)
    return codec.compress(data) + codec.flush()


def main(tool):
    samples = [b'', b'hello zlib', '中文\0zlib'.encode(), bytes(range(256)),
               b'abc\0' * 60000, random.Random(729).randbytes(65537), b'A' * 65537]
    encode_count = decode_count = reject_count = 0
    for plain in samples:
        for level in range(11):
            native = accepted(tool, 'encode', plain, level)
            codec = zlib.decompressobj()
            result = codec.decompress(native) + codec.flush()
            require(result == plain and codec.eof and not codec.unused_data,
                    f'level {level}: native output differs or has trailing bytes')
            encode_count += 1
        # 覆盖存储、固定/动态 Huffman、RLE/Huffman-only 和不同窗口大小。
        variants = [dict(level=0), dict(level=1), dict(level=6), dict(level=9),
                    dict(level=6, strategy=zlib.Z_FIXED), dict(level=6, strategy=zlib.Z_RLE),
                    dict(level=6, strategy=zlib.Z_HUFFMAN_ONLY), dict(level=6, wbits=9)]
        for options in variants:
            external = packed(plain, **options)
            require(accepted(tool, 'decode', external) == plain, f'external options {options}')
            decode_count += 1
        external = zlib.compress(plain)
        bad_check = external[:-1] + bytes([external[-1] ^ 1])
        bad_header = bytes([external[0] ^ 1]) + external[1:]
        for invalid in (b'', external[:1], external[:-1], bad_check, bad_header,
                        external + b'junk', external + zlib.compress(b'next'),
                        packed(plain, wbits=-15), packed(plain, wbits=31),
                        packed(plain, zdict=b'hello dictionary')):
            rejected(tool, invalid)
            reject_count += 1
    print(f'miniz zlib interop: {encode_count} native encodes, {decode_count} external decodes, {reject_count} rejections')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: interop.py executable')
    main(sys.argv[1])
