"""核对三种数学定义；缩放像素来自被测依赖，不声称独立认证缩放器。"""
import json
import math
from pathlib import Path
import sys


def references(pixels):
    average, difference, perceptual = pixels
    total = sum(average)
    a = sum(1 << i for i, value in enumerate(average) if value * 64 >= total)
    d = sum(1 << (y * 8 + x) for y in range(8) for x in range(8)
            if difference[y * 9 + x] > difference[y * 9 + x + 1])
    # 直接二维定义，不沿用 C 的分离式中间结果或排序代码。
    def quantize(value):
        return int(math.copysign(math.floor(abs(value) + 0.5), value))

    basis = [[quantize(math.cos((2 * x + 1) * u * math.pi / 64) * 65536)
              for x in range(32)] for u in range(8)]
    coefficients = [sum(perceptual[y * 32 + x] * basis[u][x] * basis[v][y]
                        for y in range(32) for x in range(32))
                    for v in range(8) for u in range(8)][1:]
    median = sorted(coefficients)[31]
    p = sum(1 << i for i, value in enumerate(coefficients) if value > median)
    return [f"{value:016x}" for value in (a, d, p)]


def verify(rows):
    assert len(rows) == 20
    previous = 0
    for index, row in enumerate(rows):
        assert (row["size"], row["pattern"]) == divmod(index, 5)
        data = [bytes.fromhex(value) for value in row["pixels"]]
        assert [len(value) for value in data] == [64, 72, 1024]
        size, pattern = divmod(index, 5)
        width, height = [(8, 8), (9, 8), (32, 32), (43, 17)][size]
        source = bytes((0 if pattern == 0 else 255 if pattern == 1 else
                        x * 7 + y * 9 + x * y % 17 if pattern == 2 else
                        255 * ((x + y) % 2) if pattern == 3 else
                        x * 73 + y * 151 + x * y * 19 + size * 47) % 256
                       for y in range(height) for x in range(width))
        if size < 3:
            assert data[size] == source, "identity resize changed the PGM samples"
        if pattern < 2:
            assert all(value == bytes([pattern * 255]) * len(value) for value in data)
        assert row["hashes"] == references(data), f"hash mismatch at {index}"
        current = int(row["hashes"][2], 16)
        assert row["distance"] == (current ^ previous).bit_count()
        assert current < 1 << 63
        previous = current


def main():
    rows = [json.loads(line) for line in Path(sys.argv[1]).read_text(encoding="utf-8-sig").splitlines()]
    verify(rows)
    # 验证参照确实会抓到输出位翻转，而不是只检查记录数量。
    for method in range(3):
        altered = json.loads(json.dumps(rows))
        value = altered[7]["hashes"][method]
        altered[7]["hashes"][method] = f"{int(value, 16) ^ 1:016x}"
        try:
            verify(altered)
        except AssertionError:
            continue
        raise AssertionError("oracle accepted a corrupted hash")
    print("imagehash independent integer definitions: 20 images, 60 hashes, 20 distances, 3 controls PASS")


if __name__ == "__main__":
    main()
